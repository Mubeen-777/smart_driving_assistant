#ifndef INDEXMANAGER_H
#define INDEXMANAGER_H

#include "../../source/data_structures/BTree.h"
#include "../../source/data_structures/BPlusTree.h"
#include "../../include/sdm_types.hpp"
#include <memory>
#include <string>
#include <sys/stat.h>
#include <iostream>
using namespace std;


class IndexManager
{
private:
    unique_ptr<BTree> primary_index_;
    unique_ptr<BPlusTree> driver_email_index_;
    unique_ptr<BPlusTree> vehicle_plate_index_;
    unique_ptr<BPlusTree> driver_username_index_;

    string index_dir_;

    bool ensure_directory_exists(const string& path) {
        struct stat st;
        if (stat(path.c_str(), &st) != 0) {
            cout << "      Creating index directory: " << path << endl;
            string cmd = "mkdir -p \"" + path + "\"";
            if (system(cmd.c_str()) != 0) {
                cerr << "      ERROR: Failed to create directory!" << endl;
                return false;
            }
        }
        return true;
    }

public:
    IndexManager(const string &index_dir) : index_dir_(index_dir) {}

    ~IndexManager()
    {
        close_all();
    }

    bool create_indexes()
    {
        cout << "      Creating primary B-Tree index..." << flush;
        primary_index_ = make_unique<BTree>(index_dir_ + "/primary.idx");
        if (!primary_index_->create()) {
            cerr << endl << "      ERROR: Failed to create primary index!" << endl;
            return false;
        }
        if (!primary_index_->open()) {
            cerr << endl << "      ERROR: Failed to open primary index!" << endl;
            return false;
        }
        cout << " ✓" << endl;

        cout << "      Creating driver email B+ Tree..." << flush;
        driver_email_index_ = make_unique<BPlusTree>(
            index_dir_ + "/driver_email.idx", "driver_email");
        if (!driver_email_index_->create()) {
            cerr << endl << "      ERROR: Failed to create email index!" << endl;
            return false;
        }
        if (!driver_email_index_->open()) {
            cerr << endl << "      ERROR: Failed to open email index!" << endl;
            return false;
        }
        cout << " ✓" << endl;

        cout << "      Creating vehicle plate B+ Tree..." << flush;
        vehicle_plate_index_ = make_unique<BPlusTree>(
            index_dir_ + "/vehicle_plate.idx", "vehicle_plate");
        if (!vehicle_plate_index_->create()) {
            cerr << endl << "      ERROR: Failed to create plate index!" << endl;
            return false;
        }
        if (!vehicle_plate_index_->open()) {
            cerr << endl << "      ERROR: Failed to open plate index!" << endl;
            return false;
        }
        cout << " ✓" << endl;

        cout << "      Creating driver username B+ Tree..." << flush;
        driver_username_index_ = make_unique<BPlusTree>(
            index_dir_ + "/driver_username.idx", "driver_username");
        if (!driver_username_index_->create()) {
            cerr << endl << "      ERROR: Failed to create username index!" << endl;
            return false;
        }
        if (!driver_username_index_->open()) {
            cerr << endl << "      ERROR: Failed to open username index!" << endl;
            return false;
        }
        cout << " ✓" << endl;

        return true;
    }

    bool open_indexes()
    {
        cout << "      Opening primary index..." << flush;
        primary_index_ = make_unique<BTree>(index_dir_ + "/primary.idx");
        if (!primary_index_->open()) {
            cout << " NOT FOUND" << endl;
            return false;
        }
        cout << " ✓" << endl;

        cout << "      Opening email index..." << flush;
        driver_email_index_ = make_unique<BPlusTree>(
            index_dir_ + "/driver_email.idx", "driver_email");
        if (!driver_email_index_->open()) {
            cout << " NOT FOUND" << endl;
            return false;
        }
        cout << " ✓" << endl;

        cout << "      Opening plate index..." << flush;
        vehicle_plate_index_ = make_unique<BPlusTree>(
            index_dir_ + "/vehicle_plate.idx", "vehicle_plate");
        if (!vehicle_plate_index_->open()) {
            cout << " NOT FOUND" << endl;
            return false;
        }
        cout << " ✓" << endl;

        cout << "      Opening username index..." << flush;
        driver_username_index_ = make_unique<BPlusTree>(
            index_dir_ + "/driver_username.idx", "driver_username");
        if (!driver_username_index_->open()) {
            cout << " NOT FOUND" << endl;
            return false;
        }
        cout << " ✓" << endl;

        return true;
    }

    void close_all()
    {
        if (primary_index_)
            primary_index_->close();
        if (driver_email_index_)
            driver_email_index_->close();
        if (vehicle_plate_index_)
            vehicle_plate_index_->close();
        if (driver_username_index_)
            driver_username_index_->close();
    }

    bool insert_primary(uint8_t entity_type, uint64_t entity_id,
                        uint64_t timestamp, uint64_t record_offset)
    {
        if (!primary_index_)
            return false;

        CompositeKey key(entity_type, entity_id, timestamp, 0);
        BTreeValue value(record_offset, 1, 1024);

        return primary_index_->insert(key, value);
    }

    bool search_primary(uint8_t entity_type, uint64_t entity_id,
                        uint64_t timestamp, uint64_t &record_offset)
    {
        if (!primary_index_)
            return false;

        CompositeKey key(entity_type, entity_id, timestamp, 0);
        BTreeValue value;

        if (primary_index_->search(key, value))
        {
            record_offset = value.record_offset;
            return true;
        }

        return false;
    }

    vector<uint64_t> range_query_primary(uint8_t entity_type, uint64_t entity_id,
                                              uint64_t start_time, uint64_t end_time)
    {
        vector<uint64_t> offsets;
        if (!primary_index_)
            return offsets;

        CompositeKey start_key(entity_type, entity_id, start_time, 0);
        CompositeKey end_key(entity_type, entity_id, end_time, UINT32_MAX);

        auto results = primary_index_->range_query(start_key, end_key);

        for (const auto &result : results)
        {
            offsets.push_back(result.second.record_offset);
        }

        return offsets;
    }

    bool insert_driver_email(const string &email, uint64_t driver_id)
    {
        if (!driver_email_index_)
            return false;

        BPlusKey key(email);
        BPlusValue value(driver_id, 1);
        return driver_email_index_->insert(key, value);
    }

    bool search_by_email(const string &email, uint64_t &driver_id)
    {
        if (!driver_email_index_)
            return false;

        BPlusKey key(email);
        BPlusValue value;

        if (driver_email_index_->search(key, value))
        {
            driver_id = value.primary_id;
            return true;
        }

        return false;
    }

    bool insert_driver_username(const string &username, uint64_t driver_id)
    {
        if (!driver_username_index_)
            return false;

        BPlusKey key(username);
        BPlusValue value(driver_id, 1);

        return driver_username_index_->insert(key, value);
    }

    bool search_by_username(const string &username, uint64_t &driver_id)
    {
        if (!driver_username_index_)
            return false;

        BPlusKey key(username);
        BPlusValue value;

        if (driver_username_index_->search(key, value))
        {
            driver_id = value.primary_id;
            return true;
        }

        return false;
    }

    bool insert_vehicle_plate(const string &plate, uint64_t vehicle_id)
    {
        if (!vehicle_plate_index_)
            return false;

        BPlusKey key(plate);
        BPlusValue value(vehicle_id, 2);

        return vehicle_plate_index_->insert(key, value);
    }

    bool search_by_plate(const string &plate, uint64_t &vehicle_id)
    {
        if (!vehicle_plate_index_)
            return false;

        BPlusKey key(plate);
        BPlusValue value;

        if (vehicle_plate_index_->search(key, value))
        {
            vehicle_id = value.primary_id;
            return true;
        }

        return false;
    }

    bool rebuild_driver_indexes(const vector<DriverProfile> &drivers)
    {
        for (const auto &driver : drivers)
        {
            insert_driver_email(driver.email, driver.driver_id);
            insert_driver_username(driver.username, driver.driver_id);
        }
        return true;
    }

    bool rebuild_vehicle_indexes(const vector<VehicleInfo> &vehicles)
    {
        for (const auto &vehicle : vehicles)
        {
            insert_vehicle_plate(vehicle.license_plate, vehicle.vehicle_id);
        }
        return true;
    }

    uint64_t get_primary_record_count() const
    {
        return primary_index_ ? primary_index_->get_total_records() : 0;
    }

    uint64_t get_driver_email_count() const
    {
        return driver_email_index_ ? driver_email_index_->get_total_entries() : 0;
    }

    uint64_t get_vehicle_plate_count() const
    {
        return vehicle_plate_index_ ? vehicle_plate_index_->get_total_entries() : 0;
    }
};

#endif
