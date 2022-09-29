#pragma once

#include <unordered_map>
#include <list>

template<typename K, typename V>
class LRUCache {

    std::unordered_map<K,V> cache_map;
    std::unordered_map<K,typename std::list<K>::iterator> key_map;
    std::list<K> usage;
    
    size_t max_size;

public:

    LRUCache(size_t s): max_size{s} {}

    void insert(K key, V value) {
        if (cache_map.size() >= max_size) {
            evict();
        }
        cache_map[key] = value;
        usage.push_front(key);
        key_map[key] = usage.begin();
    }

    void evict() {
        cache_map.erase(usage.back());
        key_map.erase(usage.back());
        usage.pop_back();
    }

    V access(K key) {
        if (cache_map.find(key) != cache_map.end()) {
            usage.erase(key_map[key]);
            usage.push_front(key);
            key_map[key] = usage.begin();
            return cache_map[key];
        }
        return nullptr;
    }

};