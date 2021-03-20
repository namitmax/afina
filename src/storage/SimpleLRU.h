#ifndef AFINA_STORAGE_SIMPLE_LRU_H
#define AFINA_STORAGE_SIMPLE_LRU_H

#include <map>
#include <memory>
#include <mutex>
#include <string>

#include <afina/Storage.h>

namespace Afina {
namespace Backend {

/**
 * # Map based implementation
 * That is NOT thread safe implementaiton!!
 */
class SimpleLRU : public Afina::Storage {
public:
    explicit SimpleLRU(size_t max_size = 1024) : _max_size(max_size),
                                        _current_size(0),
                                        _lru_tail(nullptr),
                                        _lru_index(),
                                        _lru_head() {}

    SimpleLRU(SimpleLRU&&) = default;

    ~SimpleLRU() override {
        _lru_index.clear();
        while (_lru_tail && _lru_tail != _lru_head.get()) {
            _lru_tail = _lru_tail->prev;
            _lru_tail->next.reset();
        }
        _lru_head.reset(); // TODO: Here is stack overflow \/
    }

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

private:

    // LRU cache node
    using lru_node = struct lru_node {
        const std::string key;
        std::string value;
        lru_node* prev;
        std::unique_ptr<lru_node> next;
    };

    // The function makes new LRU head.
    void MakeNewHead(lru_node &node);

    // The function moves the recently used element to LRU head.
    void MoveNodeToHead(lru_node &node);

    // The function delete the last element from LRU tail.
    void DeleteElementFromTail();

    // Put new value and key in LRU.
    void MakeKeyValue(const std::string &key,
                      const std::string &value);

    // This function changes the value of the given key.
    void ChangeKeyValue(lru_node &node,
                        const std::string &value);

    // Current number of bytes (keys+values)
    // that are stored in this cache.
    std::size_t _current_size = 0;

    // Maximum number of bytes could be stored in this cache.
    // i.e all (keys+values) must be less the _max_size
    std::size_t _max_size;

    // Main storage of lru_nodes, elements in this list ordered descending by "freshness": in the head
    // element that wasn't used for longest time.
    //
    // List owns all nodes
    std::unique_ptr<lru_node> _lru_head;

    lru_node *_lru_tail;

    // Index of nodes from list above, allows fast random access to elements by lru_node#key
    std::map<std::reference_wrapper<const std::string>,
             std::reference_wrapper<lru_node>, std::less<std::string>> _lru_index;
};

} // namespace Backend
} // namespace Afina

#endif // AFINA_STORAGE_SIMPLE_LRU_H
