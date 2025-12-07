#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <vector>
#include <string>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <iostream>
using namespace std;

template <typename K, typename V>
struct HashNode
{
    K key;
    V value;
    HashNode *next;

    HashNode(const K &k, const V &v) : key(k), value(v), next(nullptr) {}
};

template <typename K, typename V>
class HashTable
{
private:
    vector<HashNode<K, V> *> buckets_;
    size_t capacity_;
    size_t size_;
    float load_factor_;
    std::hash<K> hasher_; 

    size_t compute_hash(const K &key) const 
    {
        return hasher_(key) % capacity_;
    }

    void rehash()
    {
        size_t old_capacity = capacity_;
        capacity_ *= 2;

        vector<HashNode<K, V> *> old_buckets = move(buckets_);
        buckets_.resize(capacity_, nullptr);
        size_ = 0;

        for (size_t i = 0; i < old_capacity; i++)
        {
            HashNode<K, V> *node = old_buckets[i];
            while (node)
            {
                HashNode<K, V> *next = node->next;
                insert(node->key, node->value);
                delete node;
                node = next;
            }
        }
    }

public:
    HashTable(size_t initial_capacity = 1024, float load_factor = 0.75f)
        : capacity_(initial_capacity), size_(0), load_factor_(load_factor)
    {
        buckets_.resize(capacity_, nullptr);
    }

    ~HashTable()
    {
        clear();
    }

    void insert(const K &key, const V &value)
    {
        size_t index = compute_hash(key); 
        HashNode<K, V> *node = buckets_[index];

        while (node)
        {
            if (node->key == key)
            {
                node->value = value;
                return;
            }
            node = node->next;
        }

        HashNode<K, V> *new_node = new HashNode<K, V>(key, value);
        new_node->next = buckets_[index];
        buckets_[index] = new_node;
        size_++;

        if (static_cast<float>(size_) / capacity_ > load_factor_)
        {
            rehash();
        }
    }

    bool get(const K &key, V &value) const
    {
        size_t index = compute_hash(key); 
        HashNode<K, V> *node = buckets_[index];

        while (node)
        {
            if (node->key == key)
            {
                value = node->value;
                return true;
            }
            node = node->next;
        }

        return false;
    }

    bool contains(const K &key) const
    {
        V dummy;
        return get(key, dummy);
    }

    bool remove(const K &key)
    {
        size_t index = compute_hash(key); 
        HashNode<K, V> *node = buckets_[index];
        HashNode<K, V> *prev = nullptr;

        while (node)
        {
            if (node->key == key)
            {
                if (prev)
                {
                    prev->next = node->next;
                }
                else
                {
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

    void clear()
    {
        for (size_t i = 0; i < capacity_; i++)
        {
            HashNode<K, V> *node = buckets_[i];
            while (node)
            {
                HashNode<K, V> *next = node->next;
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
    float load_factor() const { return static_cast<float>(size_) / capacity_; }

    vector<K> keys() const
    {
        vector<K> result;
        result.reserve(size_);

        for (size_t i = 0; i < capacity_; i++)
        {
            HashNode<K, V> *node = buckets_[i];
            while (node)
            {
                result.push_back(node->key);
                node = node->next;
            }
        }

        return result;
    }
};

template <typename K, typename V>
class LRUCache
{
private:
    struct Node
    {
        K key;
        V value;
        Node *prev, *next;

        Node(const K &k, const V &v) : key(k), value(v), prev(nullptr), next(nullptr) {}
    };

    HashTable<K, Node *> cache_;
    Node *head_, *tail_;
    size_t capacity_;
    size_t size_;

    void move_to_head(Node *node)
    {
        remove_node(node);
        add_to_head(node);
    }

    void add_to_head(Node *node)
    {
        node->next = head_->next;
        node->prev = head_;
        head_->next->prev = node;
        head_->next = node;
    }

    void remove_node(Node *node)
    {
        node->prev->next = node->next;
        node->next->prev = node->prev;
    }

    Node *remove_tail()
    {
        Node *node = tail_->prev;
        remove_node(node);
        return node;
    }

public:
    LRUCache(size_t capacity) : capacity_(capacity), size_(0)
    {
        head_ = new Node(K(), V());
        tail_ = new Node(K(), V());
        head_->next = tail_;
        tail_->prev = head_;
    }

    ~LRUCache()
    {
        Node *node = head_;
        while (node)
        {
            Node *next = node->next;
            delete node;
            node = next;
        }
    }

    bool get(const K &key, V &value)
    {
        Node *node;
        if (!cache_.get(key, node))
        {
            return false;
        }

        move_to_head(node);
        value = node->value;
        return true;
    }

    void put(const K &key, const V &value)
    {
        Node *node;
        if (cache_.get(key, node))
        {
            node->value = value;
            move_to_head(node);
            return;
        }

        node = new Node(key, value);
        cache_.insert(key, node);
        add_to_head(node);
        size_++;

        if (size_ > capacity_)
        {
            Node *tail_node = remove_tail();
            cache_.remove(tail_node->key);
            delete tail_node;
            size_--;
        }
    }

    void remove(const K &key)
    {
        Node *node;
        if (cache_.get(key, node))
        {
            remove_node(node);
            cache_.remove(key);
            delete node;
            size_--;
        }
    }

    size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }
    void clear()
    {
        Node *node = head_->next;
        while (node != tail_)
        {
            Node *next = node->next;
            delete node;
            node = next;
        }
        head_->next = tail_;
        tail_->prev = head_;
        cache_.clear();
        size_ = 0;
    }
};

#endif
