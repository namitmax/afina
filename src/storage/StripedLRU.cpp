#include "StripedLRU.h"

namespace Afina {
namespace Backend {

StripedLRU* StripedLRU::CreateStorage(const size_t max_size = 1024, const size_t stripe_count = 2) {
    size_t capacity = 0;
    if (stripe_count != 0) {
        capacity = max_size / stripe_count;
    } else {
        throw std::runtime_error("Number of stripes is equal to zero!!!!");
    }
    if (capacity < 128) {
        throw std::runtime_error("There is no reason to use so big number "
                                 "of stripes, because size of each of them is too small!!!!");
    }
    return (new StripedLRU(max_size, stripe_count));
};

bool StripedLRU::Put(const std::string &key, const std::string &value) {
    return _shard[hash(key) % _stripe_count]->Put(key, value);
}

bool StripedLRU::PutIfAbsent(const std::string &key, const std::string &value) {
    return _shard[hash(key) % _stripe_count]->PutIfAbsent(key, value);
}

bool StripedLRU::Set(const std::string &key, const std::string &value) {
    return _shard[hash(key) % _stripe_count]->Set(key, value);
}

bool StripedLRU::Delete(const std::string &key) {
    return _shard[hash(key) % _stripe_count]->Delete(key);
}

bool StripedLRU::Get(const std::string &key, std::string &value) {
    return _shard[hash(key) % _stripe_count]->Get(key, value);
}

StripedLRU::~StripedLRU() {
    for (size_t i = 0; i < _stripe_count; i++) {
        delete _shard[i];
    }
    delete [] _shard;
};

StripedLRU::StripedLRU(size_t max_size = 1024,
                       size_t stripe_count = 1) {
    _stripe_count = stripe_count;
    _capacity = max_size / _stripe_count;
    _shard = new ThreadSafeSimpleLRU *[_stripe_count];
    for (size_t i = 0; i < _stripe_count; i++) {
        _shard[i] = new ThreadSafeSimpleLRU(_capacity);
    }
};

}
}
