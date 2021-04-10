#ifndef AFINA_NETWORK_MT_NONBLOCKING_CONNECTION_H
#define AFINA_NETWORK_MT_NONBLOCKING_CONNECTION_H

#include <cstring>
#include <deque>
#include <memory>

#include <sys/epoll.h>
#include <sys/uio.h>

#include <afina/Storage.h>
#include <afina/execute/Command.h>
#include <spdlog/logger.h>

#include "protocol/Parser.h"

namespace Afina {
namespace Network {
namespace MTnonblock {

class Connection {
public:
    Connection(int s, std::shared_ptr<Afina::Storage> pStorage_, std::shared_ptr<spdlog::logger> logger) :_socket(s),
                                                                                                           pStorage(pStorage_),
                                                                                                            _logger(logger){
        std::memset(&_event, 0, sizeof(struct epoll_event));
        _event.data.ptr = this;
    }

    inline bool isAlive() const { return true; }

    void Start();

protected:
    void OnError();
    void OnClose();
    void Close();
    void DoRead();
    void DoWrite();

private:
    friend class Worker;
    friend class ServerImpl;

    int _socket;
    struct epoll_event _event;

    char client_buffer[4096] = "";
    const size_t SIZE = 4096;

    int _parsed;
    Protocol::Parser parser;

    std::shared_ptr<spdlog::logger> _logger;

    std::deque<std::string> responses;
    size_t _shift;

    std::atomic<bool>  _active;

    std::shared_ptr<Afina::Storage> pStorage;

    size_t MAX = 128;

};

} // namespace MTnonblock
} // namespace Network
} // namespace Afina

#endif // AFINA_NETWORK_MT_NONBLOCKING_CONNECTION_H
