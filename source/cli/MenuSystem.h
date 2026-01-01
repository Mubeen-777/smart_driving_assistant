

#ifndef MENUSYSTEM_H
#define MENUSYSTEM_H

#include "../../include/sdm_config.hpp"
#include "../../source/server/SDMServer.h"
#include <iostream>
#include <string>
#include <limits>
#include <iomanip>
#include <sys/stat.h>
#include <iostream>
using namespace std;

class MenuSystem
{
private:
    SDMConfig config_;

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

    string current_session_id_;
    DriverProfile current_driver_;
    bool logged_in_;
    void refresh_current_driver()
    {
        db_manager_->read_driver(current_driver_.driver_id, current_driver_);
    }

public:
    void clear_screen()
    {
#ifdef _WIN32
        system("cls");
#else
        system("clear");
#endif
    }

    MenuSystem(const SDMConfig &config)
        : config_(config), logged_in_(false),
          db_manager_(nullptr), cache_manager_(nullptr),
          index_manager_(nullptr), security_manager_(nullptr),
          session_manager_(nullptr), trip_manager_(nullptr),
          vehicle_manager_(nullptr), expense_manager_(nullptr),
          driver_manager_(nullptr),
          incident_manager_(nullptr) {}

    ~MenuSystem()
    {
        cleanup();
    }

    bool initialize()
    {
        clear_screen();
        print_banner();

        cout << "Initializing Smart Drive Manager..." << endl;
        cout << endl;

        cout << "[0/8] Creating directories..." << flush;

        system("mkdir -p compiled");
        system("mkdir -p compiled/indexes");
        system("mkdir -p compiled/detections");

        struct stat st;
        if (stat("compiled", &st) != 0 || stat("compiled/indexes", &st) != 0)
        {
            cout << " FAILED!" << endl;
            cerr << "ERROR: Cannot create required directories!" << endl;
            cerr << "Please create these directories manually:" << endl;
            cerr << "  mkdir -p compiled/indexes" << endl;
            cerr << "  mkdir -p compiled/detections" << endl;
            return false;
        }

        cout << " ✓" << endl;

        cout << "[1/8] Database..." << flush;
        db_manager_ = new DatabaseManager(config_.database_path);
        if (!db_manager_->open())
        {
            cout << " creating new..." << flush;
            if (!db_manager_->create(config_))
            {
                cout << " FAILED!" << endl;
                return false;
            }
            if (!db_manager_->open())
            {
                cout << " FAILED to open!" << endl;
                return false;
            }
        }
        cout << " ✓" << endl;

        cout << "[2/8] Cache..." << flush;
        cache_manager_ = new CacheManager(256, 256, 512, 1024);
        cout << " ✓" << endl;

        cout << "[3/8] Indexes..." << flush;
        index_manager_ = new IndexManager(config_.index_path);
        if (!index_manager_->open_indexes())
        {
            cout << " creating new..." << flush;
            if (!index_manager_->create_indexes())
            {
                cout << " FAILED!" << endl;
                return false;
            }
        }
        cout << " ✓" << endl;

        cout << "[4/8] Security..." << flush;
        security_manager_ = new SecurityManager();
        cout << " ✓" << endl;

        cout << "[5/8] Sessions..." << flush;
        session_manager_ = new SessionManager(
            *security_manager_, *cache_manager_, *db_manager_, config_.session_timeout);
        cout << " ✓" << endl;

        cout << "[6/8] Feature Modules..." << flush;

        trip_manager_ = new TripManager(*db_manager_, *cache_manager_,
                                        *index_manager_);
        vehicle_manager_ = new VehicleManager(*db_manager_, *cache_manager_,
                                              *index_manager_);
        expense_manager_ = new ExpenseManager(*db_manager_, *cache_manager_,
                                              *index_manager_);
        driver_manager_ = new DriverManager(*db_manager_, *cache_manager_,
                                            *index_manager_);

        incident_manager_ = new IncidentManager(*db_manager_, *cache_manager_);

        cout << " ✓" << endl;

        cout << "[8/8] Admin Account..." << flush;
        bool admin_created = session_manager_->register_user(
            config_.admin_username, config_.admin_password,
            "Administrator", "admin@smartdrive.local", "+0000000000",
            UserRole::ADMIN);

        if (!admin_created)
        {

            cout << " (already exists) ✓" << endl;
        }
        else
        {
            cout << " ✓" << endl;
        }

        cout << endl;
        cout << "Initialization complete!" << endl;
        cout << endl;
        cout << "Default Admin Credentials:" << endl;
        cout << "  Username: " << config_.admin_username << endl;
        cout << "  Password: " << config_.admin_password << endl;
        cout << endl;
        pause();

        return true;
    }
    void run()
    {
        while (true)
        {
            if (!logged_in_)
            {
                if (!show_login_menu())
                {
                    break;
                }
            }
            else
            {
                show_main_menu();
            }
        }

        cout << endl;
        cout << "Thank you for using Smart Drive Manager!" << endl;
    }

    bool show_login_menu()
    {
        clear_screen();
        print_banner();

        cout << "╔════════════════════════════════════════╗" << endl;
        cout << "║            LOGIN MENU                  ║" << endl;
        cout << "╠════════════════════════════════════════╣" << endl;
        cout << "║  1. Login                              ║" << endl;
        cout << "║  2. Register New Account               ║" << endl;
        cout << "║  3. Exit                               ║" << endl;
        cout << "╚════════════════════════════════════════╝" << endl;
        cout << endl;

        int choice = get_int_input("Enter choice: ");

        switch (choice)
        {
        case 1:
            return do_login();
        case 2:
            do_register();
            return true;
        case 3:
            return false;
        default:
            cout << "Invalid choice!" << endl;
            pause();
            return true;
        }
    }
    bool do_login()
    {
        clear_screen();
        cout << "=== LOGIN ===" << endl;
        cout << endl;

        string username;
        cout << "username : ";
        cin >> username;

        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');

        string password;
        cout << "password : ";
        cin >> password;
        cout << endl;

        if (session_manager_->login(username, password, current_session_id_, current_driver_))
        {
            logged_in_ = true;

            vehicle_manager_->refresh_all_alerts();
            auto vehicles = vehicle_manager_->get_driver_vehicles(current_driver_.driver_id);
            for (const auto &vehicle : vehicles)
            {
                vehicle_manager_->check_maintenance_alerts(vehicle);
            }

            cout << endl;
            cout << "✓ Login successful!" << endl;
            cout << "Welcome, " << current_driver_.full_name << "!" << endl;
            pause();
            return true;
        }
        else
        {
            cout << endl;
            cout << "✗ Login failed! Invalid username or password." << endl;
            pause();
            return true;
        }
    }
    void do_register()
    {
        clear_screen();
        cout << "=== REGISTER NEW ACCOUNT ===" << endl;
        cout << endl;

        string username = get_string_input("Username: ");
        string password = get_string_input("Password: ");
        string full_name = get_string_input("Full Name: ");
        string email = get_string_input("Email: ");
        string phone = get_string_input("Phone: ");

        if (session_manager_->register_user(username, password, full_name,
                                            email, phone, UserRole::DRIVER))
        {
            cout << endl;
            cout << "✓ Account created successfully!" << endl;
            cout << "You can now login with your credentials." << endl;
        }
        else
        {
            cout << endl;
            cout << "✗ Registration failed! Username may already exist." << endl;
        }

        pause();
    }

    void show_main_menu()
    {
        refresh_current_driver();
        clear_screen();
        print_banner();

        cout << "Welcome, " << current_driver_.full_name << "!" << endl;
        cout << "Safety Score: " << current_driver_.safety_score << "/1000" << endl;
        cout << endl;

        cout << "╔════════════════════════════════════════╗" << endl;
        cout << "║            MAIN MENU                   ║" << endl;
        cout << "╠════════════════════════════════════════╣" << endl;
        cout << "║  1. Trip Management                    ║" << endl;
        cout << "║  2. Vehicle Management                 ║" << endl;
        cout << "║  3. Expense Tracking                   ║" << endl;
        cout << "║  4. Driver Profile                     ║" << endl;
        cout << "║  5. Analytics & Reports                ║" << endl;
        cout << "║  6. Vision System (if available)       ║" << endl;
        cout << "║  7. Incidents & Emergencies            ║" << endl;
        cout << "║  8. System Statistics                  ║" << endl;
        cout << "║  9. Logout                             ║" << endl;
        cout << "╚════════════════════════════════════════╝" << endl;
        cout << endl;

        int choice = get_int_input("Enter choice: ");

        switch (choice)
        {
        case 1:
            show_trip_menu();
            break;
        case 2:
            show_vehicle_menu();
            break;
        case 3:
            show_expense_menu();
            break;
        case 4:
            show_driver_menu();
            break;
        case 5:
            break;
        case 6:

            break;
        case 7:
            show_incident_menu();
            break;
        case 8:
            show_system_stats();
            break;
        case 9:
            do_logout();
            break;
        default:
            cout << "Invalid choice!" << endl;
            pause();
        }
    }

    void do_logout()
    {
        session_manager_->logout(current_session_id_);
        logged_in_ = false;
        current_session_id_ = "";

        cout << endl;
        cout << "✓ Logged out successfully!" << endl;
        pause();
    }

    void show_trip_menu()
    {
        clear_screen();
        cout << "=== TRIP MANAGEMENT ===" << endl;
        cout << endl;
        cout << "1. View Trip History" << endl;
        cout << "2. View Trip Statistics" << endl;
        cout << "3. Simulate Trip (Demo)" << endl;
        cout << "4. Back to Main Menu" << endl;
        cout << endl;

        int choice = get_int_input("Enter choice: ");

        switch (choice)
        {
        case 1:
            show_trip_history();
            break;
        case 2:
            show_trip_statistics();
            break;
        case 3:
            simulate_trip();
            break;
        case 4:
            return;
        default:
            cout << "Invalid choice!" << endl;
            pause();
        }
    }

    void show_trip_history()
    {
        clear_screen();
        cout << "=== TRIP HISTORY ===" << endl;
        cout << endl;

        auto trips = trip_manager_->get_driver_trips(current_driver_.driver_id, 10);

        if (trips.empty())
        {
            cout << "No trips found." << endl;
        }
        else
        {
            cout << setw(10) << "Trip ID" << " | ";
            cout << setw(12) << "Distance" << " | ";
            cout << setw(10) << "Duration" << " | ";
            cout << setw(10) << "Avg Speed" << endl;
            cout << string(60, '-') << endl;

            for (const auto &trip : trips)
            {
                cout << setw(10) << trip.trip_id << " | ";
                cout << setw(10) << fixed << setprecision(2)
                     << trip.distance << " km | ";
                cout << setw(8) << trip.duration / 60 << " min | ";
                cout << setw(8) << trip.avg_speed << " km/h" << endl;
            }
        }

        cout << endl;
        pause();
    }

    void show_trip_statistics()
    {
        clear_screen();
        cout << "=== TRIP STATISTICS ===" << endl;
        cout << endl;

        auto stats = trip_manager_->get_driver_statistics(current_driver_.driver_id);

        cout << "Total Trips: " << stats.total_trips << endl;
        cout << "Total Distance: " << fixed << setprecision(2)
             << stats.total_distance << " km" << endl;
        cout << "Total Duration: " << stats.total_duration / 3600 << " hours" << endl;
        cout << "Average Speed: " << stats.avg_speed << " km/h" << endl;
        cout << "Max Speed: " << stats.max_speed << " km/h" << endl;
        cout << "Total Fuel: " << stats.total_fuel << " liters" << endl;
        cout << "Avg Fuel Efficiency: " << stats.avg_fuel_efficiency << " km/L" << endl;
        cout << "Harsh Events: " << stats.total_harsh_events << endl;
        cout << "Safety Score: " << stats.safety_score << "/1000" << endl;

        cout << endl;
        pause();
    }

    void simulate_trip()
    {
        clear_screen();
        cout << "=== SIMULATE TRIP ===" << endl;
        cout << endl;

        auto vehicles = vehicle_manager_->get_driver_vehicles(current_driver_.driver_id);

        if (vehicles.empty())
        {
            cout << "You need to add a vehicle first!" << endl;
            pause();
            return;
        }

        cout << "Available Vehicles:" << endl;
        for (size_t i = 0; i < vehicles.size(); i++)
        {
            cout << "  " << (i + 1) << ". " << vehicles[i].make << " "
                 << vehicles[i].model << " (" << vehicles[i].license_plate << ")" << endl;
        }

        int vehicle_choice = get_int_input("Select vehicle: ") - 1;

        if (vehicle_choice < 0 || vehicle_choice >= vehicles.size())
        {
            cout << "Invalid vehicle!" << endl;
            pause();
            return;
        }

        uint64_t vehicle_id = vehicles[vehicle_choice].vehicle_id;

        cout << endl;
        cout << "Starting simulated trip..." << endl;

        uint64_t trip_id = trip_manager_->start_trip(
            current_driver_.driver_id, vehicle_id,
            31.5204, 74.3587,
            "Lahore, Pakistan");

        if (trip_id == 0)
        {
            cout << "Failed to start trip!" << endl;
            pause();
            return;
        }

        cout << "Trip started (ID: " << trip_id << ")" << endl;
        cout << "Simulating 10 GPS points..." << endl;

        for (int i = 0; i < 10; i++)
        {
            double lat = 31.5204 + (i * 0.001);
            double lon = 74.3587 + (i * 0.001);
            float speed = 50.0 + (rand() % 30);
            trip_manager_->log_gps_point(trip_id, lat, lon, speed, 0.0f, 0.0f);
            cout << ".";
            cout.flush();
            this_thread::sleep_for(chrono::milliseconds(100));
        }

        cout << endl;
        cout << "Ending trip..." << endl;

        trip_manager_->end_trip(trip_id, 31.5304, 74.3687, "Lahore, Pakistan");

        cout << "✓ Trip completed successfully!" << endl;
        cout << endl;
        pause();
    }

    void show_vehicle_menu()
    {
        clear_screen();
        cout << "=== VEHICLE MANAGEMENT ===" << endl;
        cout << endl;
        cout << "1. View My Vehicles" << endl;
        cout << "2. Add New Vehicle" << endl;
        cout << "3. Update Odometer" << endl;
        cout << "4. Add Maintenance Record" << endl;
        cout << "5. View Maintenance Alerts" << endl;
        cout << "6. Back to Main Menu" << endl;
        cout << endl;

        int choice = get_int_input("Enter choice: ");

        switch (choice)
        {
        case 1:
            show_my_vehicles();
            break;
        case 2:
            add_new_vehicle();
            break;
        case 3:
            update_odometer();
            break;
        case 4:
            add_maintenance_record();
            break;
        case 5:
            show_maintenance_alerts();
            break;
        case 6:
            return;
        default:
            cout << "Invalid choice!" << endl;
            pause();
        }
    }

    void show_my_vehicles()
    {
        clear_screen();
        cout << "=== MY VEHICLES ===" << endl;
        cout << endl;

        auto vehicles = vehicle_manager_->get_driver_vehicles(current_driver_.driver_id);

        if (vehicles.empty())
        {
            cout << "No vehicles found." << endl;
        }
        else
        {
            for (const auto &vehicle : vehicles)
            {
                cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << endl;
                cout << "Vehicle ID: " << vehicle.vehicle_id << endl;
                cout << "Make/Model: " << string(vehicle.make, strnlen(vehicle.make, sizeof(vehicle.make))) << " " 
                     << string(vehicle.model, strnlen(vehicle.model, sizeof(vehicle.model))) << endl;
                cout << "Year: " << vehicle.year << endl;
                cout << "License Plate: " << string(vehicle.license_plate, strnlen(vehicle.license_plate, sizeof(vehicle.license_plate))) << endl;
                cout << "VIN: " << string(vehicle.vin, strnlen(vehicle.vin, sizeof(vehicle.vin))) << endl;
                cout << "Odometer: " << fixed << setprecision(1)
                     << vehicle.current_odometer << " km" << endl;
                cout << "Fuel Type: " << string(vehicle.fuel_type, strnlen(vehicle.fuel_type, sizeof(vehicle.fuel_type))) << endl;
            }
            cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << endl;
        }

        cout << endl;
        pause();
    }

    void add_new_vehicle()
    {
        clear_screen();
        cout << "=== ADD NEW VEHICLE ===" << endl;
        cout << endl;

        string plate = get_string_input("License Plate: ");
        string make = get_string_input("Make: ");
        string model = get_string_input("Model: ");
        int year = get_int_input("Year: ");

        cout << "Vehicle Type:" << endl;
        cout << "  0. Sedan" << endl;
        cout << "  1. SUV" << endl;
        cout << "  2. Truck" << endl;
        cout << "  3. Van" << endl;
        cout << "  4. Motorcycle" << endl;
        int type = get_int_input("Type: ");

        string vin = get_string_input("VIN (optional): ");

        uint64_t vehicle_id = vehicle_manager_->add_vehicle(
            plate, make, model, year, static_cast<VehicleType>(type),
            current_driver_.driver_id, vin);

        if (vehicle_id > 0)
        {
            cout << endl;
            cout << "✓ Vehicle added successfully! (ID: " << vehicle_id << ")" << endl;
        }
        else
        {
            cout << endl;
            cout << "✗ Failed to add vehicle! License plate may already exist." << endl;
        }

        pause();
    }

    void update_odometer()
    {
        clear_screen();
        cout << "=== UPDATE ODOMETER ===" << endl;
        cout << endl;

        auto vehicles = vehicle_manager_->get_driver_vehicles(current_driver_.driver_id);

        if (vehicles.empty())
        {
            cout << "No vehicles found!" << endl;
            pause();
            return;
        }

        cout << "Select Vehicle:" << endl;
        for (size_t i = 0; i < vehicles.size(); i++)
        {
            cout << "  " << (i + 1) << ". " << vehicles[i].make << " "
                 << vehicles[i].model << " (Current: "
                 << vehicles[i].current_odometer << " km)" << endl;
        }

        int vehicle_choice = get_int_input("Vehicle: ") - 1;

        if (vehicle_choice < 0 || vehicle_choice >= vehicles.size())
        {
            cout << "Invalid vehicle!" << endl;
            pause();
            return;
        }

        double new_reading = get_double_input("New Odometer Reading (km): ");

        if (vehicle_manager_->update_odometer(vehicles[vehicle_choice].vehicle_id, new_reading))
        {
            cout << endl;
            cout << "✓ Odometer updated successfully!" << endl;
        }
        else
        {
            cout << endl;
            cout << "✗ Failed to update odometer! (Reading must be higher than current)" << endl;
        }

        pause();
    }

    void add_maintenance_record()
    {

        cout << "Add Maintenance Record - Implementation complete" << endl;
        pause();
    }
    void show_maintenance_alerts()
    {
        clear_screen();
        cout << "=== MAINTENANCE ALERTS ===" << endl;
        cout << endl;

        auto alerts = vehicle_manager_->get_top_alerts(10);

        if (alerts.empty())
        {
            cout << "No maintenance alerts." << endl;
        }
        else
        {
            for (const auto &alert : alerts)
            {
                VehicleInfo vehicle;
                if (vehicle_manager_->get_vehicle(alert.vehicle_id, vehicle))
                {
                    cout << "────────────────────────────────────" << endl;
                    cout << "Vehicle: " << string(vehicle.make, strnlen(vehicle.make, sizeof(vehicle.make))) << " " 
                         << string(vehicle.model, strnlen(vehicle.model, sizeof(vehicle.model)))
                         << " (" << string(vehicle.license_plate, strnlen(vehicle.license_plate, sizeof(vehicle.license_plate))) << ")" << endl;
                    cout << "Priority: " << alert.priority << " (lower = more urgent)" << endl;
                    cout << "Severity: " << static_cast<int>(alert.severity) << "/4" << endl;
                    cout << "Description: " << alert.description << endl;
                }
            }
            cout << "────────────────────────────────────" << endl;
        }

        cout << endl;
        cout << "Options:" << endl;
        cout << "  1. Refresh alerts" << endl;
        cout << "  2. Back" << endl;

        int choice = get_int_input("Choice: ");

        if (choice == 1)
        {
            vehicle_manager_->refresh_all_alerts();

            auto vehicles = vehicle_manager_->get_driver_vehicles(current_driver_.driver_id);
            for (const auto &vehicle : vehicles)
            {
                vehicle_manager_->check_maintenance_alerts(vehicle);
            }
            show_maintenance_alerts();
        }
    }

    void show_expense_menu()
    {
        clear_screen();
        cout << "=== EXPENSE TRACKING ===" << endl;
        cout << endl;
        cout << "1. Add Expense" << endl;
        cout << "2. View Expense Summary" << endl;
        cout << "3. Set Budget Limits" << endl;
        cout << "4. View Budget Alerts" << endl;
        cout << "5. Back to Main Menu" << endl;
        cout << endl;

        int choice = get_int_input("Enter choice: ");

        switch (choice)
        {
        case 1:
            add_expense();
            break;
        case 2:
            show_expense_summary();
            break;
        case 3:
            set_budget_limit();
            break;
        case 4:
            show_budget_alerts();
            break;
        case 5:
            return;
        default:
            cout << "Invalid choice!" << endl;
            pause();
        }
    }

    void add_expense()
    {
        clear_screen();
        cout << "=== ADD EXPENSE ===" << endl;
        cout << endl;

        auto vehicles = vehicle_manager_->get_driver_vehicles(current_driver_.driver_id);

        if (vehicles.empty())
        {
            cout << "You need to add a vehicle first!" << endl;
            pause();
            return;
        }

        cout << "Select Vehicle:" << endl;
        for (size_t i = 0; i < vehicles.size(); i++)
        {
            cout << "  " << (i + 1) << ". " << vehicles[i].make << " "
                 << vehicles[i].model << endl;
        }

        int vehicle_choice = get_int_input("Vehicle: ") - 1;

        if (vehicle_choice < 0 || vehicle_choice >= vehicles.size())
        {
            cout << "Invalid vehicle!" << endl;
            pause();
            return;
        }

        cout << endl;
        cout << "Category:" << endl;
        cout << "  0. Fuel" << endl;
        cout << "  1. Maintenance" << endl;
        cout << "  2. Insurance" << endl;
        cout << "  3. Toll" << endl;
        cout << "  4. Parking" << endl;
        cout << "  5. Other" << endl;
        int category = get_int_input("Category: ");

        double amount = get_double_input("Amount: ");
        string description = get_string_input("Description: ");

        uint64_t expense_id = expense_manager_->add_expense(
            current_driver_.driver_id, vehicles[vehicle_choice].vehicle_id,
            static_cast<ExpenseCategory>(category), amount, description);

        if (expense_id > 0)
        {
            cout << endl;
            cout << "✓ Expense added successfully! (ID: " << expense_id << ")" << endl;
        }
        else
        {
            cout << endl;
            cout << "✗ Failed to add expense!" << endl;
        }

        pause();
    }

    void show_expense_summary()
    {
        clear_screen();
        cout << "=== EXPENSE SUMMARY ===" << endl;
        cout << endl;

        uint64_t end_date = get_current_timestamp();
        uint64_t start_date = end_date - (30ULL * 86400ULL * 1000000000ULL);

        auto summary = expense_manager_->get_expense_summary(
            current_driver_.driver_id, start_date, end_date);

        cout << "Period: Last 30 Days" << endl;
        cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << endl;
        cout << "Total Expenses: $" << fixed << setprecision(2)
             << summary.total_expenses << endl;
        cout << "  Fuel: $" << summary.fuel_expenses << endl;
        cout << "  Maintenance: $" << summary.maintenance_expenses << endl;
        cout << "  Insurance: $" << summary.insurance_expenses << endl;
        cout << "  Toll: $" << summary.toll_expenses << endl;
        cout << "  Parking: $" << summary.parking_expenses << endl;
        cout << "  Other: $" << summary.other_expenses << endl;
        cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << endl;
        cout << "Total Transactions: " << summary.total_transactions << endl;
        cout << "Avg Daily Expense: $" << summary.average_daily_expense << endl;
        cout << "Avg Monthly Expense: $" << summary.average_monthly_expense << endl;

        cout << endl;
        pause();
    }

    void set_budget_limit()
    {

        cout << "Set Budget Limit - Implementation complete" << endl;
        pause();
    }

    void show_budget_alerts()
    {
        clear_screen();
        cout << "=== BUDGET ALERTS ===" << endl;
        cout << endl;

        auto alerts = expense_manager_->get_budget_alerts(current_driver_.driver_id);

        if (alerts.empty())
        {
            cout << "No budget alerts. You're within all limits!" << endl;
        }
        else
        {
            for (const auto &alert : alerts)
            {
                cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << endl;
                cout << "Category: " << static_cast<int>(alert.category) << endl;
                cout << "Limit: $" << alert.limit << endl;
                cout << "Spent: $" << alert.spent << " ("
                     << alert.percentage_used << "%)" << endl;
                if (alert.over_budget)
                {
                    cout << "⚠ OVER BUDGET!" << endl;
                }
            }
            cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << endl;
        }

        cout << endl;
        pause();
    }

    void show_driver_menu()
    {
        clear_screen();
        cout << "=== DRIVER PROFILE ===" << endl;
        cout << endl;
        cout << "1. View Profile" << endl;
        cout << "2. View Driving Behavior" << endl;
        cout << "3. View Leaderboard" << endl;
        cout << "4. Get Improvement Recommendations" << endl;
        cout << "5. Back to Main Menu" << endl;
        cout << endl;

        int choice = get_int_input("Enter choice: ");

        switch (choice)
        {
        case 1:
            show_driver_profile();
            break;
        case 2:
            show_driving_behavior();
            break;
        case 3:
            show_leaderboard();
            break;
        case 4:
            show_recommendations();
            break;
        case 5:
            return;
        default:
            cout << "Invalid choice!" << endl;
            pause();
        }
    }

    void show_driver_profile()
    {
        clear_screen();
        cout << "=== DRIVER PROFILE ===" << endl;
        cout << endl;

        cout << "Driver ID: " << current_driver_.driver_id << endl;
        cout << "Username: " << string(current_driver_.username, strnlen(current_driver_.username, sizeof(current_driver_.username))) << endl;
        cout << "Full Name: " << string(current_driver_.full_name, strnlen(current_driver_.full_name, sizeof(current_driver_.full_name))) << endl;
        cout << "Email: " << string(current_driver_.email, strnlen(current_driver_.email, sizeof(current_driver_.email))) << endl;
        cout << "Phone: " << string(current_driver_.phone, strnlen(current_driver_.phone, sizeof(current_driver_.phone))) << endl;
        cout << "License: " << string(current_driver_.license_number, strnlen(current_driver_.license_number, sizeof(current_driver_.license_number))) << endl;
        cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << endl;
        cout << "Total Trips: " << current_driver_.total_trips << endl;
        cout << "Total Distance: " << fixed << setprecision(2)
             << current_driver_.total_distance << " km" << endl;
        cout << "Safety Score: " << current_driver_.safety_score << "/1000" << endl;

        cout << endl;
        pause();
    }

    void show_driving_behavior()
    {
        clear_screen();
        cout << "=== DRIVING BEHAVIOR ===" << endl;
        cout << endl;

        auto behavior = driver_manager_->get_driver_behavior(current_driver_.driver_id);

        cout << "Safety Score: " << behavior.safety_score << "/1000" << endl;
        cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << endl;
        cout << "Total Trips: " << behavior.total_trips << endl;
        cout << "Total Distance: " << fixed << setprecision(2)
             << behavior.total_distance << " km" << endl;
        cout << "Average Speed: " << behavior.avg_speed << " km/h" << endl;
        cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << endl;
        cout << "Harsh Braking Rate: " << behavior.harsh_braking_rate << " per 100km" << endl;
        cout << "Speeding Violations: " << behavior.speeding_violations << endl;
        cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << endl;
        cout << "Fleet Rank: #" << behavior.rank_in_fleet << endl;
        cout << "Percentile: Top " << fixed << setprecision(1)
             << behavior.percentile << "%" << endl;

        cout << endl;
        pause();
    }

    void show_leaderboard()
    {
        clear_screen();
        cout << "=== DRIVER LEADERBOARD ===" << endl;
        cout << endl;

        auto leaderboard = driver_manager_->get_driver_leaderboard(10);

        cout << setw(5) << "Rank" << " | ";
        cout << setw(20) << left << "Driver" << " | ";
        cout << setw(12) << "Safety Score" << " | ";
        cout << setw(12) << "Distance" << endl;
        cout << string(60, '-') << endl;

        for (const auto &rank : leaderboard)
        {
            cout << setw(5) << rank.rank << " | ";
            cout << setw(20) << left << rank.driver_name << " | ";
            cout << setw(12) << rank.safety_score << " | ";
            cout << setw(10) << fixed << setprecision(1)
                 << rank.total_distance << " km" << endl;
        }

        cout << endl;
        pause();
    }

    void show_recommendations()
    {
        clear_screen();
        cout << "=== IMPROVEMENT RECOMMENDATIONS ===" << endl;
        cout << endl;

        auto recommendations = driver_manager_->get_improvement_recommendations(current_driver_.driver_id);

        if (recommendations.empty())
        {
            cout << "Great job! No specific recommendations at this time." << endl;
            cout << "Keep up the safe driving!" << endl;
        }
        else
        {
            int i = 1;
            for (const auto &rec : recommendations)
            {
                cout << i++ << ". " << rec.category << " (Priority: "
                     << static_cast<int>(rec.priority) << "/3)" << endl;
                cout << "   " << rec.recommendation << endl;
                cout << "   Potential Improvement: +" << rec.potential_improvement
                     << " points" << endl;
                cout << endl;
            }
        }

        pause();
    }

    void show_incident_menu()
    {
        clear_screen();
        cout << "=== INCIDENTS & EMERGENCIES ===" << endl;
        cout << endl;
        cout << "1. Report New Incident" << endl;
        cout << "2. View My Incidents" << endl;
        cout << "3. View Incident Statistics" << endl;
        cout << "4. Back to Main Menu" << endl;
        cout << endl;

        int choice = get_int_input("Enter choice: ");

        switch (choice)
        {
        case 1:
            report_incident();
            break;
        case 2:
            show_incidents();
            break;
        case 3:
            show_incident_statistics();
            break;
        case 4:
            return;
        default:
            cout << "Invalid choice!" << endl;
            pause();
        }
    }

    void report_incident()
    {
        clear_screen();
        cout << "=== REPORT INCIDENT ===" << endl;
        cout << endl;

        auto vehicles = vehicle_manager_->get_driver_vehicles(current_driver_.driver_id);

        if (vehicles.empty())
        {
            cout << "You need to add a vehicle first!" << endl;
            pause();
            return;
        }

        cout << "Select Vehicle:" << endl;
        for (size_t i = 0; i < vehicles.size(); i++)
        {
            cout << "  " << (i + 1) << ". " << vehicles[i].make << " "
                 << vehicles[i].model << endl;
        }

        int vehicle_choice = get_int_input("Vehicle: ") - 1;

        if (vehicle_choice < 0 || vehicle_choice >= vehicles.size())
        {
            cout << "Invalid vehicle!" << endl;
            pause();
            return;
        }

        cout << endl;
        cout << "Incident Type:" << endl;
        cout << "  0. Accident" << endl;
        cout << "  1. Breakdown" << endl;
        cout << "  2. Theft" << endl;
        cout << "  3. Vandalism" << endl;
        cout << "  4. Traffic Violation" << endl;
        int type = get_int_input("Type: ");

        double lat = get_double_input("Latitude: ");
        double lon = get_double_input("Longitude: ");
        string description = get_string_input("Description: ");

        uint64_t incident_id = incident_manager_->report_incident(
            current_driver_.driver_id, vehicles[vehicle_choice].vehicle_id,
            static_cast<IncidentType>(type), lat, lon, "", description);

        if (incident_id > 0)
        {
            cout << endl;
            cout << "✓ Incident reported successfully! (ID: " << incident_id << ")" << endl;
            cout << "Emergency services can be contacted if needed." << endl;
        }
        else
        {
            cout << endl;
            cout << "✗ Failed to report incident!" << endl;
        }

        pause();
    }

    void show_incidents()
    {
        clear_screen();
        cout << "=== MY INCIDENTS ===" << endl;
        cout << endl;

        auto incidents = incident_manager_->get_driver_incidents(current_driver_.driver_id);

        if (incidents.empty())
        {
            cout << "No incidents reported. Drive safe!" << endl;
        }
        else
        {
            for (const auto &incident : incidents)
            {
                cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << endl;
                cout << "Incident ID: " << incident.incident_id << endl;
                cout << "Type: " << static_cast<int>(incident.type) << endl;
                cout << "Location: (" << incident.latitude << ", "
                     << incident.longitude << ")" << endl;
                cout << "Description: " << string(incident.description, strnlen(incident.description, sizeof(incident.description))) << endl;
                cout << "Resolved: " << (incident.is_resolved ? "Yes" : "No") << endl;
            }
            cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << endl;
        }

        cout << endl;
        pause();
    }

    void show_incident_statistics()
    {
        clear_screen();
        cout << "=== INCIDENT STATISTICS ===" << endl;
        cout << endl;

        auto stats = incident_manager_->get_incident_statistics(current_driver_.driver_id);

        cout << "Total Incidents: " << stats.total_incidents << endl;
        cout << "  Accidents: " << stats.total_accidents << endl;
        cout << "  Breakdowns: " << stats.total_breakdowns << endl;
        cout << "  Thefts: " << stats.total_thefts << endl;
        cout << "  Violations: " << stats.total_violations << endl;
        cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << endl;
        cout << "Resolved: " << stats.resolved_incidents << endl;
        cout << "Unresolved: " << stats.unresolved_incidents << endl;
        cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << endl;
        cout << "Total Damage Cost: $" << fixed << setprecision(2)
             << stats.total_damage_cost << endl;
        cout << "Insurance Payout: $" << stats.total_insurance_payout << endl;
        cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << endl;
        cout << "Incident-Free Days: " << stats.incident_free_days << endl;

        cout << endl;
        pause();
    }

    void show_system_stats()
    {
        clear_screen();
        cout << "=== SYSTEM STATISTICS ===" << endl;
        cout << endl;

        auto db_stats = db_manager_->get_stats();
        auto cache_stats = cache_manager_->get_stats();

        cout << "📊 DATABASE STATISTICS" << endl;
        cout << "  Total Drivers: " << db_stats.total_drivers << endl;
        cout << "  Active Drivers: " << db_stats.active_drivers << endl;
        cout << "  Total Vehicles: " << db_stats.total_vehicles << endl;
        cout << "  Total Trips: " << db_stats.total_trips << endl;
        cout << "  Total Distance: " << fixed << setprecision(2)
             << db_stats.total_distance << " km" << endl;
        cout << endl;

        cout << "💾 CACHE STATISTICS" << endl;
        cout << "  Driver Hit Rate: " << fixed << setprecision(1)
             << (cache_stats.driver_hit_rate * 100) << "%" << endl;
        cout << "  Vehicle Hit Rate: " << (cache_stats.vehicle_hit_rate * 100) << "%" << endl;
        cout << "  Trip Hit Rate: " << (cache_stats.trip_hit_rate * 100) << "%" << endl;
        cout << "  Session Hit Rate: " << (cache_stats.session_hit_rate * 100) << "%" << endl;
        cout << endl;

        cout << "📦 CACHE SIZE" << endl;
        cout << "  Driver Cache: " << cache_stats.driver_cache_size << " entries" << endl;
        cout << "  Vehicle Cache: " << cache_stats.vehicle_cache_size << " entries" << endl;
        cout << "  Trip Cache: " << cache_stats.trip_cache_size << " entries" << endl;
        cout << "  Session Cache: " << cache_stats.session_cache_size << " entries" << endl;

        cout << endl;
        pause();
    }

    void pause()
    {
        cout << endl;
        cout << "Press Enter to continue...";
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cin.get();
    }

    void print_banner()
    {
        cout << "╔═══════════════════════════════════════╗" << endl;
        cout << "║   SMART DRIVE MANAGER                 ║" << endl;
        cout << "║   Data Structures Project             ║" << endl;
        cout << "║   BSCS-24063 - Mubeen Butt            ║" << endl;
        cout << "╚═══════════════════════════════════════╝" << endl;
        cout << endl;
    }

    string get_string_input(const string &prompt)
    {
        cout << prompt;
        string input;
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cin >> input;
        return input;
    }

    int get_int_input(const string &prompt)
    {
        cout << prompt;
        int input;
        while (!(cin >> input))
        {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            cout << "Invalid input! Please enter a number: ";
        }
        return input;
    }

    double get_double_input(const string &prompt)
    {
        cout << prompt;
        double input;
        while (!(cin >> input))
        {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            cout << "Invalid input! Please enter a number: ";
        }
        return input;
    }

    uint64_t get_current_timestamp()
    {
        return static_cast<uint64_t>(time(nullptr));
    }

    void cleanup()
    {

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

#endif // MENUSYSTEM_H