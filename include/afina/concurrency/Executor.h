#ifndef AFINA_CONCURRENCY_EXECUTOR_H
#define AFINA_CONCURRENCY_EXECUTOR_H

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <algorithm>
#include <iostream>

namespace Afina {
namespace Concurrency {

/**
 * # Thread pool
 */
class Executor {

public:
    enum class State {
        // Threadpool is fully operational, tasks could be added and get executed
        kRun,

        // Threadpool is on the way to be shutdown, no ned task could be added, but existing will be
        // completed as requested
        kStopping,

        // Threadppol is stopped
        kStopped
    };

    Executor (std::string name, int size,
             size_t max = 8, size_t min = 2,
             size_t time = 100) : _name(name),
                            max_queue_size(size),
                            low_watermark(min),
                            high_watermark(max),
                            _time(time) {
             std::unique_lock<std::mutex> lock(mutex);
             for (size_t  i = 0; i < low_watermark; ++i) {
                 threads.emplace_back(std::thread([this] {
                     return perform(this);
                 }));
             }
    };

    ~Executor() {
        Stop(true);
    };

    /**
     * Signal thread pool to stop, it will stop accepting new jobs and close threads just after each become
     * free. All enqueued jobs will be complete.
     *
     * In case if await flag is true, call won't return until all background jobs are done and all threads are stopped
     */
    void Stop(bool await = false) {
        state = State::kStopping;
        {
            std::unique_lock<std::mutex> condition_lock(mutex);
            empty_condition.notify_all();
        }
        for (size_t i = 0; i < threads.size(); ++i) {
            if (await) {
                threads[i].join();
            }
        }
        state = State::kStopped;
    }

    /**
     * Add function to be executed on the threadpool. Method returns true in case if task has been placed
     * onto execution queue, i.e scheduled for execution and false otherwise.
     *
     * That function doesn't wait for function result. Function could always be written in a way to notify caller about
     * execution finished by itself
     */
    template <typename F, typename... Types> bool Execute(F &&func, Types... args) {
        // Prepare "task"
        auto exec = std::bind(std::forward<F>(func), std::forward<Types>(args)...);

        if (state != State::kRun || _current_size >= max_queue_size) {
            return false;
        }

        // Enqueue new task
        {
            std::unique_lock<std::mutex> lock(mutex);
            if (threads.size() < high_watermark
                && _current_size++ > 0
                && threads.size() == _working) {
                threads.emplace_back(std::thread([this] {
                    return perform(this);
                }));
            }
        }
        std::unique_lock<std::mutex> lock(this->mutex);

        tasks.push_back(exec);
        empty_condition.notify_one();
        return true;
    }

private:
    // No copy/move/assign allowed
    Executor(const Executor &);            // = delete;
    Executor(Executor &&);                 // = delete;
    Executor &operator=(const Executor &); // = delete;
    Executor &operator=(Executor &&);      // = delete;

    /**
     * Main function that all pool threads are running. It polls internal task queue and execute tasks
     */
    friend void perform(Executor *executor) {
        auto now = std::chrono::system_clock::now();
        while (executor->state == Executor::State::kRun) {
            std::function<void ()> work_func;
            {
                std::unique_lock<std::mutex> condition_lock(executor->mutex);
                {
                    while (executor->tasks.empty()) {
                        auto wake = executor->empty_condition.wait_until(
                            condition_lock,
                            now + std::chrono::milliseconds(executor->_time));
                        if (executor->state != Executor::State::kRun ||
                                executor->tasks.empty() &&
                                executor->threads.size() > executor->low_watermark &&
                                wake == std::cv_status::timeout) {
                            if (executor->state != Executor::State::kStopping) {
                                std::unique_lock<std::mutex> lock(executor->_mutex);
                                auto id = std::this_thread::get_id();
                                auto find_it =
                                    std::find_if(executor->threads.begin(), executor->threads.end(),
                                                 [id](std::thread &thread) {
                                                     return thread.get_id() == id;
                                                 });
                                executor->threads.erase(find_it);
                            }
                            return;
                        }
                    }
                    work_func = std::move(executor->tasks.front());
                    executor->tasks.pop_front();
                    executor->_current_size--;
                    executor->_working++;
                }
            }
            try {
                work_func();
            } catch (...) {
                std::cout << "Some error  occurred during the  function execution";
            }
            {
                std::unique_lock<std::mutex> lock(executor->mutex);
                executor->_working--;
            }
        }
    };

    /**
     * Mutex to protect state below from concurrent modification
     */
    std::mutex mutex;

    /**
     * Conditional variable to await new data in case of empty queue
     */
    std::condition_variable empty_condition;

    /**
     * Vector of actual threads that perorm execution
     */
    std::vector<std::thread> threads;

    /**
     * Task queue
     */
    std::deque<std::function<void()>> tasks;

    /**
     * Flag to stop bg threads
     */
    State state;

    size_t low_watermark;
    size_t high_watermark;
    size_t max_queue_size;
    int _current_size = 0;
    int _working = 0;
    size_t _time;
    std::string _name;
    std::mutex _mutex;
};

} // namespace Concurrency
} // namespace Afina

#endif // AFINA_CONCURRENCY_EXECUTOR_H
