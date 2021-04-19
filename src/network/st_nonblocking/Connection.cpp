
#include "Connection.h"

#include <cassert>
#include <unistd.h>

#include <iostream>

namespace Afina {
namespace Network {
namespace STnonblock {

// See Connection.h
void Connection::Start() {
    _active = true;
    _parsed = 0;
    _shift = 0;
    _event.events = EPOLLIN | EPOLLRDHUP | EPOLLERR;
}

// See Connection.h
void Connection::OnError() {
    _active = false;
    _event.events = 0;
}

// See Connection.h
void Connection::OnClose() {
    _active = false;
    _event.events = 0;
}

// See Connection.h
void Connection::Close() {
    _active = false;
}

// See Connection.h
void Connection::DoRead() {
    int readed_bytes;
    try {
        if ((readed_bytes = read(_socket, client_buffer + _parsed, SIZE - _parsed)) > 0) {
            _logger->debug("Got {} bytes from socket", readed_bytes);
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
                    readed_bytes -= to_read;
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
            _parsed = 0;
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            throw std::runtime_error(std::string(strerror(errno)));
        }
    } catch (std::runtime_error &ex) {
        _logger->error("Failed to process connection on descriptor {}: {}", _socket, ex.what());
        responses.push_back("ERROR\r\n");
        if (!(_event.events & EPOLLOUT)) {
            _event.events |= EPOLLOUT;
        }
    }
}

// See Connection.h
void Connection::DoWrite() {
    const size_t write_vec_size = 64;
    iovec write_vec[write_vec_size] = {};
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
        _active = false;
    }
    if (responses.empty()) {
        _event.events &= ~EPOLLOUT;
    }
    if (responses.size() <= MAX){
        _event.events |= EPOLLIN;
    }
}

} // namespace STnonblock
} // namespace Network
} // namespace Afina