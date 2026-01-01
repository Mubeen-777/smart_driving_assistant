#ifndef SDMSERVER_H
#define SDMSERVER_H

#include "../../include/sdm_types.hpp"
#include "../../include/sdm_config.hpp"

#include "../../source/data_structures/CircularQueue.h"

#include "../../source/core/DatabaseManager.h"
#include "../../source/core/CacheManager.h"
#include "../../source/core/IndexManager.h"
#include "../../source/core/SessionManager.h"
#include "../../source/core/SecurityManager.h"

#include "../../source/core/TripManager.h"
#include "../../source/core/VehicleManager.h"
#include "../../source/core/ExpenseManager.h"
#include "../../source/core/DriverManager.h"
#include "../../source/core/IncidentManager.h"

#include "RequestHandler.h"
#include "ResponseBuilder.h"

#include <thread>
#include <vector>
#include <atomic>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
using namespace std;

class SDMServer
{
private:
    SDMConfig config_;
    atomic<bool> running_;

    int server_socket_;
    struct sockaddr_in server_addr_;

    RequestQueue request_queue_;
    vector<thread> worker_threads_;
    thread listener_thread_;

    DatabaseManager *db_manager_;
    CacheManager *cache_manager_;
    IndexManager *index_manager_;
    SecurityManager *security_manager_;
    SessionManager *session_manager_;

    TripManager *trip_manager_;
    VehicleManager *vehicle_manager_;
    ExpenseManager *expense_manager_;
    DriverManager *driver_manager_;
    IncidentManager *incident_manager_;

    RequestHandler *request_handler_;

    atomic<uint64_t> total_requests_;
    atomic<uint64_t> total_errors_;
    atomic<uint64_t> rejected_requests_;

    uint64_t start_time_;

public:
    SDMServer(const SDMConfig &config)
        : config_(config), running_(false), server_socket_(-1),
          request_queue_(config.queue_capacity),
          db_manager_(nullptr), cache_manager_(nullptr),
          index_manager_(nullptr), security_manager_(nullptr),
          session_manager_(nullptr), trip_manager_(nullptr),
          vehicle_manager_(nullptr), expense_manager_(nullptr),
          driver_manager_(nullptr),
          incident_manager_(nullptr),
          request_handler_(nullptr),
          total_requests_(0), total_errors_(0), rejected_requests_(0)
    {

        start_time_ = get_current_timestamp();
    }

    ~SDMServer()
    {
        stop();
        cleanup();
    }

    bool initialize()
    {
        cout << "=== Smart Drive Manager Server ===" << endl;
        cout << "Initializing server..." << endl;

        cout << "  [1/9] Initializing database..." << endl;
        db_manager_ = new DatabaseManager(config_.database_path);
        if (!db_manager_->open())
        {
            cerr << "    Failed to open database. Creating new..." << endl;
            if (!db_manager_->create(config_))
            {
                cerr << "    ERROR: Failed to create database!" << endl;
                return false;
            }
            if (!db_manager_->open())
            {
                cerr << "    ERROR: Failed to open newly created database!" << endl;
                return false;
            }
        }
        cout << "    ✓ Database initialized" << endl;

        cout << "  [2/9] Initializing cache manager..." << endl;
        cache_manager_ = new CacheManager(
            config_.cache_size,
            config_.cache_size,
            config_.cache_size * 2,
            config_.cache_size * 4);
        cout << "    ✓ Cache manager initialized" << endl;

        cout << "  [3/9] Initializing index manager..." << endl;
        index_manager_ = new IndexManager(config_.index_path);
        if (!index_manager_->open_indexes())
        {
            cout << "    No existing indexes found. Creating new..." << endl;
            if (!index_manager_->create_indexes())
            {
                cerr << "    ERROR: Failed to create indexes!" << endl;
                return false;
            }
        }
        cout << "    ✓ Index manager initialized" << endl;

        cout << "  [4/9] Initializing security manager..." << endl;
        security_manager_ = new SecurityManager();
        cout << "    ✓ Security manager initialized" << endl;

        cout << "  [5/9] Initializing session manager..." << endl;
        session_manager_ = new SessionManager(
            *security_manager_,
            *cache_manager_,
            *db_manager_,
            config_.session_timeout);
        cout << "    ✓ Session manager initialized" << endl;

        cout << "  [6/9] Initializing feature modules..." << endl;

        trip_manager_ = new TripManager(*db_manager_, *cache_manager_,
                                        *index_manager_);
        vehicle_manager_ = new VehicleManager(*db_manager_, *cache_manager_,
                                              *index_manager_);
        expense_manager_ = new ExpenseManager(*db_manager_, *cache_manager_,
                                              *index_manager_);
        expense_manager_->set_trip_manager(trip_manager_);
        driver_manager_ = new DriverManager(*db_manager_, *cache_manager_,
                                            *index_manager_);

        incident_manager_ = new IncidentManager(*db_manager_, *cache_manager_);

        cout << "    ✓ Feature modules initialized" << endl;

        cout << "  [8/9] Initializing request handler..." << endl;
        request_handler_ = new RequestHandler(
            *db_manager_, *cache_manager_, *session_manager_,
            *trip_manager_, *vehicle_manager_, *expense_manager_,
            *driver_manager_, *incident_manager_ );
        cout << "    ✓ Request handler initialized" << endl;

        cout << "  [9/9] Creating default admin account..." << endl;
        if (!session_manager_->register_user(
                config_.admin_username,
                config_.admin_password,
                "System Administrator",
                "admin@smartdrive.local",
                "+1234567890",
                UserRole::ADMIN))
        {
            cout << "    ! Admin account already exists" << endl;
        }
        else
        {
            cout << "    ✓ Admin account created" << endl;
            cout << "      Username: " << config_.admin_username << endl;
            cout << "      Password: " << config_.admin_password << endl;
        }

        cout << endl;
        cout << "✓ Server initialization complete!" << endl;
        cout << endl;

        return true;
    }

    bool start()
    {
        if (running_)
        {
            cerr << "Server is already running!" << endl;
            return false;
        }

        server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket_ < 0)
        {
            cerr << "ERROR: Failed to create socket" << endl;
            return false;
        }

        int opt = 1;
        if (setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR,
                       &opt, sizeof(opt)) < 0)
        {
            cerr << "ERROR: Failed to set socket options" << endl;
            close(server_socket_);
            return false;
        }

        server_addr_.sin_family = AF_INET;
        server_addr_.sin_addr.s_addr = INADDR_ANY;
        server_addr_.sin_port = htons(config_.port);

        if (bind(server_socket_, (struct sockaddr *)&server_addr_,
                 sizeof(server_addr_)) < 0)
        {
            cerr << "ERROR: Failed to bind socket to port " << config_.port << endl;
            close(server_socket_);
            return false;
        }

        if (listen(server_socket_, config_.max_connections) < 0)
        {
            cerr << "ERROR: Failed to listen on socket" << endl;
            close(server_socket_);
            return false;
        }

        running_ = true;

        cout << "Starting " << config_.worker_threads << " worker threads..." << endl;
        for (int i = 0; i < config_.worker_threads; i++)
        {
            worker_threads_.emplace_back(&SDMServer::worker_thread, this, i);
        }

        cout << "Starting listener thread..." << endl;
        listener_thread_ = thread(&SDMServer::listener_thread, this);

        cout << endl;
        cout << "╔════════════════════════════════════════╗" << endl;
        cout << "║  Smart Drive Manager Server RUNNING   ║" << endl;
        cout << "╠════════════════════════════════════════╣" << endl;
        cout << "║  Port: " << setw(31) << left << config_.port << " ║" << endl;
        cout << "║  Workers: " << setw(28) << left << config_.worker_threads << " ║" << endl;
        cout << "║  Queue Capacity: " << setw(21) << left << config_.queue_capacity << " ║" << endl;
        cout << "╚════════════════════════════════════════╝" << endl;
        cout << endl;
        cout << "Server is ready to accept connections..." << endl;
        cout << "Press Ctrl+C to stop the server." << endl;
        cout << endl;

        return true;
    }

    void stop()
    {
        if (!running_)
            return;

        cout << endl;
        cout << "Shutting down server..." << endl;

        running_ = false;

        if (server_socket_ >= 0)
        {
            close(server_socket_);
            server_socket_ = -1;
        }

        if (listener_thread_.joinable())
        {
            listener_thread_.join();
        }

        for (auto &thread : worker_threads_)
        {
            if (thread.joinable())
            {
                thread.join();
            }
        }

        worker_threads_.clear();

        print_statistics();

        cout << "Server stopped successfully." << endl;
    }

    void wait_for_shutdown()
    {
        if (listener_thread_.joinable())
        {
            listener_thread_.join();
        }
    }

private:
    void listener_thread()
    {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        while (running_)
        {

            int client_socket = accept(server_socket_,
                                       (struct sockaddr *)&client_addr,
                                       &client_len);

            if (client_socket < 0)
            {
                if (running_)
                {
                    cerr << "WARNING: Failed to accept connection" << endl;
                }
                continue;
            }

            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);

            char buffer[8192];
            ssize_t bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);

            if (bytes_read <= 0)
            {
                close(client_socket);
                continue;
            }

            buffer[bytes_read] = '\0';
            string request_data(buffer);

            ServerRequest request(
                client_socket,
                generate_request_id(),
                string(client_ip),
                request_data);

            if (!request_queue_.try_enqueue(request))
            {

                rejected_requests_++;
                send_error(client_socket, 503, "Service Unavailable",
                           "Server is overloaded. Please try again later.");
                close(client_socket);
            }

            total_requests_++;
        }
    }

    void worker_thread(int worker_id)
    {
        while (running_)
        {
            ServerRequest request;

            if (request_queue_.try_dequeue(request))
            {
                process_request(request);
            }
            else
            {
                this_thread::sleep_for(chrono::milliseconds(10));
            }
        }
    }

    void process_request(const ServerRequest &request)
    {
        try
        {
            
            if (request.request_data.find("OPTIONS") == 0)
            {
                send_cors_preflight(request.client_socket);
                return;
            }

            string response = request_handler_->handle_request(
                request.request_data,
                request.client_ip);
            send_response(request.client_socket, response);
        }
        catch (const exception &e)
        {
            total_errors_++;
            cerr << "ERROR processing request: " << e.what() << endl;
            send_error(request.client_socket, 500, "Internal Server Error",
                       "An error occurred while processing your request.");
        }

        close(request.client_socket);
    }

    void send_response(int socket, const string &response)
    {
        string http_response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Access-Control-Allow-Methods: POST, GET, OPTIONS\r\n"
            "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
            "Content-Length: " +
            to_string(response.length()) + "\r\n"
                                           "Connection: close\r\n"
                                           "\r\n" +
            response;

        send(socket, http_response.c_str(), http_response.length(), 0);
    }

    void send_error(int socket, int code, const string &status,
                    const string &message)
    {
        string response =
            "HTTP/1.1 " + to_string(code) + " " + status + "\r\n"
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Access-Control-Allow-Methods: POST, GET, OPTIONS\r\n"
            "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
            "Connection: close\r\n"
            "\r\n"
            "{\"error\":\"" + message + "\"}";

        send(socket, response.c_str(), response.length(), 0);
    }

    void send_cors_preflight(int socket)
    {
        string response =
            "HTTP/1.1 204 No Content\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Access-Control-Allow-Methods: POST, GET, OPTIONS\r\n"
            "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
            "Access-Control-Max-Age: 86400\r\n"
            "Connection: close\r\n"
            "\r\n";

        send(socket, response.c_str(), response.length(), 0);
    }

    uint64_t generate_request_id()
    {
        static atomic<uint64_t> counter(1);
        return counter++;
    }

    uint64_t get_current_timestamp()
    {
        return chrono::system_clock::now().time_since_epoch().count();
    }

    void print_statistics()
    {
        uint64_t uptime_ns = get_current_timestamp() - start_time_;
        uint64_t uptime_seconds = uptime_ns / 1000000000ULL;

        auto cache_stats = cache_manager_->get_stats();

        cout << endl;
        cout << "=== Server Statistics ===" << endl;
        cout << "  Uptime: " << uptime_seconds << " seconds" << endl;
        cout << "  Total Requests: " << total_requests_ << endl;
        cout << "  Total Errors: " << total_errors_ << endl;
        cout << "  Rejected Requests: " << rejected_requests_ << endl;
        cout << "  Queue Size: " << request_queue_.size() << "/" << request_queue_.capacity() << endl;
        cout << endl;
        cout << "=== Cache Statistics ===" << endl;
        cout << "  Driver Hit Rate: " << (cache_stats.driver_hit_rate * 100) << "%" << endl;
        cout << "  Vehicle Hit Rate: " << (cache_stats.vehicle_hit_rate * 100) << "%" << endl;
        cout << "  Trip Hit Rate: " << (cache_stats.trip_hit_rate * 100) << "%" << endl;
        cout << "  Session Hit Rate: " << (cache_stats.session_hit_rate * 100) << "%" << endl;
        cout << endl;
    }

    void cleanup()
    {
        delete request_handler_;
        
        delete incident_manager_;
        
        delete driver_manager_;
        delete expense_manager_;
        delete vehicle_manager_;
        delete trip_manager_;
        
        delete session_manager_;
        delete security_manager_;
        delete index_manager_;
        delete cache_manager_;
        delete db_manager_;
    }
};

#endif