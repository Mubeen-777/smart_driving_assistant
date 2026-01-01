#include <libwebsockets.h>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
#include <cstring>
#include <iostream>
#include <sstream>
#include <iomanip>
#include "udp_receiver.h"

using namespace std;

struct per_session_data {
    bool initialized;
};

queue<string> message_queue;
mutex queue_mutex;
atomic<bool> running(true);

UDPReceiver* udp_receiver = nullptr;

string adas_data_to_json(const AdasData& data) {
    ostringstream oss;
    oss << fixed << setprecision(6);
    oss << "{"
        << "\"type\":\"live_data\","
        << "\"data\":{"
        << "\"speed\":" << (data.kalman_speed * 3.6) << ","  
        << "\"acceleration\":" << data.accel_y << ","
        << "\"latitude\":" << data.latitude << ","
        << "\"longitude\":" << data.longitude << ","
        << "\"gps_speed\":" << (data.gps_speed * 3.6) << ","
        << "\"accel_x\":" << data.accel_x << ","
        << "\"accel_y\":" << data.accel_y << ","
        << "\"accel_z\":" << data.accel_z << ","
        << "\"gyro_x\":" << data.gyro_x << ","
        << "\"gyro_y\":" << data.gyro_y << ","
        << "\"gyro_z\":" << data.gyro_z << ","
        << "\"timestamp\":" << data.timestamp
        << "}}";
    return oss.str();
}

string adas_event_to_json(const AdasEvent& event) {
    ostringstream oss;
    oss << fixed << setprecision(6);
    
    string type = "warning";
    if (event.event_type == "CRASH" || event.event_type == "IMPACT") {
        type = "crash";
    }
    
    oss << "{"
        << "\"type\":\"" << type << "\","
        << "\"data\":{"
        << "\"warning_type\":\"" << event.event_type << "\","
        << "\"value\":" << event.value << ","
        << "\"latitude\":" << event.latitude << ","
        << "\"longitude\":" << event.longitude << ","
        << "\"timestamp\":" << event.timestamp
        << "}}";
    return oss.str();
}

void broadcast_message(const string& message) {
    lock_guard<mutex> lock(queue_mutex);
    message_queue.push(message);
}

static int callback_smartdrive(struct lws *wsi, enum lws_callback_reasons reason,
                               void *user, void *in, size_t len) {
    per_session_data *pss = (per_session_data*)user;
    
    switch (reason) {
        case LWS_CALLBACK_ESTABLISHED:
            cout << "✅ WebSocket client connected" << endl;
            pss->initialized = true;
            break;
            
        case LWS_CALLBACK_RECEIVE: {
            string message((char*)in, len);
            cout << "📨 Received from frontend: " << message << endl;
            
            break;
        }
        
        case LWS_CALLBACK_SERVER_WRITEABLE: {
            lock_guard<mutex> lock(queue_mutex);
            
            if (!message_queue.empty()) {
                string msg = message_queue.front();
                message_queue.pop();
                
                unsigned char buf[LWS_PRE + 4096];
                size_t msg_len = msg.length();
                
                if (msg_len < 4096) {
                    memcpy(&buf[LWS_PRE], msg.c_str(), msg_len);
                    lws_write(wsi, &buf[LWS_PRE], msg_len, LWS_WRITE_TEXT);
                }
                
                if (!message_queue.empty()) {
                    lws_callback_on_writable(wsi);
                }
            }
            break;
        }
        
        case LWS_CALLBACK_CLOSED:
            cout << "❌ WebSocket client disconnected" << endl;
            break;
            
        default:
            break;
    }
    
    return 0;
}

static struct lws_protocols protocols[] = {
    {
        "smartdrive-protocol",
        callback_smartdrive,
        sizeof(per_session_data),
        4096,
    },
    { NULL, NULL, 0, 0 }
};

void websocket_thread() {
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    
    info.port = 8081;
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;
    
    struct lws_context *context = lws_create_context(&info);
    if (!context) {
        cerr << "❌ Failed to create WebSocket context" << endl;
        return;
    }
    
    cout << "✅ WebSocket server listening on port 8081" << endl;
    
    while (running) {
        lws_service(context, 50);
        lws_callback_on_writable_all_protocol(context, &protocols[0]);
    }
    
    lws_context_destroy(context);
}

int main() {
    cout << "═══════════════════════════════════════════════" << endl;
    cout << "  SMART DRIVE WEBSOCKET BRIDGE WITH UDP" << endl;
    cout << "═══════════════════════════════════════════════" << endl;
    
    udp_receiver = new UDPReceiver(5555);
    
    udp_receiver->set_data_callback([](const AdasData& data) {
        string json = adas_data_to_json(data);
        broadcast_message(json);
    });
    
    udp_receiver->set_event_callback([](const AdasEvent& event) {
        string json = adas_event_to_json(event);
        broadcast_message(json);
        
        if (event.event_type == "CRASH" || event.event_type == "IMPACT") {
            cout << "🚨🚨🚨 CRITICAL EVENT: " << event.event_type << " 🚨🚨🚨" << endl;
        }
    });
    
    if (!udp_receiver->start()) {
        cerr << "❌ Failed to start UDP receiver" << endl;
        delete udp_receiver;
        return 1;
    }
    
    cout << endl;
    cout << "📱 Configure your Android device:" << endl;
    cout << "   1. Open MainActivity.kt" << endl;
    cout << "   2. Set UDP_SERVER_IP to your computer's IP" << endl;
    cout << "   3. Set UDP_PORT to 5555" << endl;
    cout << endl;
    
    thread ws_thread(websocket_thread);
    
    cout << "✅ System ready. Press Ctrl+C to stop." << endl;
    
    ws_thread.join();
    
    running = false;
    udp_receiver->stop();
    delete udp_receiver;
    
    cout << "👋 Shutdown complete" << endl;
    
    return 0;
}