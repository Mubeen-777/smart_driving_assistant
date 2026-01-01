#ifndef UDP_RECEIVER_H
#define UDP_RECEIVER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <atomic>
#include <functional>
#include <cstring>
#include <iostream>
#include <sstream>
#include <vector>

using namespace std;

struct AdasData {
    uint64_t timestamp;
    double latitude;
    double longitude;
    float kalman_speed;    
    float gps_speed;       
    float accel_x;         
    float accel_y;
    float accel_z;
    float gyro_x;          
    float gyro_y;
    float gyro_z;
};

struct AdasEvent {
    string event_type;     
    float value;
    double latitude;
    double longitude;
    uint64_t timestamp;
};

class UDPReceiver {
private:
    int socket_fd_;
    uint16_t port_;
    atomic<bool> running_;
    thread receive_thread_;
    
    function<void(const AdasData&)> data_callback_;
    function<void(const AdasEvent&)> event_callback_;
    
    void receive_loop() {
        char buffer[2048];
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        cout << "✅ UDP Receiver listening on port " << port_ << endl;
        cout << "📱 Waiting for ADAS data from device..." << endl;
        
        while (running_) {
            ssize_t bytes_received = recvfrom(socket_fd_, buffer, sizeof(buffer) - 1, 0,
                                             (struct sockaddr*)&client_addr, &client_len);
            
            if (bytes_received > 0) {
                buffer[bytes_received] = '\0';
                string message(buffer);
                
                char client_ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
                
                parse_message(message, client_ip);
            }
        }
    }
    
    void parse_message(const string& message, const string& client_ip) {
        vector<string> parts = split(message, ',');
        
        if (parts.empty()) return;
        
        if (parts[0] == "ADAS_DATA" && parts.size() >= 12) {
            AdasData data;
            try {
                data.timestamp = stoull(parts[1]);
                data.latitude = stod(parts[2]);
                data.longitude = stod(parts[3]);
                data.kalman_speed = stof(parts[4]);
                data.gps_speed = stof(parts[5]);
                data.accel_x = stof(parts[6]);
                data.accel_y = stof(parts[7]);
                data.accel_z = stof(parts[8]);
                data.gyro_x = stof(parts[9]);
                data.gyro_y = stof(parts[10]);
                data.gyro_z = stof(parts[11]);
                
                if (data_callback_) {
                    data_callback_(data);
                }
                
                static int packet_count = 0;
                if (++packet_count % 50 == 0) {
                    cout << "📊 [" << client_ip << "] Speed: " 
                         << (data.kalman_speed * 3.6) << " km/h | "
                         << "GPS: " << data.latitude << ", " << data.longitude 
                         << " | Accel: " << data.accel_y << " m/s²" << endl;
                }
                
            } catch (const exception& e) {
                cerr << "❌ Error parsing ADAS_DATA: " << e.what() << endl;
            }
        }
        else if (parts[0] == "ADAS_EVENT" && parts.size() >= 6) {
            AdasEvent event;
            try {
                event.event_type = parts[1];
                event.value = stof(parts[2]);
                event.latitude = stod(parts[3]);
                event.longitude = stod(parts[4]);
                event.timestamp = stoull(parts[5]);
                
                cout << endl << "🚨 EVENT DETECTED: " << event.event_type 
                     << " | Value: " << event.value 
                     << " | Location: " << event.latitude << ", " << event.longitude 
                     << endl << endl;
                
                if (event_callback_) {
                    event_callback_(event);
                }
                
            } catch (const exception& e) {
                cerr << "❌ Error parsing ADAS_EVENT: " << e.what() << endl;
            }
        }
    }
    
    vector<string> split(const string& str, char delimiter) {
        vector<string> tokens;
        stringstream ss(str);
        string token;
        
        while (getline(ss, token, delimiter)) {
            tokens.push_back(token);
        }
        
        return tokens;
    }

public:
    UDPReceiver(uint16_t port = 5555) 
        : socket_fd_(-1), port_(port), running_(false) {}
    
    ~UDPReceiver() {
        stop();
    }
    
    bool start() {
        if (running_) {
            cerr << "❌ UDP Receiver already running" << endl;
            return false;
        }
        
        socket_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (socket_fd_ < 0) {
            cerr << "❌ Failed to create UDP socket" << endl;
            return false;
        }
        
        int opt = 1;
        setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(port_);
        
        if (bind(socket_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            cerr << "❌ Failed to bind UDP socket to port " << port_ << endl;
            close(socket_fd_);
            return false;
        }
        
        running_ = true;
        receive_thread_ = thread(&UDPReceiver::receive_loop, this);
        
        return true;
    }
    
    void stop() {
        if (!running_) return;
        
        running_ = false;
        
        if (socket_fd_ >= 0) {
            close(socket_fd_);
            socket_fd_ = -1;
        }
        
        if (receive_thread_.joinable()) {
            receive_thread_.join();
        }
        
        cout << "🛑 UDP Receiver stopped" << endl;
    }
    
    void set_data_callback(function<void(const AdasData&)> callback) {
        data_callback_ = callback;
    }
    
    void set_event_callback(function<void(const AdasEvent&)> callback) {
        event_callback_ = callback;
    }
    
    bool is_running() const { return running_; }
};

#endif