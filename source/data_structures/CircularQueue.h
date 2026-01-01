#ifndef CIRCULARQUEUE_H
#define CIRCULARQUEUE_H

#include <vector>
#include <mutex>
#include <condition_variable>
#include <stdexcept>
#include <atomic>
#include <iostream>
#include <string>
#include <chrono>
using namespace std;

struct ServerRequest {
    int client_socket;
    uint64_t request_id;
    uint64_t timestamp;
    string client_ip;
    string request_data;
    
    ServerRequest() : client_socket(-1), request_id(0), timestamp(0) {}
    
    ServerRequest(int sock, uint64_t id, const string& ip, const string& data)
        : client_socket(sock), request_id(id), timestamp(0), 
          client_ip(ip), request_data(data) {
        timestamp = chrono::system_clock::now().time_since_epoch().count();
    }
};

template <typename T>
class CircularQueue
{
private:
    vector<T> buffer_;
    int head_;
    int tail_;
    int capacity_;
    atomic<int> size_;
    mutex mtx_;
    condition_variable not_empty_;
    condition_variable not_full_;
    bool shutdown_;

public:
    CircularQueue(int capacity)
        : head_(0), tail_(0), capacity_(capacity), size_(0), shutdown_(false)
    {
        buffer_.resize(capacity);
    }

    ~CircularQueue()
    {
        shutdown();
    }

    void enqueue(const T &item)
    {
        unique_lock<mutex> lock(mtx_);

        not_full_.wait(lock, [this]()
                       { return size_.load() < capacity_ || shutdown_; });

        if (shutdown_)
        {
            throw runtime_error("Queue is shut down");
        }

        buffer_[tail_] = item;
        tail_ = (tail_ + 1) % capacity_;
        size_++;

        not_empty_.notify_one();
    }

    bool try_enqueue(const T &item)
    {
        unique_lock<mutex> lock(mtx_);

        if (size_.load() >= capacity_ || shutdown_)
        {
            return false;
        }

        buffer_[tail_] = item;
        tail_ = (tail_ + 1) % capacity_;
        size_++;

        not_empty_.notify_one();
        return true;
    }

    T dequeue()
    {
        unique_lock<mutex> lock(mtx_);

        not_empty_.wait(lock, [this]()
                        { return size_.load() > 0 || shutdown_; });

        if (shutdown_ && size_.load() == 0)
        {
            throw runtime_error("Queue is shut down and empty");
        }

        T item = buffer_[head_];
        head_ = (head_ + 1) % capacity_;
        size_--;

        not_full_.notify_one();
        return item;
    }

    bool try_dequeue(T &item)
    {
        unique_lock<mutex> lock(mtx_);

        if (size_.load() == 0)
        {
            return false;
        }

        item = buffer_[head_];
        head_ = (head_ + 1) % capacity_;
        size_--;

        not_full_.notify_one();
        return true;
    }

    bool peek(T &item) const
    {
        unique_lock<mutex> lock(mtx_);

        if (size_.load() == 0)
        {
            return false;
        }

        item = buffer_[head_];
        return true;
    }

    int size() const { return size_.load(); }
    int capacity() const { return capacity_; }
    bool empty() const { return size_.load() == 0; }
    bool full() const { return size_.load() >= capacity_; }

    void shutdown()
    {
        {
            unique_lock<mutex> lock(mtx_);
            shutdown_ = true;
        }
        not_empty_.notify_all();
        not_full_.notify_all();
    }

    void clear()
    {
        unique_lock<mutex> lock(mtx_);
        head_ = 0;
        tail_ = 0;
        size_ = 0;
        not_full_.notify_all();
    }
};

template <typename T>
class LockFreeCircularQueue
{
private:
    vector<T> buffer_;
    atomic<int> head_;
    atomic<int> tail_;
    int capacity_;

public:
    LockFreeCircularQueue(int capacity)
        : head_(0), tail_(0), capacity_(capacity)
    {
        buffer_.resize(capacity);
    }

    bool try_enqueue(const T &item)
    {
        int current_tail = tail_.load(memory_order_relaxed);
        int next_tail = (current_tail + 1) % capacity_;
        int current_head = head_.load(memory_order_acquire);

        if (next_tail == current_head)
        {
            return false;
        }

        buffer_[current_tail] = item;
        tail_.store(next_tail, memory_order_release);
        return true;
    }

    bool try_dequeue(T &item)
    {
        int current_head = head_.load(memory_order_relaxed);
        int current_tail = tail_.load(memory_order_acquire);

        if (current_head == current_tail)
        {
            return false;
        }

        item = buffer_[current_head];
        head_.store((current_head + 1) % capacity_, memory_order_release);
        return true;
    }

    bool empty() const
    {
        return head_.load(memory_order_acquire) ==
               tail_.load(memory_order_acquire);
    }

    int size() const
    {
        int h = head_.load(memory_order_acquire);
        int t = tail_.load(memory_order_acquire);
        return (t >= h) ? (t - h) : (capacity_ - h + t);
    }
};

struct GPSDataPoint
{
    uint64_t trip_id;
    uint64_t timestamp;
    double latitude;
    double longitude;
    float speed;
    float altitude;
    float accuracy;
    uint8_t satellites;

    GPSDataPoint() : trip_id(0), timestamp(0), latitude(0), longitude(0),
                     speed(0), altitude(0), accuracy(0), satellites(0) {}

    GPSDataPoint(uint64_t tid, uint64_t ts, double lat, double lon, float spd)
        : trip_id(tid), timestamp(ts), latitude(lat), longitude(lon),
          speed(spd), altitude(0), accuracy(0), satellites(0) {}
};

using GPSBuffer = CircularQueue<GPSDataPoint>;
using RequestQueue = CircularQueue<ServerRequest>;
using FastGPSBuffer = LockFreeCircularQueue<GPSDataPoint>;

#endif