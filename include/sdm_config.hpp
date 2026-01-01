#ifndef SDM_CONFIG_HPP
#define SDM_CONFIG_HPP

#include <string>
#include <cstdint>
#include <fstream>
#include <map>
#include <iostream>
using namespace std;

struct SDMConfig {
    
    uint64_t total_size;            
    uint64_t block_size;            
    uint32_t max_drivers;
    uint32_t max_vehicles;
    uint32_t max_trips;
    uint8_t btree_order;
    uint32_t cache_size;
    
    uint16_t port;
    uint32_t max_connections;
    uint32_t queue_capacity;
    uint16_t worker_threads;
    
    bool require_authentication;
    string password_hash_algo;
    uint32_t session_timeout;       
    string admin_username;
    string admin_password;
    
    uint16_t segment_tree_depth;
    uint32_t alert_check_interval;  
    
    string database_path;
    string index_path;
    string log_path;
    
    SDMConfig() : total_size(524288000), block_size(4096), max_drivers(10000),
                 max_vehicles(50000), max_trips(10000000), btree_order(5),
                 cache_size(256), port(8080), max_connections(1000),
                 queue_capacity(10000), worker_threads(16),
                 require_authentication(true), password_hash_algo("SHA256"),
                 session_timeout(1800), admin_username("admin"),
                 admin_password("admin123"), segment_tree_depth(15),
                 alert_check_interval(3600),
                 database_path("compiled/SDM.db"),
                 index_path("compiled/indexes"),
                 log_path("compiled/SDM.log") {}
    
    bool load_from_file(const string& filename) {
        ifstream file(filename);
        if (!file.is_open()) return false;
        
        string line, section;
        while (getline(file, line)) {
            
            size_t comment_pos = line.find('#');
            if (comment_pos != string::npos) {
                line = line.substr(0, comment_pos);
            }
            
            line.erase(0, line.find_first_not_of(" \t\r\n"));
            line.erase(line.find_last_not_of(" \t\r\n") + 1);
            
            if (line.empty()) continue;
            
            if (line[0] == '[' && line[line.length() - 1] == ']') {
                section = line.substr(1, line.length() - 2);
                continue;
            }
            
            size_t eq_pos = line.find('=');
            if (eq_pos == string::npos) continue;
            
            string key = line.substr(0, eq_pos);
            string value = line.substr(eq_pos + 1);
            
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            
            parse_value(section, key, value);
        }
        
        file.close();
        return true;
    }
    
private:
    void parse_value(const string& section, const string& key, const string& value) {
        if (section == "database") {
            if (key == "total_size") total_size = stoull(value);
            else if (key == "block_size") block_size = stoull(value);
            else if (key == "max_drivers") max_drivers = stoul(value);
            else if (key == "max_vehicles") max_vehicles = stoul(value);
            else if (key == "max_trips") max_trips = stoul(value);
            else if (key == "btree_order") btree_order = stoi(value);
            else if (key == "cache_size") cache_size = stoul(value);
        }
        else if (section == "server") {
            if (key == "port") port = stoi(value);
            else if (key == "max_connections") max_connections = stoul(value);
            else if (key == "queue_capacity") queue_capacity = stoul(value);
            else if (key == "worker_threads") worker_threads = stoi(value);
        }
        else if (section == "security") {
            if (key == "require_authentication") require_authentication = (value == "true");
            else if (key == "password_hash_algo") password_hash_algo = value;
            else if (key == "session_timeout") session_timeout = stoul(value);
            else if (key == "admin_username") admin_username = value;
            else if (key == "admin_password") admin_password = value;
        }
        else if (section == "analytics") {
            if (key == "segment_tree_depth") segment_tree_depth = stoi(value);
            else if (key == "alert_check_interval") alert_check_interval = stoul(value);
        }
        else if (section == "paths") {
            if (key == "database_path") database_path = value;
            else if (key == "index_path") index_path = value;
            else if (key == "log_path") log_path = value;
        }
    }
};

#endif 