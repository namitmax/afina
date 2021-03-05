//
// Created by namitmax on 03/03/2021.
//

#ifndef AFINA_STORAGE_TRIPED_LRU_H
#define AFINA_STORAGE_TRIPED_LRU_H

#include "ThreadSafeSimpleLRU.h"

namespace Afina {
namespace Backend {

class StripedLRU: public Afina::Storage {
public:

    static StripedLRU* CreateStorage(const size_t max_size, const size_t stripe_count);

    // Implements Afina::Storage interface
    bool Put(const std::string &key, const std::string &value) override;

    // Implements Afina::Storage interface
    bool PutIfAbsent(const std::string &key, const std::string &value) override;

    // Implements Afina::Storage interface
    bool Set(const std::string &key, const std::string &value) override;

    // Implements Afina::Storage interface
    bool Delete(const std::string &key) override;

    // Implements Afina::Storage interface
    bool Get(const std::string &key, std::string &value) override;

    ~StripedLRU();

private:
    StripedLRU(size_t max_size, size_t stripe_count);

    std::size_t _capacity = 0;
    size_t _stripe_count = 0;
    std::hash<std::string> hash;
    ThreadSafeSimpleLRU* *_shard = 0;
    // hash
};
} // namespace Backend
} // namespace Afina

#endif // AFINA_STORAGE_TRIPED_LRU_H
