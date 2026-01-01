
#include <websocketpp/config/asio.hpp>
#include <websocketpp/server.hpp>
#include <nlohmann/json.hpp>
#include <iostream>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <opencv2/opencv.hpp>
#include <chrono>
#include <functional>
#include <memory>
#include <vector>
#include <cstdint>
#include <string>
#include <iomanip>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>
#include <curl/curl.h>

#include "camera.h"
#include "lane_detector.h"

using namespace std;
using json = nlohmann::json;
using server = websocketpp::server<websocketpp::config::asio>;

static const std::string base64_chars = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

std::string base64_encode(unsigned char const* bytes_to_encode, size_t in_len) {
    std::string ret;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];

    while (in_len--) {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; i < 4; i++)
                ret += base64_chars[char_array_4[i]];
            i = 0;
        }
    }

    if (i) {
        for (j = i; j < 3; j++)
            char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (j = 0; j < i + 1; j++)
            ret += base64_chars[char_array_4[j]];

        while (i++ < 3)
            ret += '=';
    }

    return ret;
}

class HTTPClient {
private:
    string main_server_url;
    
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, string* userp) {
        size_t totalSize = size * nmemb;
        userp->append((char*)contents, totalSize);
        return totalSize;
    }
    
public:
    HTTPClient(const string& url = "http://localhost:8080") : main_server_url(url) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    }
    
    ~HTTPClient() {
        curl_global_cleanup();
    }
    
    json send_request(const string& operation, const json& data) {
        CURL* curl = curl_easy_init();
        if (!curl) {
            cerr << "Failed to initialize CURL" << endl;
            return {{"status", "error"}, {"message", "CURL initialization failed"}};
        }
        
        json request_data = data;
        request_data["operation"] = operation;
        string request_body = request_data.dump();
        
        string response_string;
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        
        curl_easy_setopt(curl, CURLOPT_URL, main_server_url.c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, request_body.length());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
        
        CURLcode res = curl_easy_perform(curl);
        
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        
        if (res != CURLE_OK) {
            cerr << "HTTP request failed: " << curl_easy_strerror(res) << endl;
            return {{"status", "error"}, {"message", "HTTP request failed"}};
        }
        
        try {
            return json::parse(response_string);
        } catch (const exception& e) {
            cerr << "Failed to parse JSON response: " << e.what() << endl;
            return {{"status", "error"}, {"message", "Invalid JSON response"}};
        }
    }
};

class SmartDriveBridge {
private:
    server ws_server;
    thread server_thread;
    atomic<bool> running;
    

    unique_ptr<CameraManager> camera;
    unique_ptr<UltraFastLaneDetector> lane_detector;
    thread camera_thread;
    atomic<bool> camera_running;
    

    int udp_socket;
    thread udp_receiver_thread;
    atomic<bool> udp_receiver_running;
    const int UDP_PORT = 5555;
    

    unique_ptr<HTTPClient> http_client;
    string current_session_id;
    

    using connection_hdl = websocketpp::connection_hdl;
    struct connection_data {
        json info;
        time_t connected_at;
        string session_id;
    };
    
    map<connection_hdl, connection_data, owner_less<connection_hdl>> clients;
    mutex clients_mutex;
    

    struct MobileData {
        atomic<double> speed{0};
        atomic<double> acceleration{0};
        atomic<double> latitude{31.5204};
        atomic<double> longitude{74.3587};
        atomic<double> altitude{0};
        atomic<double> accuracy{10.0};
        atomic<double> accel_x{0};
        atomic<double> accel_y{0};
        atomic<double> accel_z{0};
        atomic<double> gyro_x{0};
        atomic<double> gyro_y{0};
        atomic<double> gyro_z{0};
        atomic<uint64_t> timestamp{0};
        atomic<bool> connected{false};
        atomic<int> packet_count{0};
    } mobile_data;
    

    struct SafetyData {
        atomic<double> safety_score{1000};
        atomic<int> rapid_accel_count{0};
        atomic<int> hard_brake_count{0};
        atomic<int> impact_count{0};
        atomic<int> lane_departures{0};
        
        mutex lane_status_mutex;
        string lane_status = "CENTERED";
        
        void set_lane_status(const string& status) {
            lock_guard<mutex> lock(lane_status_mutex);
            lane_status = status;
        }
        
        string get_lane_status() {
            lock_guard<mutex> lock(lane_status_mutex);
            return lane_status;
        }
    } safety_data;
    

    struct TripData {
        atomic<bool> active{false};
        atomic<uint64_t> trip_id{0};
        atomic<uint64_t> start_time{0};
        atomic<double> start_lat{0};
        atomic<double> start_lon{0};
        atomic<uint64_t> driver_id{0};
        atomic<uint64_t> vehicle_id{0};
    } trip_data;
    

    atomic<int> total_frames{0};
    atomic<int> total_udp_packets{0};
    

    chrono::steady_clock::time_point last_broadcast;
    chrono::steady_clock::time_point last_gps_log;
    
public:
    SmartDriveBridge() : 
        running(false), 
        camera_running(false),
        udp_socket(-1),
        udp_receiver_running(false),
        http_client(nullptr) {
        

        ws_server.init_asio();
        ws_server.set_reuse_addr(true);
        
        last_broadcast = chrono::steady_clock::now();
        last_gps_log = chrono::steady_clock::now();
        

        ws_server.set_open_handler(bind(&SmartDriveBridge::on_open, this, placeholders::_1));
        ws_server.set_close_handler(bind(&SmartDriveBridge::on_close, this, placeholders::_1));
        ws_server.set_message_handler(bind(&SmartDriveBridge::on_message, this, placeholders::_1, placeholders::_2));
        
        cout << "SmartDriveBridge initialized" << endl;
        cout << "  - WebSocket: localhost:8081" << endl;
        cout << "  - UDP Receiver: port 5555" << endl;
        cout << "  - Main Server: http://localhost:8080" << endl;
    }
    
    ~SmartDriveBridge() {
        stop();
    }
    
    bool initialize() {
        cout << "=== Smart Drive Bridge Initialization ===" << endl;
        
        try {

            http_client = make_unique<HTTPClient>("http://localhost:8080");
            

            cout << "Initializing camera system..." << endl;
            camera = make_unique<CameraManager>();
            CameraManager::CameraConfig cam_cfg;
            cam_cfg.source = CameraManager::findDroidCamDevice();
            cam_cfg.width = 640;
            cam_cfg.height = 480;
            cam_cfg.fps = 30;
            cam_cfg.type = CameraManager::CAMERA_V4L2;
            
            if (!camera->initialize(cam_cfg)) {
                cerr << "Failed to initialize camera" << endl;
                camera.reset(nullptr);
            } else {
                cout << "✓ Camera initialized: " << cam_cfg.source << endl;
            }
            

            if (camera) {
                lane_detector = make_unique<UltraFastLaneDetector>();
                if (!lane_detector->initialize("")) {
                    cerr << "Failed to initialize lane detector" << endl;
                    lane_detector.reset(nullptr);
                } else {
                    cout << "✓ Lane detector initialized" << endl;
                }
            }
            
            cout << "✓ Bridge initialized successfully" << endl;
            cout << "  Note: All database operations will go through main server" << endl;
            return true;
            
        } catch (const exception& e) {
            cerr << "Initialization error: " << e.what() << endl;
            return false;
        }
    }
    
    void start() {
        if (running) return;
        
        try {
            running = true;
            

            ws_server.listen(8081);
            ws_server.start_accept();
            
            server_thread = thread([this]() {
                cout << "WebSocket server starting on port 8081" << endl;
                try {
                    ws_server.run();
                } catch (const exception& e) {
                    cerr << "WebSocket server error: " << e.what() << endl;
                }
            });
            

            if (camera) {
                camera_running = true;
                camera_thread = thread(&SmartDriveBridge::camera_loop, this);
                cout << "✓ Camera processing started" << endl;
            }
            

            if (setup_udp_receiver()) {
                udp_receiver_running = true;
                udp_receiver_thread = thread(&SmartDriveBridge::udp_receiver_loop, this);
                cout << "✓ UDP receiver started on port 5555" << endl;
            }
            

            thread broadcast_thread(&SmartDriveBridge::broadcast_loop, this);
            broadcast_thread.detach();
            

            thread gps_log_thread(&SmartDriveBridge::gps_logging_loop, this);
            gps_log_thread.detach();
            
            cout << endl << "✅ Bridge server started successfully!" << endl;
            cout << "   Frontend: ws://localhost:8081" << endl;
            cout << "   Mobile UDP: port 5555" << endl;
            cout << "   Camera: " << (camera ? "Active" : "Inactive") << endl;
            cout << "   Main Server: http://localhost:8080" << endl;
            cout << endl;
            
        } catch (const exception& e) {
            cerr << "Failed to start bridge: " << e.what() << endl;
            running = false;
        }
    }
    
    void stop() {
        if (!running) return;
        
        running = false;
        camera_running = false;
        udp_receiver_running = false;
        

        if (udp_socket >= 0) {
            close(udp_socket);
            udp_socket = -1;
        }
        

        if (udp_receiver_thread.joinable()) {
            udp_receiver_thread.join();
        }
        
        if (camera_thread.joinable()) {
            camera_thread.join();
        }
        

        try {
            ws_server.stop_listening();
            
            {
                lock_guard<mutex> lock(clients_mutex);
                for (auto& client : clients) {
                    try {
                        ws_server.close(client.first, websocketpp::close::status::going_away, "Server shutdown");
                    } catch (...) {}
                }
                clients.clear();
            }
            
            ws_server.stop();
            
            if (server_thread.joinable()) {
                server_thread.join();
            }
        } catch (const exception& e) {
            cerr << "Error stopping WebSocket server: " << e.what() << endl;
        }
        

        if (camera) {
            camera->release();
        }
        
        cout << "Bridge stopped" << endl;
    }
    
    bool is_running() const { return running; }
    
private:

    bool setup_udp_receiver() {
        udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
        if (udp_socket < 0) {
            cerr << "Failed to create UDP socket" << endl;
            return false;
        }
        
        int opt = 1;
        setsockopt(udp_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(UDP_PORT);
        
        if (bind(udp_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            cerr << "Failed to bind UDP socket to port " << UDP_PORT << endl;
            close(udp_socket);
            return false;
        }
        
        return true;
    }
    

    void udp_receiver_loop() {
        char buffer[2048];
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        struct pollfd pfd;
        pfd.fd = udp_socket;
        pfd.events = POLLIN;
        
        cout << "📱 Waiting for mobile data on UDP port " << UDP_PORT << "..." << endl;
        
        while (udp_receiver_running) {
            int ret = poll(&pfd, 1, 100);
            
            if (ret < 0) {
                if (errno != EINTR) {
                    cerr << "UDP poll error: " << strerror(errno) << endl;
                }
                continue;
            }
            
            if (ret == 0) continue;
            
            ssize_t bytes_received = recvfrom(udp_socket, buffer, sizeof(buffer) - 1, 0,
                                             (struct sockaddr*)&client_addr, &client_len);
            
            if (bytes_received > 0) {
                buffer[bytes_received] = '\0';
                process_udp_packet(string(buffer, bytes_received), &client_addr);
            }
        }
    }
    
    void process_udp_packet(const string& packet, struct sockaddr_in* client_addr) {
        total_udp_packets++;
        mobile_data.packet_count++;
        mobile_data.connected = true;
        
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr->sin_addr), client_ip, INET_ADDRSTRLEN);
        

        vector<string> parts;
        stringstream ss(packet);
        string part;
        
        while (getline(ss, part, ',')) {
            parts.push_back(part);
        }
        
        if (parts.empty()) return;
        
        try {
            if (parts[0] == "ADAS_DATA" && parts.size() >= 12) {

                mobile_data.timestamp = stoull(parts[1]);
                mobile_data.latitude = stod(parts[2]);
                mobile_data.longitude = stod(parts[3]);
                
                double kalman_speed = stof(parts[4]);
                mobile_data.speed = kalman_speed * 3.6;
                
                mobile_data.accel_x = stof(parts[6]);
                mobile_data.accel_y = stof(parts[7]);
                mobile_data.accel_z = stof(parts[8]);
                mobile_data.gyro_x = stof(parts[9]);
                mobile_data.gyro_y = stof(parts[10]);
                mobile_data.gyro_z = stof(parts[11]);
                

                double current_accel = mobile_data.accel_y.load();
                mobile_data.acceleration = current_accel;
                

                detect_harsh_events(current_accel);
                

                update_safety_score();
                

                static int log_count = 0;
                if (++log_count % 100 == 0) {
                    cout << "📱 Mobile [" << client_ip << "]: " 
                         << fixed << setprecision(1)
                         << "Speed=" << mobile_data.speed.load() << " km/h, "
                         << "Accel=" << mobile_data.accel_y.load() << " m/s², "
                         << "Location=" << fixed << setprecision(6)
                         << mobile_data.latitude.load() << ", "
                         << mobile_data.longitude.load() << endl;
                }
                
            } else if (parts[0] == "ADAS_EVENT" && parts.size() >= 6) {

                string event_type = parts[1];
                float event_value = stof(parts[2]);
                double event_lat = stod(parts[3]);
                double event_lon = stod(parts[4]);
                uint64_t event_time = stoull(parts[5]);
                
                handle_mobile_event(event_type, event_value, event_lat, event_lon);
                
                cout << endl << "🚨 MOBILE EVENT [" << client_ip << "]: " 
                     << event_type << " (" << event_value << ")" << endl
                     << "   Location: " << event_lat << ", " << event_lon << endl << endl;
            }
        } catch (const exception& e) {
            cerr << "Error parsing UDP packet: " << e.what() << endl;
        }
    }
    
    void handle_mobile_event(const string& event_type, float value, double lat, double lon) {
        if (event_type == "HARD_BRAKE") {
            safety_data.hard_brake_count++;
            broadcast_warning("hard_braking", value, lat, lon);
            

            if (trip_data.active.load() && http_client && !current_session_id.empty()) {
                json request_data = {
                    {"session_id", current_session_id},
                    {"event_type", "hard_braking"},
                    {"description", "Hard braking detected by mobile app"},
                    {"point_deduction", 10},
                    {"trip_id", trip_data.trip_id.load()}
                };
                
                auto response = http_client->send_request("driver_report_event", request_data);
                if (response.value("status", "") == "success") {
                    cout << "Hard braking event reported to main server" << endl;
                }
            }
            
        } else if (event_type == "RAPID_ACCEL") {
            safety_data.rapid_accel_count++;
            broadcast_warning("rapid_acceleration", value, lat, lon);
            
        } else if (event_type == "CRASH" || event_type == "IMPACT") {
            safety_data.impact_count++;
            broadcast_warning("impact", value, lat, lon);
            

            if (http_client && !current_session_id.empty()) {
                json request_data = {
                    {"session_id", current_session_id},
                    {"vehicle_id", trip_data.vehicle_id.load()},
                    {"type", "0"},
                    {"latitude", lat},
                    {"longitude", lon},
                    {"description", "Impact/Crash detected by mobile app"}
                };
                
                auto response = http_client->send_request("incident_report", request_data);
                if (response.value("status", "") == "success") {
                    cout << "🚨 Impact incident reported to main server" << endl;
                }
            }
        }
        
        update_safety_score();
    }
    
    void detect_harsh_events(double current_accel) {
        static double last_accel = 0;
        static auto last_time = chrono::steady_clock::now();
        
        auto now = chrono::steady_clock::now();
        double time_diff = chrono::duration<double>(now - last_time).count();
        
        if (time_diff > 0.1) {
            double jerk = (current_accel - last_accel) / time_diff;
            last_accel = current_accel;
            last_time = now;
            

            if (current_accel < -4.0) {
                safety_data.hard_brake_count++;
                broadcast_warning("hard_braking", current_accel, 
                                 mobile_data.latitude.load(), 
                                 mobile_data.longitude.load());
            }

            else if (current_accel > 3.0) {
                safety_data.rapid_accel_count++;
                broadcast_warning("rapid_acceleration", current_accel,
                                 mobile_data.latitude.load(),
                                 mobile_data.longitude.load());
            }
        }
    }
    
    void update_safety_score() {
        double score = 1000.0;
        score -= safety_data.rapid_accel_count.load() * 5.0;
        score -= safety_data.hard_brake_count.load() * 10.0;
        score -= safety_data.lane_departures.load() * 3.0;
        score -= safety_data.impact_count.load() * 50.0;
        
        safety_data.safety_score = max(0.0, min(1000.0, score));
    }
    

    void camera_loop() {
        cv::Mat frame;
        int frame_count = 0;
        auto last_fps_time = chrono::steady_clock::now();
        auto last_warning_time = chrono::steady_clock::now();
        double current_fps = 0;
        const auto WARNING_COOLDOWN = chrono::milliseconds(500);
        
        while (camera_running.load()) {
            try {
                if (camera && camera->grabFrame(frame) && !frame.empty()) {
                    frame_count++;
                    total_frames++;
                    

                    auto now = chrono::steady_clock::now();
                    auto fps_elapsed = chrono::duration_cast<chrono::milliseconds>(now - last_fps_time).count();
                    if (fps_elapsed >= 1000) {
                        current_fps = total_frames.load() * 1000.0 / fps_elapsed;
                        total_frames = 0;
                        last_fps_time = now;
                    }
                    

                    process_camera_frame(frame, current_fps, last_warning_time, WARNING_COOLDOWN);
                    

                    send_video_frame(frame);
                    
                    this_thread::sleep_for(chrono::milliseconds(10));
                } else {
                    this_thread::sleep_for(chrono::milliseconds(33));
                }
            } catch (const exception& e) {
                cerr << "Camera loop error: " << e.what() << endl;
                this_thread::sleep_for(chrono::milliseconds(100));
            }
        }
    }
    
    void process_camera_frame(cv::Mat& frame, double fps, 
                              chrono::steady_clock::time_point& last_warning_time,
                              const chrono::milliseconds& warning_cooldown) {

        cv::putText(frame, "FPS: " + to_string((int)fps),
                    cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 0.7,
                    cv::Scalar(0, 255, 0), 2);
        
        string mobile_status = mobile_data.connected.load() ? "Mobile: Connected" : "Mobile: Disconnected";
        cv::Scalar mobile_color = mobile_data.connected.load() ? cv::Scalar(0, 255, 0) : cv::Scalar(0, 0, 255);
        cv::putText(frame, mobile_status, cv::Point(10, 60),
                    cv::FONT_HERSHEY_SIMPLEX, 0.6, mobile_color, 2);
        
        string trip_status = trip_data.active.load() ? 
                           "Trip: Active #" + to_string(trip_data.trip_id.load()) : 
                           "Trip: Inactive";
        cv::Scalar trip_color = trip_data.active.load() ? cv::Scalar(0, 255, 0) : cv::Scalar(0, 0, 255);
        cv::putText(frame, trip_status, cv::Point(10, 90),
                    cv::FONT_HERSHEY_SIMPLEX, 0.6, trip_color, 2);
        

        if (lane_detector) {
            try {
                auto result = lane_detector->detectLanes(frame);
                lane_detector->drawLanes(frame, result, true);
                

                string direction;
                double deviation;
                bool departure = lane_detector->checkLaneDeparture(result, frame, direction, deviation);
                
                if (departure) {
                    safety_data.lane_departures++;
                    safety_data.set_lane_status(direction);
                    lane_detector->drawDepartureWarning(frame, direction, deviation, true);
                    

                    auto now = chrono::steady_clock::now();
                    if (chrono::duration_cast<chrono::milliseconds>(now - last_warning_time) >= warning_cooldown) {
                        last_warning_time = now;
                        
                        json warning_data = {
                            {"direction", direction},
                            {"deviation", deviation},
                            {"count", safety_data.lane_departures.load()},
                            {"timestamp", time(nullptr)}
                        };
                        
                        json warning_msg = {
                            {"type", "lane_warning"},
                            {"data", warning_data}
                        };
                        
                        broadcast_message(warning_msg.dump());
                    }
                } else {
                    safety_data.set_lane_status("CENTERED");
                }
            } catch (const exception& e) {
                cerr << "Lane detection error: " << e.what() << endl;
            }
        }
    }
    
    void send_video_frame(const cv::Mat& frame) {
        vector<uchar> buffer;
        cv::imencode(".jpg", frame, buffer, {cv::IMWRITE_JPEG_QUALITY, 70});
        
        if (!buffer.empty()) {
            string base64_frame = base64_encode(buffer.data(), buffer.size());
            
            json video_msg = {
                {"type", "video_frame"},
                {"data", base64_frame},
                {"timestamp", time(nullptr)}
            };
            
            broadcast_message(video_msg.dump());
        }
    }
    

    void gps_logging_loop() {
        while (running) {
            try {
                if (trip_data.active.load() && mobile_data.connected.load() && 
                    !current_session_id.empty() && http_client) {
                    
                    auto now = chrono::steady_clock::now();
                    auto elapsed = chrono::duration_cast<chrono::seconds>(now - last_gps_log).count();
                    

                    if (elapsed >= 10) {
                        last_gps_log = now;
                        
                        json request_data = {
                            {"session_id", current_session_id},
                            {"trip_id", trip_data.trip_id.load()},
                            {"latitude", mobile_data.latitude.load()},
                            {"longitude", mobile_data.longitude.load()},
                            {"speed", mobile_data.speed.load()},
                            {"acceleration", mobile_data.acceleration.load()}
                        };
                        
                        auto response = http_client->send_request("trip_log_gps", request_data);
                        if (response.value("status", "") != "success") {
                            cerr << "Failed to log GPS point to main server" << endl;
                        }
                    }
                }
                
                this_thread::sleep_for(chrono::seconds(1));
            } catch (const exception& e) {
                cerr << "GPS logging error: " << e.what() << endl;
                this_thread::sleep_for(chrono::seconds(5));
            }
        }
    }
    

    void broadcast_loop() {
        const auto BROADCAST_INTERVAL = chrono::milliseconds(1000);
        
        while (running) {
            try {
                broadcast_live_data();
                this_thread::sleep_for(BROADCAST_INTERVAL);
            } catch (const exception& e) {
                cerr << "Broadcast error: " << e.what() << endl;
                this_thread::sleep_for(chrono::milliseconds(500));
            }
        }
    }
    
    void broadcast_live_data() {
        json live_data = {
            {"speed", mobile_data.speed.load()},
            {"acceleration", mobile_data.accel_y.load()},
            {"latitude", mobile_data.latitude.load()},
            {"longitude", mobile_data.longitude.load()},
            {"accuracy", mobile_data.accuracy.load()},
            {"safety_score", safety_data.safety_score.load()},
            {"lane_status", safety_data.get_lane_status()},
            {"rapid_accel_count", safety_data.rapid_accel_count.load()},
            {"hard_brake_count", safety_data.hard_brake_count.load()},
            {"lane_departures", safety_data.lane_departures.load()},
            {"impact_count", safety_data.impact_count.load()},
            {"trip_active", trip_data.active.load()},
            {"trip_id", trip_data.trip_id.load()},
            {"mobile_connected", mobile_data.connected.load()},
            {"timestamp", time(nullptr)},
            {"source", "mobile"}
        };
        
        json data_msg = {
            {"type", "live_data"},
            {"data", live_data}
        };
        
        broadcast_message(data_msg.dump());
    }
    
    void broadcast_warning(const string& warning_type, double value, double lat, double lon) {
        json warning_data = {
            {"warning_type", warning_type},
            {"value", value},
            {"latitude", lat},
            {"longitude", lon},
            {"timestamp", time(nullptr)},
            {"trip_active", trip_data.active.load()},
            {"trip_id", trip_data.trip_id.load()}
        };
        
        json warning_msg = {
            {"type", "warning"},
            {"data", warning_data}
        };
        
        broadcast_message(warning_msg.dump());
    }
    

    void on_open(connection_hdl hdl) {
        lock_guard<mutex> lock(clients_mutex);
        
        connection_data data;
        data.connected_at = time(nullptr);
        data.info = {
            {"type", "dashboard"},
            {"connected_at", data.connected_at}
        };
        
        clients[hdl] = data;
        
        cout << "New WebSocket client connected. Total: " << clients.size() << endl;
        
        send_initial_data(hdl);
    }
    
    void on_close(connection_hdl hdl) {
        lock_guard<mutex> lock(clients_mutex);
        clients.erase(hdl);
        cout << "WebSocket client disconnected. Remaining: " << clients.size() << endl;
    }
    
    void on_message(connection_hdl hdl, server::message_ptr msg) {
        try {
            string payload = msg->get_payload();
            if (payload.empty()) return;
            
            json data = json::parse(payload);
            string cmd = data.value("command", "");
            string type = data.value("type", "");
            
            if (cmd == "start_trip") {
                handle_start_trip(hdl, data);
            } else if (cmd == "stop_trip") {
                handle_stop_trip(hdl, data);
            } else if (cmd == "toggle_camera") {
                handle_toggle_camera(hdl, data);
            } else if (cmd == "get_stats") {
                handle_get_stats(hdl, data);
            } else if (cmd == "get_camera_status") {
                handle_get_camera_status(hdl, data);
            } else if (cmd == "check_droidcam") {
                handle_check_droidcam(hdl, data);
            } else if (cmd == "ping") {
                handle_ping(hdl, data);
            } else if (cmd == "auth") {
                handle_auth(hdl, data);
            } else if (type == "auth") {
                handle_auth(hdl, data);
            }
        } catch (const exception& e) {
            cerr << "WebSocket message error: " << e.what() << endl;
        }
    }
    
    void handle_auth(connection_hdl hdl, const json& data) {
        current_session_id = data.value("session_id", "");
        cout << "Client authenticated with session: " << current_session_id << endl;
        
        json response = {
            {"type", "auth_response"},
            {"data", {
                {"authenticated", !current_session_id.empty()},
                {"timestamp", time(nullptr)}
            }}
        };
        
        send_message(hdl, response.dump());
    }
    
    void send_initial_data(connection_hdl hdl) {
        json init_data = {
            {"type", "init"},
            {"data", {
                {"server_version", "2.2"},
                {"camera_available", camera != nullptr},
                {"lane_detection", lane_detector != nullptr},
                {"mobile_connected", mobile_data.connected.load()},
                {"trip_active", trip_data.active.load()},
                {"timestamp", time(nullptr)},
                {"requires_auth", true}
            }}
        };
        
        send_message(hdl, init_data.dump());
    }
    
    void handle_start_trip(connection_hdl hdl, const json& data) {
        if (!http_client) {
            send_error(hdl, "HTTP client not initialized");
            return;
        }
        
        if (current_session_id.empty()) {
            send_error(hdl, "Not authenticated. Please login first");
            return;
        }
        
        uint64_t driver_id = data.value("driver_id", 0ULL);
        uint64_t vehicle_id = data.value("vehicle_id", 0ULL);
        double start_lat = data.value("latitude", mobile_data.latitude.load());
        double start_lon = data.value("longitude", mobile_data.longitude.load());
        string address = data.value("address", "");
        

        json request_data = {
            {"session_id", current_session_id},
            {"driver_id", driver_id},
            {"vehicle_id", vehicle_id},
            {"latitude", start_lat},
            {"longitude", start_lon},
            {"address", address}
        };
        
        auto response = http_client->send_request("trip_start", request_data);
        
        if (response.value("status", "") == "success") {
            uint64_t trip_id = response["data"].value("trip_id", 0ULL);
            
            trip_data.active = true;
            trip_data.trip_id = trip_id;
            trip_data.start_time = time(nullptr);
            trip_data.start_lat = start_lat;
            trip_data.start_lon = start_lon;
            trip_data.driver_id = driver_id;
            trip_data.vehicle_id = vehicle_id;
            
            json ws_response = {
                {"type", "trip_started"},
                {"data", {
                    {"trip_id", trip_id},
                    {"start_time", trip_data.start_time.load()},
                    {"driver_id", driver_id},
                    {"vehicle_id", vehicle_id},
                    {"start_latitude", start_lat},
                    {"start_longitude", start_lon},
                    {"status", "active"}
                }}
            };
            
            send_message(hdl, ws_response.dump());
            broadcast_live_data();
            
            cout << "✅ Trip started via main server: " << trip_id << endl;
        } else {
            send_error(hdl, response.value("message", "Failed to start trip"));
        }
    }
    
    void handle_stop_trip(connection_hdl hdl, const json& data) {
        if (!http_client) {
            send_error(hdl, "HTTP client not initialized");
            return;
        }
        
        if (current_session_id.empty()) {
            send_error(hdl, "Not authenticated");
            return;
        }
        
        uint64_t trip_id = data.value("trip_id", trip_data.trip_id.load());
        double end_lat = data.value("latitude", mobile_data.latitude.load());
        double end_lon = data.value("longitude", mobile_data.longitude.load());
        string address = data.value("address", "");
        

        json request_data = {
            {"session_id", current_session_id},
            {"trip_id", trip_id},
            {"latitude", end_lat},
            {"longitude", end_lon},
            {"address", address}
        };
        
        auto response = http_client->send_request("trip_end", request_data);
        
        if (response.value("status", "") == "success") {
            trip_data.active = false;
            trip_data.trip_id = 0;
            trip_data.driver_id = 0;
            trip_data.vehicle_id = 0;
            
            json ws_response = {
                {"type", "trip_stopped"},
                {"data", {
                    {"trip_id", trip_id},
                    {"end_time", time(nullptr)},
                    {"end_latitude", end_lat},
                    {"end_longitude", end_lon},
                    {"status", "completed"}
                }}
            };
            
            send_message(hdl, ws_response.dump());
            broadcast_live_data();
            
            cout << "🛑 Trip stopped via main server: " << trip_id << endl;
        } else {
            send_error(hdl, response.value("message", "Failed to stop trip"));
        }
    }
    
    void handle_toggle_camera(connection_hdl hdl, const json& data) {
        bool enable = data.value("enable", !camera_running.load());
        
        if (enable && !camera_running.load()) {
            if (camera) {
                if (camera_thread.joinable()) {
                    camera_thread.join();
                }
                
                camera_running = true;
                camera_thread = thread(&SmartDriveBridge::camera_loop, this);
            }
        } else if (!enable && camera_running.load()) {
            camera_running = false;
            if (camera_thread.joinable()) {
                camera_thread.join();
            }
        }
        
        json response = {
            {"type", "camera_status"},
            {"data", {
                {"enabled", camera_running.load()},
                {"available", camera != nullptr}
            }}
        };
        
        send_message(hdl, response.dump());
    }
    
    void handle_get_stats(connection_hdl hdl, const json& data) {
        json stats = {
            {"speed", mobile_data.speed.load()},
            {"acceleration", mobile_data.accel_y.load()},
            {"latitude", mobile_data.latitude.load()},
            {"longitude", mobile_data.longitude.load()},
            {"safety_score", safety_data.safety_score.load()},
            {"lane_status", safety_data.get_lane_status()},
            {"rapid_accel_count", safety_data.rapid_accel_count.load()},
            {"hard_brake_count", safety_data.hard_brake_count.load()},
            {"lane_departures", safety_data.lane_departures.load()},
            {"impact_count", safety_data.impact_count.load()},
            {"trip_active", trip_data.active.load()},
            {"trip_id", trip_data.trip_id.load()},
            {"mobile_connected", mobile_data.connected.load()},
            {"mobile_packets", mobile_data.packet_count.load()},
            {"total_frames", total_frames.load()},
            {"total_udp_packets", total_udp_packets.load()}
        };
        
        json response = {
            {"type", "stats_response"},
            {"data", stats}
        };
        
        send_message(hdl, response.dump());
    }
    
    void handle_get_camera_status(connection_hdl hdl, const json& data) {
        json status;
        
        if (camera) {
            status["available"] = true;
            status["opened"] = camera->isOpened();
            status["fps"] = camera->getCurrentFPS();
            auto size = camera->getFrameSize();
            status["resolution"] = to_string(size.width) + "x" + to_string(size.height);
            status["source"] = "DroidCam (USB)";
        } else {
            status["available"] = false;
        }
        
        json response = {
            {"type", "camera_status_response"},
            {"data", status}
        };
        
        send_message(hdl, response.dump());
    }
    
    void handle_check_droidcam(connection_hdl hdl, const json& data) {
        string droidcam_device = CameraManager::findDroidCamDevice();
        
        json droidcam_info = {
            {"found", droidcam_device != ""},
            {"device", droidcam_device},
            {"status", droidcam_device != "" ? "connected" : "not_found"}
        };
        
        if (camera && camera->isOpened()) {
            auto size = camera->getFrameSize();
            droidcam_info["resolution"] = to_string(size.width) + "x" + to_string(size.height);
        }
        
        json response = {
            {"type", "droidcam_status"},
            {"data", droidcam_info}
        };
        
        send_message(hdl, response.dump());
    }
    
    void handle_ping(connection_hdl hdl, const json& data) {
        json response = {
            {"type", "pong"},
            {"timestamp", time(nullptr)}
        };
        
        send_message(hdl, response.dump());
    }
    
    void send_error(connection_hdl hdl, const string& message) {
        json response = {
            {"type", "error"},
            {"message", message},
            {"timestamp", time(nullptr)}
        };
        
        send_message(hdl, response.dump());
    }
    
    void send_message(connection_hdl hdl, const string& msg) {
        try {
            ws_server.send(hdl, msg, websocketpp::frame::opcode::text);
        } catch (const exception& e) {
            cerr << "Error sending message: " << e.what() << endl;
            lock_guard<mutex> lock(clients_mutex);
            clients.erase(hdl);
        }
    }
    
    void broadcast_message(const string& msg) {
        lock_guard<mutex> lock(clients_mutex);
        vector<connection_hdl> to_remove;
        
        for (const auto& client : clients) {
            try {
                ws_server.send(client.first, msg, websocketpp::frame::opcode::text);
            } catch (...) {
                to_remove.push_back(client.first);
            }
        }
        

        for (const auto& hdl : to_remove) {
            clients.erase(hdl);
        }
    }
};

int main(int argc, char** argv) {
    cout << "========================================" << endl;
    cout << "  Smart Drive Unified Bridge v2.3" << endl;
    cout << "  Camera + Mobile UDP + WebSocket" << endl;
    cout << "  Using Main Server HTTP API" << endl;
    cout << "========================================" << endl;
    
    SmartDriveBridge bridge;
    
    if (!bridge.initialize()) {
        cerr << "Failed to initialize bridge. Exiting..." << endl;
        return 1;
    }
    
    cout << endl << "Starting bridge server..." << endl;
    cout << "  WebSocket: ws://localhost:8081" << endl;
    cout << "  UDP Mobile: port 5555" << endl;
    cout << "  Main Server: http://localhost:8080" << endl;
    cout << "  All database operations go through main server" << endl;
    cout << endl;
    
    bridge.start();
    

    string command;
    while (bridge.is_running()) {
        cout << "> ";
        getline(cin, command);
        
        if (command == "stop" || command == "exit") {
            break;
        } else if (command == "status") {
            cout << "Bridge is running" << endl;
        } else if (command == "help") {
            cout << "Commands: stop, exit, status, help" << endl;
        } else if (!command.empty()) {
            cout << "Unknown command. Type 'help' for available commands." << endl;
        }
    }
    
    cout << "Stopping bridge..." << endl;
    bridge.stop();
    
    cout << "Bridge stopped successfully." << endl;
    return 0;
}