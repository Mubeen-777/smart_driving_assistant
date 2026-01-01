#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include "../../include/sdm_types.hpp"
#include "../../include/sdm_config.hpp"
#include <fstream>
#include <string>
#include <vector>
#include <stdexcept>
#include <iostream>
#include <sys/stat.h>
#include <cstdlib>
#include <ctime>
#include <cstring>

using namespace std;

class DatabaseManager
{
private:
    fstream file_;
    string filename_;
    SDMHeader header_;
    bool is_open_;

    uint64_t driver_table_start_;
    uint64_t vehicle_table_start_;
    uint64_t trip_table_start_;
    uint64_t maintenance_table_start_;
    uint64_t expense_table_start_;
    uint64_t document_table_start_;
    uint64_t incident_table_start_;

    void calculate_offsets()
    {
        uint64_t current_offset = sizeof(SDMHeader);

        header_.driver_table_offset = current_offset;
        driver_table_start_ = current_offset;
        current_offset += header_.max_drivers * sizeof(DriverProfile);

        header_.vehicle_table_offset = current_offset;
        vehicle_table_start_ = current_offset;
        current_offset += header_.max_vehicles * sizeof(VehicleInfo);

        header_.trip_table_offset = current_offset;
        trip_table_start_ = current_offset;
        current_offset += header_.max_trips * sizeof(TripRecord);

        header_.maintenance_table_offset = current_offset;
        maintenance_table_start_ = current_offset;
        current_offset += 100000 * sizeof(MaintenanceRecord);

        header_.expense_table_offset = current_offset;
        expense_table_start_ = current_offset;
        current_offset += 500000 * sizeof(ExpenseRecord);

        header_.document_table_offset = current_offset;
        document_table_start_ = current_offset;
        current_offset += 100000 * sizeof(DocumentMetadata);

        header_.incident_table_offset = current_offset;
        incident_table_start_ = current_offset;
        current_offset += 50000 * sizeof(IncidentReport);

        header_.total_size = current_offset;
    }

public:
    DatabaseManager(const string &filename)
        : filename_(filename), is_open_(false) {}

    ~DatabaseManager()
    {
        close();
    }

    bool create(const SDMConfig &config)
    {
        if (file_.is_open()) {
            file_.close();
        }

        cout << "      Creating database: " << filename_ << endl;

        
        size_t last_slash = filename_.find_last_of('/');
        if (last_slash != string::npos) {
            string directory = filename_.substr(0, last_slash);
            struct stat st;
            if (stat(directory.c_str(), &st) != 0) {
                cout << "      Creating directory: " << directory << endl;
                string cmd = "mkdir -p \"" + directory + "\"";
                if (system(cmd.c_str()) != 0) {
                    cerr << "      ERROR: Failed to create directory!" << endl;
                }
            }
        }
        
        file_.open(filename_, ios::out | ios::binary | ios::trunc);
        if (!file_.is_open())
        {
            cerr << "      ERROR: Cannot create file: " << filename_ << endl;
            return false;
        }

        
        header_ = SDMHeader();
        header_.created_time = get_current_timestamp();
        header_.last_modified = header_.created_time;
        header_.max_drivers = config.max_drivers;
        header_.max_vehicles = config.max_vehicles;
        header_.max_trips = config.max_trips;

        calculate_offsets();

        
        file_.write(reinterpret_cast<const char *>(&header_), sizeof(SDMHeader));

        cout << "      Initializing " << header_.max_drivers << " driver slots..." << flush;
        DriverProfile empty_driver;
        empty_driver.is_active = 0;
        for (uint32_t i = 0; i < header_.max_drivers; i++)
        {
            file_.write(reinterpret_cast<const char *>(&empty_driver), sizeof(DriverProfile));
        }
        cout << " ✓" << endl;

        cout << "      Initializing " << header_.max_vehicles << " vehicle slots..." << flush;
        VehicleInfo empty_vehicle;
        empty_vehicle.is_active = 0;
        for (uint32_t i = 0; i < header_.max_vehicles; i++)
        {
            file_.write(reinterpret_cast<const char *>(&empty_vehicle), sizeof(VehicleInfo));
        }
        cout << " ✓" << endl;

        cout << "      Initializing " << header_.max_trips << " trip slots..." << flush;
        TripRecord empty_trip;
        for (uint32_t i = 0; i < header_.max_trips; i++)
        {
            file_.write(reinterpret_cast<const char *>(&empty_trip), sizeof(TripRecord));
        }
        cout << " ✓" << endl;

        cout << "      Initializing maintenance records..." << flush;
        MaintenanceRecord empty_maintenance;
        for (uint32_t i = 0; i < 100000; i++)
        {
            file_.write(reinterpret_cast<const char *>(&empty_maintenance), sizeof(MaintenanceRecord));
        }
        cout << " ✓" << endl;

        cout << "      Initializing expense records..." << flush;
        ExpenseRecord empty_expense;
        for (uint32_t i = 0; i < 500000; i++)
        {
            file_.write(reinterpret_cast<const char *>(&empty_expense), sizeof(ExpenseRecord));
        }
        cout << " ✓" << endl;

        cout << "      Initializing document metadata..." << flush;
        DocumentMetadata empty_doc;
        for (uint32_t i = 0; i < 100000; i++)
        {
            file_.write(reinterpret_cast<const char *>(&empty_doc), sizeof(DocumentMetadata));
        }
        cout << " ✓" << endl;

        cout << "      Initializing incident reports..." << flush;
        IncidentReport empty_incident;
        for (uint32_t i = 0; i < 50000; i++)
        {
            file_.write(reinterpret_cast<const char *>(&empty_incident), sizeof(IncidentReport));
        }
        cout << " ✓" << endl;

        file_.flush();
        file_.close();

        cout << "      Database created (" << (header_.total_size / 1024 / 1024) << " MB)" << endl;

        return true;
    }

    bool open()
    {
        file_.open(filename_, ios::in | ios::out | ios::binary);
        if (!file_.is_open())
        {
            return false;
        }

        file_.read(reinterpret_cast<char *>(&header_), sizeof(SDMHeader));

        if (string(header_.magic, 8) != "SDMDB001")
        {
            file_.close();
            return false;
        }

        driver_table_start_ = header_.driver_table_offset;
        vehicle_table_start_ = header_.vehicle_table_offset;
        trip_table_start_ = header_.trip_table_offset;
        maintenance_table_start_ = header_.maintenance_table_offset;
        expense_table_start_ = header_.expense_table_offset;
        document_table_start_ = header_.document_table_offset;
        incident_table_start_ = header_.incident_table_offset;

        is_open_ = true;
        return true;
    }

    void close()
    {
        if (is_open_ && file_.is_open())
        {
            header_.last_modified = get_current_timestamp();
            file_.seekp(0, ios::beg);
            file_.write(reinterpret_cast<const char *>(&header_), sizeof(SDMHeader));
            file_.flush();
            file_.close();
            is_open_ = false;
        }
    }

    
    bool create_driver(const DriverProfile &driver)
    {
        if (!is_open_) return false;

        for (uint32_t i = 0; i < header_.max_drivers; i++)
        {
            DriverProfile existing;
            uint64_t offset = driver_table_start_ + (i * sizeof(DriverProfile));

            file_.seekg(offset, ios::beg);
            file_.read(reinterpret_cast<char *>(&existing), sizeof(DriverProfile));

            if (existing.is_active == 0)
            {
                file_.seekp(offset, ios::beg);
                file_.write(reinterpret_cast<const char *>(&driver), sizeof(DriverProfile));
                file_.flush();
                return true;
            }
        }

        return false;
    }

    bool read_driver(uint64_t driver_id, DriverProfile &driver)
    {
        if (!is_open_) return false;

        for (uint32_t i = 0; i < header_.max_drivers; i++)
        {
            uint64_t offset = driver_table_start_ + (i * sizeof(DriverProfile));
            file_.seekg(offset, ios::beg);
            file_.read(reinterpret_cast<char *>(&driver), sizeof(DriverProfile));

            if (driver.is_active == 1 && driver.driver_id == driver_id)
            {
                return true;
            }
        }

        return false;
    }

    bool update_driver(const DriverProfile &driver)
    {
        if (!is_open_) return false;

        for (uint32_t i = 0; i < header_.max_drivers; i++)
        {
            DriverProfile existing;
            uint64_t offset = driver_table_start_ + (i * sizeof(DriverProfile));

            file_.seekg(offset, ios::beg);
            file_.read(reinterpret_cast<char *>(&existing), sizeof(DriverProfile));

            if (existing.is_active == 1 && existing.driver_id == driver.driver_id)
            {
                file_.seekp(offset, ios::beg);
                file_.write(reinterpret_cast<const char *>(&driver), sizeof(DriverProfile));
                file_.flush();
                return true;
            }
        }

        return false;
    }

    bool delete_driver(uint64_t driver_id)
    {
        if (!is_open_) return false;

        for (uint32_t i = 0; i < header_.max_drivers; i++)
        {
            DriverProfile driver;
            uint64_t offset = driver_table_start_ + (i * sizeof(DriverProfile));

            file_.seekg(offset, ios::beg);
            file_.read(reinterpret_cast<char *>(&driver), sizeof(DriverProfile));

            if (driver.is_active == 1 && driver.driver_id == driver_id)
            {
                driver.is_active = 0;
                file_.seekp(offset, ios::beg);
                file_.write(reinterpret_cast<const char *>(&driver), sizeof(DriverProfile));
                file_.flush();
                return true;
            }
        }

        return false;
    }

    vector<DriverProfile> get_all_drivers()
    {
        vector<DriverProfile> drivers;
        if (!is_open_) return drivers;

        for (uint32_t i = 0; i < header_.max_drivers; i++)
        {
            DriverProfile driver;
            uint64_t offset = driver_table_start_ + (i * sizeof(DriverProfile));

            file_.seekg(offset, ios::beg);
            file_.read(reinterpret_cast<char *>(&driver), sizeof(DriverProfile));

            if (driver.is_active == 1)
            {
                drivers.push_back(driver);
            }
        }

        return drivers;
    }

    
    bool create_vehicle(const VehicleInfo &vehicle)
    {
        if (!is_open_) return false;

        for (uint32_t i = 0; i < header_.max_vehicles; i++)
        {
            VehicleInfo existing;
            uint64_t offset = vehicle_table_start_ + (i * sizeof(VehicleInfo));

            file_.seekg(offset, ios::beg);
            file_.read(reinterpret_cast<char *>(&existing), sizeof(VehicleInfo));

            if (existing.is_active == 0)
            {
                file_.seekp(offset, ios::beg);
                file_.write(reinterpret_cast<const char *>(&vehicle), sizeof(VehicleInfo));
                file_.flush();
                return true;
            }
        }

        return false;
    }

    bool read_vehicle(uint64_t vehicle_id, VehicleInfo &vehicle)
    {
        if (!is_open_) return false;

        for (uint32_t i = 0; i < header_.max_vehicles; i++)
        {
            uint64_t offset = vehicle_table_start_ + (i * sizeof(VehicleInfo));
            file_.seekg(offset, ios::beg);
            file_.read(reinterpret_cast<char *>(&vehicle), sizeof(VehicleInfo));

            if (vehicle.is_active == 1 && vehicle.vehicle_id == vehicle_id)
            {
                return true;
            }
        }

        return false;
    }

    bool update_vehicle(const VehicleInfo &vehicle)
    {
        if (!is_open_) return false;

        for (uint32_t i = 0; i < header_.max_vehicles; i++)
        {
            VehicleInfo existing;
            uint64_t offset = vehicle_table_start_ + (i * sizeof(VehicleInfo));

            file_.seekg(offset, ios::beg);
            file_.read(reinterpret_cast<char *>(&existing), sizeof(VehicleInfo));

            if (existing.is_active == 1 && existing.vehicle_id == vehicle.vehicle_id)
            {
                file_.seekp(offset, ios::beg);
                file_.write(reinterpret_cast<const char *>(&vehicle), sizeof(VehicleInfo));
                file_.flush();
                return true;
            }
        }

        return false;
    }

    bool delete_vehicle(uint64_t vehicle_id)
    {
        if (!is_open_) return false;

        for (uint32_t i = 0; i < header_.max_vehicles; i++)
        {
            VehicleInfo vehicle;
            uint64_t offset = vehicle_table_start_ + (i * sizeof(VehicleInfo));

            file_.seekg(offset, ios::beg);
            file_.read(reinterpret_cast<char *>(&vehicle), sizeof(VehicleInfo));

            if (vehicle.is_active == 1 && vehicle.vehicle_id == vehicle_id)
            {
                vehicle.is_active = 0;
                file_.seekp(offset, ios::beg);
                file_.write(reinterpret_cast<const char *>(&vehicle), sizeof(VehicleInfo));
                file_.flush();
                return true;
            }
        }

        return false;
    }

    vector<VehicleInfo> get_vehicles_by_owner(uint64_t owner_id)
    {
        vector<VehicleInfo> vehicles;
        if (!is_open_) return vehicles;

        for (uint32_t i = 0; i < header_.max_vehicles; i++)
        {
            VehicleInfo vehicle;
            uint64_t offset = vehicle_table_start_ + (i * sizeof(VehicleInfo));

            file_.seekg(offset, ios::beg);
            file_.read(reinterpret_cast<char *>(&vehicle), sizeof(VehicleInfo));

            if (vehicle.is_active == 1 && vehicle.owner_driver_id == owner_id)
            {
                vehicles.push_back(vehicle);
            }
        }

        return vehicles;
    }

    
    bool create_trip(const TripRecord &trip)
    {
        if (!is_open_) return false;

        for (uint32_t i = 0; i < header_.max_trips; i++)
        {
            TripRecord existing;
            uint64_t offset = trip_table_start_ + (i * sizeof(TripRecord));

            file_.seekg(offset, ios::beg);
            file_.read(reinterpret_cast<char *>(&existing), sizeof(TripRecord));

            if (existing.trip_id == 0)
            {
                file_.seekp(offset, ios::beg);
                file_.write(reinterpret_cast<const char *>(&trip), sizeof(TripRecord));
                file_.flush();
                return true;
            }
        }

        return false;
    }

    bool read_trip(uint64_t trip_id, TripRecord &trip)
    {
        if (!is_open_) return false;

        for (uint32_t i = 0; i < header_.max_trips; i++)
        {
            uint64_t offset = trip_table_start_ + (i * sizeof(TripRecord));
            file_.seekg(offset, ios::beg);
            file_.read(reinterpret_cast<char *>(&trip), sizeof(TripRecord));

            if (trip.trip_id == trip_id)
            {
                return true;
            }
        }

        return false;
    }

    bool update_trip(const TripRecord &trip)
    {
        if (!is_open_) return false;

        for (uint32_t i = 0; i < header_.max_trips; i++)
        {
            TripRecord existing;
            uint64_t offset = trip_table_start_ + (i * sizeof(TripRecord));

            file_.seekg(offset, ios::beg);
            file_.read(reinterpret_cast<char *>(&existing), sizeof(TripRecord));

            if (existing.trip_id == trip.trip_id)
            {
                file_.seekp(offset, ios::beg);
                file_.write(reinterpret_cast<const char *>(&trip), sizeof(TripRecord));
                file_.flush();
                return true;
            }
        }

        return false;
    }

    vector<TripRecord> get_trips_by_driver(uint64_t driver_id, int limit = 100)
    {
        vector<TripRecord> trips;
        if (!is_open_) return trips;

        int count = 0;
        for (uint32_t i = 0; i < header_.max_trips && count < limit; i++)
        {
            TripRecord trip;
            uint64_t offset = trip_table_start_ + (i * sizeof(TripRecord));

            file_.seekg(offset, ios::beg);
            file_.read(reinterpret_cast<char *>(&trip), sizeof(TripRecord));

            if (trip.trip_id != 0 && trip.driver_id == driver_id)
            {
                trips.push_back(trip);
                count++;
            }
        }

        return trips;
    }

    bool create_maintenance(const MaintenanceRecord &record)
    {
        if (!is_open_) return false;

        for (uint32_t i = 0; i < 100000; i++)
        {
            MaintenanceRecord existing;
            uint64_t offset = maintenance_table_start_ + (i * sizeof(MaintenanceRecord));

            file_.seekg(offset, ios::beg);
            file_.read(reinterpret_cast<char *>(&existing), sizeof(MaintenanceRecord));

            if (existing.maintenance_id == 0)
            {
                file_.seekp(offset, ios::beg);
                file_.write(reinterpret_cast<const char *>(&record), sizeof(MaintenanceRecord));
                file_.flush();
                return true;
            }
        }

        return false;
    }

    vector<MaintenanceRecord> get_maintenance_by_vehicle(uint64_t vehicle_id)
    {
        vector<MaintenanceRecord> records;
        if (!is_open_) return records;

        for (uint32_t i = 0; i < 100000; i++)
        {
            MaintenanceRecord record;
            uint64_t offset = maintenance_table_start_ + (i * sizeof(MaintenanceRecord));

            file_.seekg(offset, ios::beg);
            file_.read(reinterpret_cast<char *>(&record), sizeof(MaintenanceRecord));

            if (record.maintenance_id != 0 && record.vehicle_id == vehicle_id)
            {
                records.push_back(record);
            }
        }

        return records;
    }

    bool create_expense(const ExpenseRecord &expense)
    {
        if (!is_open_) return false;

        for (uint32_t i = 0; i < 500000; i++)
        {
            ExpenseRecord existing;
            uint64_t offset = expense_table_start_ + (i * sizeof(ExpenseRecord));

            file_.seekg(offset, ios::beg);
            file_.read(reinterpret_cast<char *>(&existing), sizeof(ExpenseRecord));

            if (existing.expense_id == 0)
            {
                file_.seekp(offset, ios::beg);
                file_.write(reinterpret_cast<const char *>(&expense), sizeof(ExpenseRecord));
                file_.flush();
                return true;
            }
        }

        return false;
    }

    vector<ExpenseRecord> get_expenses_by_driver(uint64_t driver_id, int limit = 100)
    {
        vector<ExpenseRecord> expenses;
        if (!is_open_) return expenses;

        int count = 0;
        for (uint32_t i = 0; i < 500000 && count < limit; i++)
        {
            ExpenseRecord expense;
            uint64_t offset = expense_table_start_ + (i * sizeof(ExpenseRecord));

            file_.seekg(offset, ios::beg);
            file_.read(reinterpret_cast<char *>(&expense), sizeof(ExpenseRecord));

            if (expense.expense_id != 0 && expense.driver_id == driver_id)
            {
                expenses.push_back(expense);
                count++;
            }
        }

        return expenses;
    }

    vector<ExpenseRecord> get_expenses_by_category(uint64_t driver_id, ExpenseCategory category)
    {
        vector<ExpenseRecord> expenses;
        if (!is_open_) return expenses;

        for (uint32_t i = 0; i < 500000; i++)
        {
            ExpenseRecord expense;
            uint64_t offset = expense_table_start_ + (i * sizeof(ExpenseRecord));

            file_.seekg(offset, ios::beg);
            file_.read(reinterpret_cast<char *>(&expense), sizeof(ExpenseRecord));

            if (expense.expense_id != 0 && expense.driver_id == driver_id &&
                expense.category == category)
            {
                expenses.push_back(expense);
            }
        }

        return expenses;
    }

    uint64_t get_current_timestamp() const
    {
        return static_cast<uint64_t>(time(nullptr));
    }

    const SDMHeader &get_header() const { return header_; }
    bool is_database_open() const { return is_open_; }

    DatabaseStats get_stats()
    {
        DatabaseStats stats;
        if (!is_open_) return stats;

        for (uint32_t i = 0; i < header_.max_drivers; i++)
        {
            DriverProfile driver;
            uint64_t offset = driver_table_start_ + (i * sizeof(DriverProfile));
            file_.seekg(offset, ios::beg);
            file_.read(reinterpret_cast<char *>(&driver), sizeof(DriverProfile));

            if (driver.is_active == 1)
            {
                stats.total_drivers++;
                stats.active_drivers++;
                stats.total_distance += driver.total_distance;
            }
        }

        for (uint32_t i = 0; i < header_.max_vehicles; i++)
        {
            VehicleInfo vehicle;
            uint64_t offset = vehicle_table_start_ + (i * sizeof(VehicleInfo));
            file_.seekg(offset, ios::beg);
            file_.read(reinterpret_cast<char *>(&vehicle), sizeof(VehicleInfo));

            if (vehicle.is_active == 1)
            {
                stats.total_vehicles++;
            }
        }

        for (uint32_t i = 0; i < header_.max_trips; i++)
        {
            TripRecord trip;
            uint64_t offset = trip_table_start_ + (i * sizeof(TripRecord));
            file_.seekg(offset, ios::beg);
            file_.read(reinterpret_cast<char *>(&trip), sizeof(TripRecord));

            if (trip.trip_id != 0)
            {
                stats.total_trips++;
            }
        }

        stats.database_size = header_.total_size;
        stats.used_space = stats.database_size;

        return stats;
    }
};

#endif