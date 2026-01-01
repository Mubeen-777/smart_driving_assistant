#ifndef CACHEMANAGER_H
#define CACHEMANAGER_H

#include "../../source/data_structures/HashTable.h"
#include "../../include/sdm_types.hpp"
#include <memory>
#include <string>
#include <chrono>
#include <mutex>
#include <iostream>
using namespace std;

template<typename T>
struct CacheEntry {
    T data;
    uint64_t timestamp;
    uint32_t access_count;
    bool dirty;
    
    CacheEntry() : timestamp(0), access_count(0), dirty(false) {}
    CacheEntry(const T& d) : data(d), access_count(0), dirty(false) {
        timestamp = chrono::system_clock::now().time_since_epoch().count();
    }
};

class CacheManager {
private:
    LRUCache<uint64_t, CacheEntry<DriverProfile>> driver_cache_;
    LRUCache<uint64_t, CacheEntry<VehicleInfo>> vehicle_cache_;
    LRUCache<uint64_t, CacheEntry<TripRecord>> trip_cache_;
    
    LRUCache<string, SessionInfo> session_cache_;
    
    HashTable<string, vector<uint64_t>> query_result_cache_;
    
    uint64_t driver_hits_;
    uint64_t driver_misses_;
    uint64_t vehicle_hits_;
    uint64_t vehicle_misses_;
    uint64_t trip_hits_;
    uint64_t trip_misses_;
    uint64_t session_hits_;
    uint64_t session_misses_;
    mutable std::mutex cache_mutex_;

public:
    CacheManager(size_t driver_capacity = 256, 
                 size_t vehicle_capacity = 256,
                 size_t trip_capacity = 512,
                 size_t session_capacity = 1024)
        : driver_cache_(driver_capacity),
          vehicle_cache_(vehicle_capacity),
          trip_cache_(trip_capacity),
          session_cache_(session_capacity),
          driver_hits_(0), driver_misses_(0),
          vehicle_hits_(0), vehicle_misses_(0),
          trip_hits_(0), trip_misses_(0),
          session_hits_(0), session_misses_(0) {}
    
    bool get_driver(uint64_t driver_id, DriverProfile& driver) {
        lock_guard<mutex> lock(cache_mutex_);
        CacheEntry<DriverProfile> entry;
        if (driver_cache_.get(driver_id, entry)) {
            driver = entry.data;
            entry.access_count++;
            driver_cache_.put(driver_id, entry);
            driver_hits_++;
            return true;
        }
        driver_misses_++;
        return false;
    }
    
    void put_driver(uint64_t driver_id, const DriverProfile& driver, bool dirty = false) {
        lock_guard<mutex> lock(cache_mutex_);
        CacheEntry<DriverProfile> entry(driver);
        entry.dirty = dirty;
        driver_cache_.put(driver_id, entry);
    }
    
    void invalidate_driver(uint64_t driver_id) {
        lock_guard<mutex> lock(cache_mutex_);
        driver_cache_.remove(driver_id);
    }
    
    bool get_vehicle(uint64_t vehicle_id, VehicleInfo& vehicle) {
        lock_guard<mutex> lock(cache_mutex_);
        CacheEntry<VehicleInfo> entry;
        if (vehicle_cache_.get(vehicle_id, entry)) {
            vehicle = entry.data;
            entry.access_count++;
            vehicle_cache_.put(vehicle_id, entry);
            vehicle_hits_++;
            return true;
        }
        vehicle_misses_++;
        return false;
    }
    
    void put_vehicle(uint64_t vehicle_id, const VehicleInfo& vehicle, bool dirty = false) {
        lock_guard<mutex> lock(cache_mutex_);
        CacheEntry<VehicleInfo> entry(vehicle);
        entry.dirty = dirty;
        vehicle_cache_.put(vehicle_id, entry);
    }
    
    void invalidate_vehicle(uint64_t vehicle_id) {
        lock_guard<mutex> lock(cache_mutex_);
        vehicle_cache_.remove(vehicle_id);
    }
    
    bool get_trip(uint64_t trip_id, TripRecord& trip) {
        lock_guard<mutex> lock(cache_mutex_);
        CacheEntry<TripRecord> entry;
        if (trip_cache_.get(trip_id, entry)) {
            trip = entry.data;
            entry.access_count++;
            trip_cache_.put(trip_id, entry);
            trip_hits_++;
            return true;
        }
        trip_misses_++;
        return false;
    }
    
    void put_trip(uint64_t trip_id, const TripRecord& trip, bool dirty = false) {
        lock_guard<mutex> lock(cache_mutex_);
        CacheEntry<TripRecord> entry(trip);
        entry.dirty = dirty;
        trip_cache_.put(trip_id, entry);
    }
    
    void invalidate_trip(uint64_t trip_id) {
        lock_guard<mutex> lock(cache_mutex_);
        trip_cache_.remove(trip_id);
    }
    
    bool get_session(const string& session_id, SessionInfo& session) {
        lock_guard<mutex> lock(cache_mutex_);
        if (session_cache_.get(session_id, session)) {
            uint64_t current_time = static_cast<uint64_t>(time(nullptr));
            uint64_t elapsed = (current_time > session.last_activity) ? 
                              (current_time - session.last_activity) : 0;
            
            if (elapsed > 1800) { 
                session_cache_.remove(session_id);
                session_misses_++;
                return false;
            }
            
            session.last_activity = current_time;
            session_cache_.put(session_id, session); 
            session_hits_++;
            return true;
        }
        session_misses_++;
        return false;
    }
    
    void put_session(const string& session_id, const SessionInfo& session) {
        lock_guard<mutex> lock(cache_mutex_);
        session_cache_.put(session_id, session);
    }
    
    void invalidate_session(const string& session_id) {
        lock_guard<mutex> lock(cache_mutex_);
        session_cache_.remove(session_id);
    }
    
    bool get_query_result(const string& query_key, vector<uint64_t>& results) {
        lock_guard<mutex> lock(cache_mutex_);
        return query_result_cache_.get(query_key, results);
    }
    
    void put_query_result(const string& query_key, const vector<uint64_t>& results) {
        lock_guard<mutex> lock(cache_mutex_);
        query_result_cache_.insert(query_key, results);
    }
    
    void invalidate_query_result(const string& query_key) {
        lock_guard<mutex> lock(cache_mutex_);
        query_result_cache_.remove(query_key);
    }
    
    void clear_query_cache() {
        lock_guard<mutex> lock(cache_mutex_);
        query_result_cache_.clear();
    }
    
    struct CacheStats {
        uint64_t driver_hits;
        uint64_t driver_misses;
        double driver_hit_rate;
        
        uint64_t vehicle_hits;
        uint64_t vehicle_misses;
        double vehicle_hit_rate;
        
        uint64_t trip_hits;
        uint64_t trip_misses;
        double trip_hit_rate;
        
        uint64_t session_hits;
        uint64_t session_misses;
        double session_hit_rate;
        
        size_t driver_cache_size;
        size_t vehicle_cache_size;
        size_t trip_cache_size;
        size_t session_cache_size;
        size_t query_cache_size;
    };
    
    CacheStats get_stats() const {
        lock_guard<mutex> lock(cache_mutex_);
        CacheStats stats;
        
        stats.driver_hits = driver_hits_;
        stats.driver_misses = driver_misses_;
        stats.driver_hit_rate = (driver_hits_ + driver_misses_ > 0) 
            ? (double)driver_hits_ / (driver_hits_ + driver_misses_) : 0.0;
        
        stats.vehicle_hits = vehicle_hits_;
        stats.vehicle_misses = vehicle_misses_;
        stats.vehicle_hit_rate = (vehicle_hits_ + vehicle_misses_ > 0)
            ? (double)vehicle_hits_ / (vehicle_hits_ + vehicle_misses_) : 0.0;
        
        stats.trip_hits = trip_hits_;
        stats.trip_misses = trip_misses_;
        stats.trip_hit_rate = (trip_hits_ + trip_misses_ > 0)
            ? (double)trip_hits_ / (trip_hits_ + trip_misses_) : 0.0;
        
        stats.session_hits = session_hits_;
        stats.session_misses = session_misses_;
        stats.session_hit_rate = (session_hits_ + session_misses_ > 0)
            ? (double)session_hits_ / (session_hits_ + session_misses_) : 0.0;
        
        stats.driver_cache_size = driver_cache_.size();
        stats.vehicle_cache_size = vehicle_cache_.size();
        stats.trip_cache_size = trip_cache_.size();
        stats.session_cache_size = session_cache_.size();
        stats.query_cache_size = query_result_cache_.size();
        
        return stats;
    }
    
    void reset_stats() {
        lock_guard<mutex> lock(cache_mutex_);
        driver_hits_ = 0;
        driver_misses_ = 0;
        vehicle_hits_ = 0;
        vehicle_misses_ = 0;
        trip_hits_ = 0;
        trip_misses_ = 0;
        session_hits_ = 0;
        session_misses_ = 0;
    }
    
    void clear_all() {
        lock_guard<mutex> lock(cache_mutex_);
        driver_cache_.clear();
        vehicle_cache_.clear();
        trip_cache_.clear();
        session_cache_.clear();
        query_result_cache_.clear();
    }
    
    void clear_expired_sessions() {
        cout << "void clear_expired_sessions(); is not implemented yet.";
    }
    
    void warmup_driver_cache(const vector<DriverProfile>& drivers) {
        for (const auto& driver : drivers) {
            put_driver(driver.driver_id, driver);
        }
    }
    
    void warmup_vehicle_cache(const vector<VehicleInfo>& vehicles) {
        for (const auto& vehicle : vehicles) {
            put_vehicle(vehicle.vehicle_id, vehicle);
        }
    }
};

#endif