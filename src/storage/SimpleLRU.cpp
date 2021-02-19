#include "SimpleLRU.h"
#include <unistd.h>
namespace Afina {
namespace Backend {

void SimpleLRU::MakeNewHead(lru_node &node) {
    lru_node *nodePointer = &node;
    if (_lru_head) {
        _lru_head->prev = nodePointer;
    } else { // trivial case: LRU is empty
        _lru_tail = nodePointer;
    }
    nodePointer->next = std::move(_lru_head);
    _lru_head.reset(nodePointer);
}

void SimpleLRU::MoveNodeToHead(lru_node &node) {
    lru_node *nodePointer = &node;
    if (_lru_head.get() == nodePointer) { // The node is already the LRU head.
        return;
    }
    if (nodePointer->next) {
        auto temp = std::move(_lru_head);
        _lru_head = std::move(nodePointer->prev->next); // new place
        nodePointer->prev->next = std::move(nodePointer->next); // old place
        temp->prev = nodePointer;
        nodePointer->next = std::move(temp); // new place
        nodePointer->prev->next->prev = nodePointer->prev; // old place
    } else { // trivial case (1 element in list)
        _lru_tail = nodePointer->prev;
        _lru_head->prev = nodePointer;
        nodePointer->next = std::move(_lru_head);
        _lru_head = std::move(nodePointer->prev->next);
    }
}

void SimpleLRU::DeleteElementFromTail() {
    std::size_t deltaSize = _lru_tail->key.size() + _lru_tail->value.size();
    _lru_index.erase(_lru_tail->key);
    if (_lru_head.get() != _lru_tail) {
        _lru_tail = _lru_tail->prev;
        _lru_tail->next.reset(nullptr);
    } else { // trivial case: 1 element in LRU
        _lru_head.reset(nullptr);
    }
    _current_size -= (deltaSize);
}

void SimpleLRU::MakeKeyValue(const std::string &key,
                             const std::string &value) {
    while (_current_size + key.size() + value.size() > _max_size) {
        DeleteElementFromTail();
    }
    auto *node = new lru_node{key, value, nullptr, nullptr};
    MakeNewHead(*node);
    _lru_index.insert({std::reference_wrapper<const std::string>(node->key),
        std::reference_wrapper<lru_node>(*node)});
    _current_size += key.size() + value.size();
}

void SimpleLRU::ChangeKeyValue(lru_node &node,
                               const std::string &value) {
    MoveNodeToHead(node);
    if (value.size() > node.value.size()) {
        while (_current_size + value.size() - node.value.size() > _max_size) {
            DeleteElementFromTail();
        }
        _current_size += (value.size() - node.value.size());
    } else {
        _current_size -= (node.value.size() - value.size());
    }
    node.value = value;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Put(const std::string &key, const std::string &value) {
    if (key.size() + value.size() > _max_size) {
        return false;
    }
    auto node = _lru_index.find(key);
    if (node != _lru_index.end()) { // key exist
        ChangeKeyValue((node->second).get(), value);
    } else { // key doesn't exist
        MakeKeyValue(key, value);
    }
    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::PutIfAbsent(const std::string &key, const std::string &value) {
    if (key.size() + value.size() > _max_size) {
        return false;
    }
    if (_lru_index.find(key) == _lru_index.end()) {
        MakeKeyValue(key, value);
        return true;
    }
    return false;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Set(const std::string &key, const std::string &value) {
    if (key.size() + value.size() > _max_size) {
        return false;
    }
    auto node = _lru_index.find(key);
    if (node != _lru_index.end()) {
        ChangeKeyValue((node->second).get(), value);
        return true;
    }
    return false;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Delete(const std::string &key) {
    auto temp = _lru_index.find(key);
    if (temp == _lru_index.end()) {
        return false;
    }
    lru_node *nodePointer = &(temp->second.get());
    _lru_index.erase(temp);
    if (nodePointer != _lru_head.get()) {
        if (nodePointer->next) { // default case
            nodePointer->next->prev = nodePointer->prev;
            nodePointer->prev->next = std::move(nodePointer->next);
        } else { // the last element
            _lru_tail = nodePointer->prev;
            nodePointer->prev->next.reset();
        }
    } else { // the first element
        if (nodePointer->next) { // more than one element in LRU
            nodePointer->next->prev = nullptr;
            _lru_head = std::move(nodePointer->next);
        } else {
            _lru_head.reset();
            _lru_tail = nullptr;
        }
    }
    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Get(const std::string &key, std::string &value) {
    auto node = _lru_index.find(key);
    if (node != _lru_index.end()) {
        value = node->second.get().value;
        MoveNodeToHead(node->second.get());
        return true;
    }
    return false;
}

} // namespace Backend
} // namespace Afina
