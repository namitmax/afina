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
#include <unordered_map>

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
                 std::thread thread = std::thread([this] {
                     return perform(this);
                 });
                 threads.emplace(thread.get_id(), std::move(thread));
             }
             state = State::kRun;
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
        {
            std::unique_lock<std::mutex> _lock(mutex);
            state = State::kStopping;
        }
        empty_condition.notify_all();
        /*for (auto &thread : threads) {
            if (await && thread.second.joinable()) {
                thread.second.join();
            }
        }*/
        {
            std::unique_lock<std::mutex> _lock(mutex);
            if (await) {
                while (!threads.empty()){
                    stop_condition.wait(_lock);
                }
            }
            state = State::kStopped;
        }
        /*if (_working == 0) {
            state = State::kStopped;
        }*/
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
        std::unique_lock<std::mutex> lock(this->mutex);
        if (state != State::kRun || tasks.size() >= max_queue_size) {
            return false;
        }
        // Enqueue new task
        if (threads.size() < high_watermark
            && threads.size() == _working) {
            auto thread = std::thread([this] {
                return perform(this);
            });
            threads.emplace(thread.get_id(), std::move(thread));
        }

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
        std::unique_lock<std::mutex> lock(executor->_mutex);
        while (executor->state == Executor::State::kRun || !executor->tasks.empty()) {
            std::function<void()> work_func;
            while (executor->tasks.empty()) {
                auto now = std::chrono::system_clock::now();
                auto wake = executor->empty_condition.wait_until(
                            lock, now + std::chrono::milliseconds(executor->_time));
                if (executor->state != Executor::State::kRun ||
                    executor->tasks.empty() && executor->threads.size() > executor->low_watermark &&
                        wake == std::cv_status::timeout) {
                    if (executor->state != Executor::State::kStopping) {
                        auto id = std::this_thread::get_id();
                        executor->threads.at(id).detach();
                        executor->threads.erase(id);
                    }
                    return;
                }
                if (executor->state != Executor::State::kRun) {
                    auto id = std::this_thread::get_id();
                    executor->threads.at(id).detach();
                    executor->threads.erase(id);
                    if (executor->threads.empty()) {
                        executor->stop_condition.notify_all();
                    }
                    return;
                }
            }
            work_func = std::move(executor->tasks.front());
            executor->tasks.pop_front();
            executor->_working++;
            lock.unlock();
            try {
                work_func();
            } catch (const std::exception &err) {
                std::cout << "Some error  occurred during the  function execution, what : " << err.what() << std::endl;
            } catch (...) {
                std::cout << "Some error  occurred during the  function execution, there is no log"  << std::endl;
            }
            lock.lock();
            executor->_working--;
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

    std::condition_variable stop_condition;
    /**
     * Vector of actual threads that perorm execution
     */
    std::unordered_map<std::thread::id, std::thread> threads;
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
    int _working = 0;
    size_t _time;
    std::string _name;
    std::mutex _mutex;
};

} // namespace Concurrency
} // namespace Afina

#endif // AFINA_CONCURRENCY_EXECUTOR_H
