#ifndef SESSIONMANAGER_H
#define SESSIONMANAGER_H

#include "../../include/sdm_types.hpp"
#include "SecurityManager.h"
#include "CacheManager.h"
#include "DatabaseManager.h"
#include "../../source/data_structures/Map.h"
#include <string>
#include <chrono>
#include <iostream>

using namespace std;

class SessionManager {
private:
    SecurityManager& security_;
    CacheManager& cache_;
    DatabaseManager& db_;
    uint32_t session_timeout_;
    Map<uint64_t, vector<string>> driver_sessions_;
    
public:
    SessionManager(SecurityManager& security, CacheManager& cache, 
                   DatabaseManager& db, uint32_t timeout = 1800)
        : security_(security), cache_(cache), db_(db), session_timeout_(timeout) {}
    
    bool login(const string& username, const string& password, 
               string& session_id, DriverProfile& driver) {
        DriverProfile found_driver;
        bool found = false;
        
        auto all_drivers = db_.get_all_drivers();
        for (const auto& d : all_drivers) {
            if (string(d.username, strnlen(d.username, sizeof(d.username))) == username) {
                found_driver = d;
                found = true;
                break;
            }
        }
        
        if (!found) {
            return false;
        }
        
        string password_hash = security_.hash_password(password);
        if (string(found_driver.password_hash, strnlen(found_driver.password_hash, sizeof(found_driver.password_hash))) != password_hash) {
            return false;
        }
        
        session_id = security_.generate_session_id();
        
        SessionInfo session;
        memset(&session, 0, sizeof(SessionInfo));
        strncpy(session.session_id, session_id.c_str(), sizeof(session.session_id) - 1);
        session.driver_id = found_driver.driver_id;
        session.login_time = static_cast<uint64_t>(time(nullptr));
        session.last_activity = session.login_time;
        session.operations_count = 0;
        
        cache_.put_session(session_id, session);
        
        found_driver.last_login = session.login_time;
        db_.update_driver(found_driver);
        
        cache_.put_driver(found_driver.driver_id, found_driver);
        
        vector<string>* sessions = driver_sessions_.find(found_driver.driver_id);
        if (sessions) {
            sessions->push_back(session_id);
        } else {
            vector<string> new_sessions;
            new_sessions.push_back(session_id);
            driver_sessions_.insert(found_driver.driver_id, new_sessions);
        }
        
        driver = found_driver;
        return true;
    }
    
    bool logout(const string& session_id) {
        SessionInfo session;
        if (cache_.get_session(session_id, session)) {
            uint64_t driver_id = session.driver_id;
            
            vector<string>* sessions = driver_sessions_.find(driver_id);
            if (sessions) {
                auto it = find(sessions->begin(), sessions->end(), session_id);
                if (it != sessions->end()) {
                    sessions->erase(it);
                }
                if (sessions->empty()) {
                    driver_sessions_.erase(driver_id);
                }
            }
        }
        
        cache_.invalidate_session(session_id);
        return true;
    }
    
    bool validate_session(const string& session_id, SessionInfo& session) {
        if (!security_.is_valid_session_id(session_id)) {
            return false;
        }
        
        if (!cache_.get_session(session_id, session)) {
            return false;
        }
        
        session.last_activity = static_cast<uint64_t>(time(nullptr));
        cache_.put_session(session_id, session);
        
        return true;
    }
    
    bool get_driver_from_session(const string& session_id, DriverProfile& driver) {
        SessionInfo session;
        if (!validate_session(session_id, session)) {
            return false;
        }
        
        if (cache_.get_driver(session.driver_id, driver)) {
            return true;
        }
        
        if (db_.read_driver(session.driver_id, driver)) {
            cache_.put_driver(session.driver_id, driver);
            return true;
        }
        
        return false;
    }
    
    void increment_operation_count(const string& session_id) {
        SessionInfo session;
        if (cache_.get_session(session_id, session)) {
            session.operations_count++;
            cache_.put_session(session_id, session);
        }
    }
    
    bool is_admin(const string& session_id) {
        DriverProfile driver;
        if (get_driver_from_session(session_id, driver)) {
            return driver.role == UserRole::ADMIN;
        }
        return false;
    }
    
    bool is_fleet_manager(const string& session_id) {
        DriverProfile driver;
        if (get_driver_from_session(session_id, driver)) {
            return driver.role == UserRole::FLEET_MANAGER;
        }
        return false;
    }
    
    bool register_user(const string& username, const string& password,
                      const string& full_name, const string& email,
                      const string& phone, UserRole role = UserRole::DRIVER) {
        auto all_drivers = db_.get_all_drivers();
        for (const auto& d : all_drivers) {
            if (string(d.username, strnlen(d.username, sizeof(d.username))) == username) {
                return false;
            }
        }
        
        DriverProfile new_driver;
        new_driver.driver_id = db_.get_max_driver_id() + 1;
        
        strncpy(new_driver.username, username.c_str(), sizeof(new_driver.username) - 1);
        strncpy(new_driver.full_name, full_name.c_str(), sizeof(new_driver.full_name) - 1);
        strncpy(new_driver.email, email.c_str(), sizeof(new_driver.email) - 1);
        strncpy(new_driver.phone, phone.c_str(), sizeof(new_driver.phone) - 1);
        
        string password_hash = security_.hash_password(password);
        strncpy(new_driver.password_hash, password_hash.c_str(), sizeof(new_driver.password_hash) - 1);
        
        new_driver.role = role;
        new_driver.is_active = 1;
        new_driver.created_time = static_cast<uint64_t>(time(nullptr));
        new_driver.safety_score = 1000;
        
        return db_.create_driver(new_driver);
    }
    
    bool change_password(const string& session_id, 
                        const string& old_password,
                        const string& new_password) {
        DriverProfile driver;
        if (!get_driver_from_session(session_id, driver)) {
            return false;
        }
        
        string old_hash = security_.hash_password(old_password);
        if (string(driver.password_hash, strnlen(driver.password_hash, sizeof(driver.password_hash))) != old_hash) {
            return false;
        }
        
        string new_hash = security_.hash_password(new_password);
        strncpy(driver.password_hash, new_hash.c_str(), sizeof(driver.password_hash) - 1);
        
        bool success = db_.update_driver(driver);
        
        if (success) {
            cache_.put_driver(driver.driver_id, driver, true);
        }
        
        return success;
    }
    
    bool reset_password_admin(uint64_t driver_id, const string& new_password,
                             const string& admin_session_id) {
        if (!is_admin(admin_session_id)) {
            return false;
        }
        
        DriverProfile driver;
        if (!db_.read_driver(driver_id, driver)) {
            return false;
        }
        
        string new_hash = security_.hash_password(new_password);
        strncpy(driver.password_hash, new_hash.c_str(), sizeof(driver.password_hash) - 1);
        
        bool success = db_.update_driver(driver);
        
        if (success) {
            cache_.invalidate_driver(driver_id);
        }
        
        return success;
    }
    
    void cleanup_expired_sessions() {
        cache_.clear_expired_sessions();
    }
    
    void logout_all_driver_sessions(uint64_t driver_id) {
        vector<string>* sessions = driver_sessions_.find(driver_id);
        if (sessions) {
            for (const auto& session_id : *sessions) {
                cache_.invalidate_session(session_id);
            }
            driver_sessions_.erase(driver_id);
        }
    }
    
    size_t get_active_session_count(uint64_t driver_id) const {
        vector<string> sessions;
        if (driver_sessions_.get(driver_id, sessions)) {
            return sessions.size();
        }
        return 0;
    }
    
    vector<string> get_driver_sessions(uint64_t driver_id) const {
        vector<string> sessions;
        driver_sessions_.get(driver_id, sessions);
        return sessions;
    }
    
    void cleanup_expired_and_orphaned() {
        cleanup_expired_sessions();
        
        vector<uint64_t> drivers_to_clean;
        
        for (auto it = driver_sessions_.begin(); it != driver_sessions_.end(); ++it) {
            auto pair = *it;
            uint64_t driver_id = pair.first;
            vector<string> sessions = pair.second;
            
            vector<string> valid_sessions;
            for (const auto& session_id : sessions) {
                SessionInfo session;
                if (cache_.get_session(session_id, session)) {
                    valid_sessions.push_back(session_id);
                }
            }
            
            if (valid_sessions.empty()) {
                drivers_to_clean.push_back(driver_id);
            } else {
                driver_sessions_.insert(driver_id, valid_sessions);
            }
        }
        
        for (uint64_t driver_id : drivers_to_clean) {
            driver_sessions_.erase(driver_id);
        }
    }
};

#endif