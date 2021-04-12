#include "Connection.h"

#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace Afina {
namespace Network {
namespace MTnonblock {

// See Connection.h
void Connection::Start() {
    _write_only = false;
    _active.store(true, std::memory_order_relaxed);
    _parsed = 0;
    _shift = 0;
    _event.events = EPOLLIN | EPOLLRDHUP | EPOLLERR;
}

// See Connection.h
void Connection::OnError() {
    _event.events = 0;
    _active.store(false, std::memory_order_relaxed);
}

// See Connection.h
void Connection::OnClose() {
    _event.events = 0;
    _active.store(false, std::memory_order_relaxed);
}

// See Connection.h
void Connection::DoRead() {
    std::atomic_thread_fence(std::memory_order_acquire);
    std::string argument_for_command;
    std::unique_ptr<Execute::Command> command_to_execute;
    std::size_t arg_remains = 0;
    int readed_bytes = -1;
    try {
        if ((readed_bytes = read(_socket, client_buffer + _parsed, SIZE - _parsed)) > 0) {
            _logger->debug("Got {} bytes from socket", readed_bytes);
            readed_bytes += _parsed;
            _parsed = 0;
            while (readed_bytes > 0) {
                // There is no command yet
                if (!command_to_execute) {
                    std::size_t parsed = 0;
                    if (parser.Parse(client_buffer + _parsed, readed_bytes, parsed)) {
                        // Here we are, current chunk finished some command, process it
                        command_to_execute = parser.Build(arg_remains);
                        if (arg_remains > 0) {
                            arg_remains += 2;
                        }
                    }

                    // Parsed might fails to consume any bytes from input stream (UTF-16 chars and only 1 byte left)
                    if (parsed == 0) {
                        _parsed = readed_bytes;
                        std::memmove(client_buffer, client_buffer + _parsed, _parsed);
                        break;
                    } else {
                        readed_bytes -= parsed;
                        _parsed += parsed;
                    }
                }
                // There is command, but we still wait for argument to arrive...
                if (command_to_execute && arg_remains > 0) {
                    // There is some parsed command, and now we are reading argument
                    _logger->debug("Fill argument: {} bytes of {}", readed_bytes, arg_remains);
                    std::size_t to_read = std::min(arg_remains, std::size_t(readed_bytes));
                    argument_for_command.append(client_buffer + _parsed, to_read);

                    arg_remains -= to_read;
                    _parsed += to_read;
                }
                // There are command & argument - RUN!
                if (command_to_execute && arg_remains == 0) {
                    _logger->debug("Start command execution");

                    std::string result;
                    if (argument_for_command.size()) {
                        argument_for_command.resize(argument_for_command.size() - 2);
                    }
                    command_to_execute->Execute(*pStorage, argument_for_command, result);

                    // Put response in the queue
                    result += "\r\n";
                    responses.push_back(std::move(result));
                    if (responses.size() > MAX) {
                        _event.events &= ~EPOLLIN;
                    }
                    if (!(_event.events & EPOLLOUT)) {
                        _event.events |= EPOLLOUT;
                    }

                    // Prepare for the next command
                    command_to_execute.reset();
                    argument_for_command.resize(0);
                    parser.Reset();
                }
            }
            if (readed_bytes == 0) {
                _parsed = 0;
            }
        } else if (readed_bytes == 0) {
            _write_only = true;
            _parsed = 0;
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            throw std::runtime_error(std::string(strerror(errno)));
        }
    } catch (std::runtime_error &ex) {
        _logger->error("Failed to process connection on descriptor {}: {}", _socket, ex.what());
        _write_only = true;
        responses.push_back("ERROR\r\n");
        if (!(_event.events & EPOLLOUT)) {
            _event.events |= EPOLLOUT;
        }
    }
    std::atomic_thread_fence(std::memory_order_release);
}

// See Connection.h
void Connection::DoWrite() {
    std::atomic_thread_fence(std::memory_order_acquire);
    size_t write_vec_size = 64;
    iovec write_vec[write_vec_size];
    size_t write_vec_v = 0;
    {
        auto it = responses.begin();
        write_vec[write_vec_v].iov_base = &((*it)[0]) + _shift;
        write_vec[write_vec_v].iov_len = it->size() - _shift;
        it++;
        write_vec_v++;
        for (; it != responses.end(); it++) {
            write_vec[write_vec_v].iov_base = &((*it)[0]);
            write_vec[write_vec_v].iov_len = it->size();
            if (++write_vec_v >= write_vec_size) {
                break;
            }
        }
    }

    int writed = 0;
    if ((writed = writev(_socket, write_vec, write_vec_v)) > 0) {
        size_t i = 0;
        while (i < write_vec_v && writed >= write_vec[i].iov_len) {
            responses.pop_front();
            writed -= write_vec[i].iov_len;
            i++;
        }
        _shift = writed;
    } else if (writed < 0 && writed != EAGAIN) {
        _active.store(false, std::memory_order_relaxed);
    }
    if (responses.empty()) {
        _event.events &= ~EPOLLOUT;
    }
    if (responses.size() <= MAX){
        _event.events |= EPOLLIN;
    }
    if (_write_only && responses.empty()) {
        close(_socket);
    }
    std::atomic_thread_fence(std::memory_order_release);
}

} // namespace MTnonblock
} // namespace Network
} // namespace Afina
