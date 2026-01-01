#ifndef MAP_H
#define MAP_H

#include <vector>
#include <string>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <utility>
#include <algorithm>
#include <initializer_list>
#include <iostream>
using namespace std;

template <typename K, typename V>
class Map {
private:
    struct MapNode {
        K key;
        V value;
        MapNode* next;
        
        MapNode(const K& k, const V& v) : key(k), value(v), next(nullptr) {}
    };
    
    vector<MapNode*> buckets_;
    size_t capacity_;
    size_t size_;
    float load_factor_;
    hash<K> hasher_;
    
    size_t compute_hash(const K& key) const {
        return hasher_(key) % capacity_;
    }
    
    void rehash() {
        size_t old_capacity = capacity_;
        capacity_ *= 2;
        
        vector<MapNode*> old_buckets = move(buckets_);
        buckets_.resize(capacity_, nullptr);
        size_ = 0;
        
        for (size_t i = 0; i < old_capacity; i++) {
            MapNode* node = old_buckets[i];
            while (node) {
                MapNode* next = node->next;
                insert(node->key, node->value);
                delete node;
                node = next;
            }
        }
    }
    
public:
    class Iterator {
    private:
        const Map* map_;
        size_t bucket_idx_;
        MapNode* current_;
        
        void advance_to_next_valid() {
            while (current_ == nullptr && bucket_idx_ < map_->capacity_) {
                bucket_idx_++;
                if (bucket_idx_ < map_->capacity_) {
                    current_ = map_->buckets_[bucket_idx_];
                }
            }
        }
        
    public:
        Iterator(const Map* m, size_t idx, MapNode* node) 
            : map_(m), bucket_idx_(idx), current_(node) {
            if (current_ == nullptr && bucket_idx_ < map_->capacity_) {
                advance_to_next_valid();
            }
        }
        
        pair<K, V> operator*() const {
            return {current_->key, current_->value};
        }
        
        Iterator& operator++() {
            if (current_) {
                current_ = current_->next;
                if (!current_) {
                    bucket_idx_++;
                    advance_to_next_valid();
                }
            }
            return *this;
        }
        
        bool operator!=(const Iterator& other) const {
            return current_ != other.current_ || bucket_idx_ != other.bucket_idx_;
        }
        
        bool operator==(const Iterator& other) const {
            return current_ == other.current_ && bucket_idx_ == other.bucket_idx_;
        }
        
        K& key() const { return current_->key; }
        V& value() const { return current_->value; }
    };
    
    Map(size_t initial_capacity = 1024, float load_factor = 0.75f)
        : capacity_(initial_capacity), size_(0), load_factor_(load_factor) {
        buckets_.resize(capacity_, nullptr);
    }
    
    Map(initializer_list<pair<K, V>> list, size_t initial_capacity = 1024, float load_factor = 0.75f)
        : capacity_(initial_capacity), size_(0), load_factor_(load_factor) {
        if (list.size() * 2 > capacity_) capacity_ = list.size() * 2;
        buckets_.resize(capacity_, nullptr);
        for (const auto& item : list) {
            insert(item.first, item.second);
        }
    }
    
    ~Map() {
        clear();
    }
    
    void insert(const K& key, const V& value) {
        size_t index = compute_hash(key);
        MapNode* node = buckets_[index];
        
        while (node) {
            if (node->key == key) {
                node->value = value;
                return;
            }
            node = node->next;
        }
        
        MapNode* new_node = new MapNode(key, value);
        new_node->next = buckets_[index];
        buckets_[index] = new_node;
        size_++;
        
        if (static_cast<float>(size_) / capacity_ > load_factor_) {
            rehash();
        }
    }
    
    V& operator[](const K& key) {
        size_t index = compute_hash(key);
        MapNode* node = buckets_[index];
        
        while (node) {
            if (node->key == key) {
                return node->value;
            }
            node = node->next;
        }
        
        insert(key, V());
        return (*this)[key];
    }
    
    bool get(const K& key, V& value) const {
        size_t index = compute_hash(key);
        MapNode* node = buckets_[index];
        
        while (node) {
            if (node->key == key) {
                value = node->value;
                return true;
            }
            node = node->next;
        }
        
        return false;
    }
    
    bool contains(const K& key) const {
        V dummy;
        return get(key, dummy);
    }
    
    V* find(const K& key) {
        size_t index = compute_hash(key);
        MapNode* node = buckets_[index];
        
        while (node) {
            if (node->key == key) {
                return &node->value;
            }
            node = node->next;
        }
        
        return nullptr;
    }
    
    const V* find(const K& key) const {
        size_t index = compute_hash(key);
        MapNode* node = buckets_[index];
        
        while (node) {
            if (node->key == key) {
                return &node->value;
            }
            node = node->next;
        }
        
        return nullptr;
    }
    
    bool erase(const K& key) {
        size_t index = compute_hash(key);
        MapNode* node = buckets_[index];
        MapNode* prev = nullptr;
        
        while (node) {
            if (node->key == key) {
                if (prev) {
                    prev->next = node->next;
                } else {
                    buckets_[index] = node->next;
                }
                delete node;
                size_--;
                return true;
            }
            prev = node;
            node = node->next;
        }
        
        return false;
    }
    
    void clear() {
        for (size_t i = 0; i < capacity_; i++) {
            MapNode* node = buckets_[i];
            while (node) {
                MapNode* next = node->next;
                delete node;
                node = next;
            }
            buckets_[i] = nullptr;
        }
        size_ = 0;
    }
    
    size_t size() const { return size_; }
    size_t capacity() const { return capacity_; }
    bool empty() const { return size_ == 0; }
    
    Iterator begin() {
        for (size_t i = 0; i < capacity_; i++) {
            if (buckets_[i] != nullptr) {
                return Iterator(this, i, buckets_[i]);
            }
        }
        return end();
    }
    
    Iterator begin() const {
        for (size_t i = 0; i < capacity_; i++) {
            if (buckets_[i] != nullptr) {
                return Iterator(this, i, buckets_[i]);
            }
        }
        return end();
    }
    
    Iterator end() {
        return Iterator(this, capacity_, nullptr);
    }

    Iterator end() const {
        return Iterator(this, capacity_, nullptr);
    }
    
    vector<K> keys() const {
        vector<K> result;
        result.reserve(size_);
        
        for (size_t i = 0; i < capacity_; i++) {
            MapNode* node = buckets_[i];
            while (node) {
                result.push_back(node->key);
                node = node->next;
            }
        }
        
        return result;
    }
    
    vector<V> values() const {
        vector<V> result;
        result.reserve(size_);
        
        for (size_t i = 0; i < capacity_; i++) {
            MapNode* node = buckets_[i];
            while (node) {
                result.push_back(node->value);
                node = node->next;
            }
        }
        
        return result;
    }
};

template <typename K, typename V>
class MultiMap {
private:
    Map<K, vector<V>> internal_map_;
    size_t total_elements_;
    
public:
    MultiMap() : total_elements_(0) {}
    
    void insert(const K& key, const V& value) {
        vector<V>* values = internal_map_.find(key);
        if (values) {
            values->push_back(value);
        } else {
            vector<V> new_values;
            new_values.push_back(value);
            internal_map_.insert(key, new_values);
        }
        total_elements_++;
    }
    
    vector<V> get(const K& key) const {
        vector<V> result;
        internal_map_.get(key, result);
        return result;
    }
    
    bool contains(const K& key) const {
        return internal_map_.contains(key);
    }
    
    size_t count(const K& key) const {
        vector<V> values;
        if (internal_map_.get(key, values)) {
            return values.size();
        }
        return 0;
    }
    
    bool erase_key(const K& key) {
        size_t removed = count(key);
        if (internal_map_.erase(key)) {
            total_elements_ -= removed;
            return true;
        }
        return false;
    }
    
    bool erase_value(const K& key, const V& value) {
        vector<V>* values = internal_map_.find(key);
        if (!values) return false;
        
        auto it = find(values->begin(), values->end(), value);
        if (it != values->end()) {
            values->erase(it);
            total_elements_--;
            if (values->empty()) {
                internal_map_.erase(key);
            }
            return true;
        }
        return false;
    }
    
    void clear() {
        internal_map_.clear();
        total_elements_ = 0;
    }
    
    size_t size() const { return total_elements_; }
    size_t key_count() const { return internal_map_.size(); }
    bool empty() const { return total_elements_ == 0; }
    
    vector<K> keys() const {
        return internal_map_.keys();
    }
};

#endif