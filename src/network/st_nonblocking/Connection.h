#ifndef AFINA_NETWORK_ST_NONBLOCKING_CONNECTION_H
#define AFINA_NETWORK_ST_NONBLOCKING_CONNECTION_H

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
namespace STnonblock {

class Connection {
public:
    Connection(int s,
               std::shared_ptr<spdlog::logger> &logger,
               std::shared_ptr<Afina::Storage> ps) : _socket(s),
                                                     _logger(logger),
                                                     pStorage(ps) {
        std::memset(&_event, 0, sizeof(struct epoll_event));
        _event.data.ptr = this;
    }

    inline bool isAlive() const { return _active; }

    void Start();

protected:
    void OnError();
    void OnClose();
    void Close();
    void DoRead();
    void DoWrite();

private:
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

    bool _active;

    std::shared_ptr<Afina::Storage> pStorage;

    size_t MAX = 128;
};

} // namespace STnonblock
} // namespace Network
} // namespace Afina

#endif // AFINA_NETWORK_ST_NONBLOCKING_CONNECTION_H