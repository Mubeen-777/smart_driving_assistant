#ifndef STACK_H
#define STACK_H

#include <vector>
#include <cstdint>
#include <cstring>
#include<iostream>
using namespace std;

template <typename T>
class Stack
{
private:
    vector<T> data_;
    size_t max_size_;

public:
    Stack(size_t max_size = 100) : max_size_(max_size)
    {
        data_.reserve(max_size);
    }

    void push(const T &item)
    {
        if (data_.size() >= max_size_)
        {
            data_.erase(data_.begin());
        }
        data_.push_back(item);
    }

    T pop()
    {
        if (empty())
        {
            throw underflow_error("Stack is empty");
        }
        T item = data_.back();
        data_.pop_back();
        return item;
    }

    const T &top() const
    {
        if (empty())
        {
            throw underflow_error("Stack is empty");
        }
        return data_.back();
    }

    T &top()
    {
        if (empty())
        {
            throw underflow_error("Stack is empty");
        }
        return data_.back();
    }

    bool empty() const { return data_.empty(); }
    size_t size() const { return data_.size(); }
    size_t max_size() const { return max_size_; }

    void clear() { data_.clear(); }

    const vector<T> &get_all() const { return data_; }
};

struct UndoState
{
    uint8_t operation_type;
    uint8_t entity_type;
    uint64_t entity_id;
    uint64_t timestamp;
    char old_data[1024];
    uint32_t old_data_size;
    char new_data[1024];
    uint32_t new_data_size;
    char description[128];

    UndoState() : operation_type(0), entity_type(0), entity_id(0),
                  timestamp(0), old_data_size(0), new_data_size(0)
    {
        memset(old_data, 0, sizeof(old_data));
        memset(new_data, 0, sizeof(new_data));
        memset(description, 0, sizeof(description));
    }
};

struct NavigationState
{
    char screen_id[64];
    uint64_t entity_id;
    uint64_t timestamp;
    char parameters[256];
    NavigationState() : entity_id(0), timestamp(0)
    {
        memset(screen_id, 0, sizeof(screen_id));
        memset(parameters, 0, sizeof(parameters));
    }

    NavigationState(const char *screen, uint64_t id)
        : entity_id(id), timestamp(0)
    {
        strncpy(screen_id, screen, sizeof(screen_id) - 1);
        memset(parameters, 0, sizeof(parameters));
    }
};

using UndoStack = Stack<UndoState>;
using NavigationStack = Stack<NavigationState>;
#endif