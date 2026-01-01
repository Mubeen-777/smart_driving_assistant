#ifndef MINHEAP_H
#define MINHEAP_H

#include <vector>
#include <stdexcept>
#include <cstdint>
#include <iostream>
using namespace std;

template <typename T, typename Compare = less<T>>
class MinHeap
{
private:
    vector<T> heap_;
    Compare comp_;

    int parent(int i) const { return (i - 1) / 2; }
    int left_child(int i) const { return 2 * i + 1; }
    int right_child(int i) const { return 2 * i + 2; }

    void heapify_up(int index)
    {
        while (index > 0 && comp_(heap_[index], heap_[parent(index)]))
        {
            swap(heap_[index], heap_[parent(index)]);
            index = parent(index);
        }
    }

    void heapify_down(int index)
    {
        int size = heap_.size();

        while (true)
        {
            int smallest = index;
            int left = left_child(index);
            int right = right_child(index);

            if (left < size && comp_(heap_[left], heap_[smallest]))
            {
                smallest = left;
            }

            if (right < size && comp_(heap_[right], heap_[smallest]))
            {
                smallest = right;
            }

            if (smallest == index)
                break;

            swap(heap_[index], heap_[smallest]);
            index = smallest;
        }
    }

public:
    MinHeap() = default;

    MinHeap(const vector<T> &data) : heap_(data)
    {
        for (int i = heap_.size() / 2 - 1; i >= 0; i--)
        {
            heapify_down(i);
        }
    }

    void insert(const T &value)
    {
        heap_.push_back(value);
        heapify_up(heap_.size() - 1);
    }

    const T &peek() const
    {
        if (heap_.empty())
        {
            throw underflow_error("Heap is empty");
        }
        return heap_[0];
    }

    T extract_min()
    {
        if (heap_.empty())
        {
            throw underflow_error("Heap is empty");
        }

        T min_val = heap_[0];
        heap_[0] = heap_.back();
        heap_.pop_back();

        if (!heap_.empty())
        {
            heapify_down(0);
        }

        return min_val;
    }

    void update(int index, const T &new_value)
    {
        if (index < 0 || index >= heap_.size())
        {
            throw out_of_range("Index out of range");
        }

        T old_value = heap_[index];
        heap_[index] = new_value;

        if (comp_(new_value, old_value))
        {
            heapify_up(index);
        }
        else
        {
            heapify_down(index);
        }
    }

    void remove(int index)
    {
        if (index < 0 || index >= heap_.size())
        {
            throw out_of_range("Index out of range");
        }

        heap_[index] = heap_.back();
        heap_.pop_back();

        if (index < heap_.size())
        {
            heapify_up(index);
            heapify_down(index);
        }
    }

    vector<T> get_top_k(int k) const
    {
        vector<T> result;
        MinHeap<T, Compare> temp_heap = *this;

        for (int i = 0; i < k && !temp_heap.empty(); i++)
        {
            result.push_back(temp_heap.extract_min());
        }

        return result;
    }

    bool empty() const { return heap_.empty(); }
    size_t size() const { return heap_.size(); }
    void clear() { heap_.clear(); }

    const vector<T> &get_internal_array() const { return heap_; }
};

struct MaintenanceAlert
{
    uint64_t vehicle_id;
    uint64_t alert_id;
    uint32_t priority;
    uint64_t due_timestamp;
    char description[128];
    uint8_t severity;

    MaintenanceAlert() : vehicle_id(0), alert_id(0), priority(0),
                         due_timestamp(0), severity(0)
    {
        memset(description, 0, sizeof(description));
    }

    MaintenanceAlert(uint64_t vid, uint64_t aid, uint32_t pri,
                     uint64_t due, const string &desc, uint8_t sev)
        : vehicle_id(vid), alert_id(aid), priority(pri),
          due_timestamp(due), severity(sev)
    {
        strncpy(description, desc.c_str(), sizeof(description) - 1);
    }

    bool operator<(const MaintenanceAlert &other) const
    {
        return priority < other.priority;
    }
};

using MaintenanceAlertQueue = MinHeap<MaintenanceAlert>;

#endif