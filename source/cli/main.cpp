
#include "MenuSystem.h"
#include "../../include/sdm_config.hpp"

#include <iostream>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <vector>

using namespace std;

SDMServer *g_server = nullptr;

void signal_handler(int signal)
{
    if (signal == SIGINT || signal == SIGTERM)
    {
        cout << endl
             << endl;
        cout << "Received shutdown signal..." << endl;
        if (g_server)
        {
            g_server->stop();
        }
        exit(0);
    }
}

int main(int argc, char *argv[])
{

    string config_file = "../../include/sdm.conf";
    bool server_mode = true;

    for (int i = 1; i < argc; i++)
    {
        string arg = argv[i];

        if (arg == "--config" && i + 1 < argc)
        {
            config_file = argv[++i];
        }
        else if (arg == "--server")
        {
            server_mode = true;
        }
        else if (arg == "--help")
        {
            cout << "Smart Drive Manager - Usage:" << endl;
            cout << "  " << argv[0] << " [OPTIONS]" << endl;
            cout << endl;
            cout << "Options:" << endl;
            cout << "  --config FILE    Use specified configuration file" << endl;
            cout << "  --server         Run in server mode (daemon)" << endl;
            cout << "  --help           Show this help message" << endl;
            cout << endl;
            return 0;
        }
    }

    SDMConfig config;
    cout << "Loading config from: " << config_file << endl;

    string project_root = "";

    ifstream test_file("../../include/sdm.conf");
    if (test_file.good()) {
        project_root = "../../";
        test_file.close();
    } else {
        test_file.open("../include/sdm.conf");
        if (test_file.good()) {
            project_root = "../";
            test_file.close();
        }
    }
    
    vector<string> config_paths = {
        config_file,
        project_root + "include/sdm.conf",
        "../../include/sdm.conf",
        "../include/sdm.conf",
        "include/sdm.conf",
        "../../config/default.sdmconf",
        "../config/default.sdmconf"
    };

    bool config_loaded = false;
    for (const auto& path : config_paths)
    {
        if (config.load_from_file(path))
        {
            cout << "✓ Config loaded successfully from: " << path << endl;
            config_loaded = true;
            break;
        }
    }

    if (!config_loaded)
    {
        cout << "⚠ Warning: Could not load config file from any location" << endl;
        cout << "  Tried: " << config_file << endl;
        for (size_t i = 1; i < config_paths.size(); i++)
        {
            cout << "  Tried: " << config_paths[i] << endl;
        }
        cout << "  Using default configuration..." << endl;
    }

    cout << "\n=== Configuration ===" << endl;
    cout << "Database path: " << config.database_path << endl;
    cout << "Index path: " << config.index_path << endl;
    cout << "Port: " << config.port << endl;
    cout << "Worker Threads: " << config.worker_threads << " (from config file)" << endl;
    cout << "Max Connections: " << config.max_connections << endl;
    cout << "Queue Capacity: " << config.queue_capacity << endl;
    cout << "========================\n" << endl;

    if (server_mode)
    {

        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);

        SDMServer server(config);
        g_server = &server;

        if (!server.initialize())
        {
            cerr << "ERROR: Server initialization failed!" << endl;
            return 1;
        }

        if (!server.start())
        {
            cerr << "ERROR: Server start failed!" << endl;
            return 1;
        }

        server.wait_for_shutdown();
    }
    else
    {
        MenuSystem menu(config);

        if (!menu.initialize())
        {
            cerr << "ERROR: Menu system initialization failed!" << endl;
            return 1;
        }

        menu.run();
    }

    return 0;
}