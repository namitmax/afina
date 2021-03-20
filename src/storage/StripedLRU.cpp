#include "StripedLRU.h"

namespace Afina {
namespace Backend {

std::unique_ptr<StripedLRU> StripedLRU::CreateStorage(const size_t max_size, const size_t stripe_count) {
    size_t capacity = 0;
    if (stripe_count != 0) {
        capacity = max_size / stripe_count;
    } else {
        throw std::runtime_error("Number of stripes is equal to zero!!!!");
    }
    if (capacity < 1 * 1024 * 1024UL) {
        throw std::runtime_error("There is no reason to use so big number "
                                 "of stripes, because size of each of them is too small!!!!");
    }
    return std::unique_ptr<StripedLRU>(new StripedLRU(max_size, stripe_count));
};

bool StripedLRU::Put(const std::string &key, const std::string &value) {
    std::lock_guard<std::mutex> _lock(_mutex_for_shard[hash(key) % _stripe_count]);
    return _shard[hash(key) % _stripe_count].Put(key, value);
}

bool StripedLRU::PutIfAbsent(const std::string &key, const std::string &value) {
    std::lock_guard<std::mutex> _lock(_mutex_for_shard[hash(key) % _stripe_count]);
    return _shard[hash(key) % _stripe_count].PutIfAbsent(key, value);
}

bool StripedLRU::Set(const std::string &key, const std::string &value) {
    std::lock_guard<std::mutex> _lock(_mutex_for_shard[hash(key) % _stripe_count]);
    return _shard[hash(key) % _stripe_count].Set(key, value);
}

bool StripedLRU::Delete(const std::string &key) {
    std::lock_guard<std::mutex> _lock(_mutex_for_shard[hash(key) % _stripe_count]);
    return _shard[hash(key) % _stripe_count].Delete(key);
}

bool StripedLRU::Get(const std::string &key, std::string &value) {
    std::lock_guard<std::mutex> _lock(_mutex_for_shard[hash(key) % _stripe_count]);
    return _shard[hash(key) % _stripe_count].Get(key, value);
}

StripedLRU::StripedLRU(size_t max_size,
                       size_t stripe_count):  _stripe_count(stripe_count),
                                              _capacity(max_size / _stripe_count),
                                              _mutex_for_shard(stripe_count) {
    for (size_t i = 0; i < _stripe_count; i++) {
        _shard.emplace_back(SimpleLRU(_capacity));
    }
};

}
}
