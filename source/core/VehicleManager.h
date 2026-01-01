#ifndef VEHICLEMANAGER_H
#define VEHICLEMANAGER_H

#include "../../include/sdm_types.hpp"
#include "../../source/core/DatabaseManager.h"
#include "../../source/core/CacheManager.h"
#include "../../source/core/IndexManager.h"
#include "../../source/data_structures/MinHeap.h"
#include <vector>
#include <string>
#include <set>

class VehicleManager
{
private:
    DatabaseManager &db_;
    CacheManager &cache_;
    IndexManager &index_;
    uint64_t next_vehicle_id_;
    uint64_t next_maintenance_id_;
    uint64_t next_alert_id_;
    set<string> processed_alerts_;
    
    MaintenanceAlertQueue alert_queue_;

    uint32_t calculate_alert_priority(const VehicleInfo &vehicle,
                                      const MaintenanceRecord &last_service,
                                      MaintenanceType type)
    {
        uint64_t current_time = get_current_timestamp();

        uint64_t days_overdue = 0;
        if (last_service.next_service_date > 0 && current_time > last_service.next_service_date)
        {
            days_overdue = (current_time - last_service.next_service_date) / 86400ULL;
        }

        uint8_t severity = 2; 
        switch (type)
        {
        case MaintenanceType::BRAKE_SERVICE:
        case MaintenanceType::ENGINE_CHECK:
            severity = 4; 
            break;
        case MaintenanceType::OIL_CHANGE:
        case MaintenanceType::TRANSMISSION:
            severity = 3; 
            break;
        default:
            severity = 2; 
            break;
        }

        uint32_t priority = 1000;
        priority -= (days_overdue * 20);
        priority -= (severity * 50);

        if (vehicle.current_odometer > last_service.next_service_odometer)
        {
            double km_overdue = vehicle.current_odometer - last_service.next_service_odometer;
            priority -= (km_overdue / 100);
        }

        return (priority > 0) ? priority : 1;
    }
    void clear_maintenance_alerts_for_vehicle_and_type(uint64_t vehicle_id, MaintenanceType type)
    {
        std::string alert_key = std::to_string(vehicle_id) + "_" +
                                std::to_string(static_cast<int>(type));
        processed_alerts_.erase(alert_key);

        auto all_alerts = alert_queue_.get_top_k(1000);

        while (!alert_queue_.empty())
        {
            alert_queue_.extract_min();
        }

        std::string type_string = get_maintenance_type_string(type);
        for (const auto &alert : all_alerts)
        {
            if (!(alert.vehicle_id == vehicle_id &&
                  std::string(alert.description).find(type_string) != std::string::npos))
            {
                alert_queue_.insert(alert);
            }
        }
    }

public:
    void check_maintenance_alerts(const VehicleInfo &vehicle)
    {
        auto maintenance_history = get_vehicle_maintenance_history(vehicle.vehicle_id);

        if (maintenance_history.empty())
        {
            std::string alert_key = std::to_string(vehicle.vehicle_id) + "_initial";

            if (processed_alerts_.find(alert_key) == processed_alerts_.end())
            {
                MaintenanceAlert alert(
                    vehicle.vehicle_id,
                    generate_alert_id(),
                    500,
                    get_current_timestamp(),
                    "Initial maintenance check required",
                    2);
                alert_queue_.insert(alert);
                processed_alerts_.insert(alert_key);
            }
            return;
        }

        MaintenanceType types[] = {
            MaintenanceType::OIL_CHANGE,
            MaintenanceType::BRAKE_SERVICE,
            MaintenanceType::TIRE_ROTATION,
            MaintenanceType::ENGINE_CHECK};

        for (auto type : types)
        {
            MaintenanceRecord *last_service = nullptr;
            for (auto &record : maintenance_history)
            {
                if (record.type == type)
                {
                    if (!last_service || record.service_date > last_service->service_date)
                    {
                        last_service = &record;
                    }
                }
            }

            if (last_service && last_service->next_service_date > 0)
            {
                uint64_t current_time = get_current_timestamp();

                if (current_time >= last_service->next_service_date ||
                    vehicle.current_odometer >= last_service->next_service_odometer)
                {
                    std::string alert_key = std::to_string(vehicle.vehicle_id) + "_" +
                                            std::to_string(static_cast<int>(type));

                    if (processed_alerts_.find(alert_key) == processed_alerts_.end())
                    {
                        uint32_t priority = calculate_alert_priority(vehicle, *last_service, type);
                        std::string desc = get_maintenance_type_string(type) + " is due";

                        MaintenanceAlert alert(
                            vehicle.vehicle_id,
                            generate_alert_id(),
                            priority,
                            last_service->next_service_date,
                            desc,
                            get_maintenance_severity(type));

                        alert_queue_.insert(alert);
                        processed_alerts_.insert(alert_key);
                    }
                }
            }
        }
    }

    VehicleManager(DatabaseManager &db, CacheManager &cache, IndexManager &index)
        : db_(db), cache_(cache), index_(index) 
    {
        next_vehicle_id_ = db_.get_max_vehicle_id() + 1;
        next_maintenance_id_ = db_.get_max_maintenance_id() + 1;
        next_alert_id_ = 1; 
    }

    uint64_t add_vehicle(const std::string &license_plate,
                         const std::string &make,
                         const std::string &model,
                         uint32_t year,
                         VehicleType type,
                         uint64_t owner_driver_id,
                         const std::string &vin = "")
    {
        
        uint64_t existing_id;
        if (index_.search_by_plate(license_plate, existing_id))
        {
            return 0; 
        }

        uint64_t vehicle_id = generate_vehicle_id();

        VehicleInfo vehicle;
        memset(&vehicle, 0, sizeof(VehicleInfo));
        vehicle.vehicle_id = vehicle_id;
        vehicle.owner_driver_id = owner_driver_id;
        
        if (!license_plate.empty())
            strncpy(vehicle.license_plate, license_plate.c_str(), sizeof(vehicle.license_plate) - 1);
        
        if (!make.empty())
            strncpy(vehicle.make, make.c_str(), sizeof(vehicle.make) - 1);
        
        if (!model.empty())
            strncpy(vehicle.model, model.c_str(), sizeof(vehicle.model) - 1);
        
        vehicle.year = year;
        vehicle.type = type;
        
        if (!vin.empty())
            strncpy(vehicle.vin, vin.c_str(), sizeof(vehicle.vin) - 1);
        
        vehicle.is_active = 1;
        vehicle.created_time = get_current_timestamp();

        if (!db_.create_vehicle(vehicle))
        {
            return 0;
        }

        index_.insert_vehicle_plate(license_plate, vehicle_id);
        index_.insert_primary(2, vehicle_id, vehicle.created_time, 0); 

        cache_.put_vehicle(vehicle_id, vehicle);

        return vehicle_id;
    }

    bool update_vehicle(const VehicleInfo &vehicle)
    {
        if (!db_.update_vehicle(vehicle))
        {
            return false;
        }

        cache_.put_vehicle(vehicle.vehicle_id, vehicle, true);
        return true;
    }

    bool delete_vehicle(uint64_t vehicle_id)
    {
        if (!db_.delete_vehicle(vehicle_id))
        {
            return false;
        }

        cache_.invalidate_vehicle(vehicle_id);
        return true;
    }

    bool get_vehicle(uint64_t vehicle_id, VehicleInfo &vehicle)
    {
        
        if (cache_.get_vehicle(vehicle_id, vehicle))
        {
            return true;
        }

        if (db_.read_vehicle(vehicle_id, vehicle))
        {
            cache_.put_vehicle(vehicle_id, vehicle);
            return true;
        }

        return false;
    }

    bool get_vehicle_by_plate(const std::string &license_plate, VehicleInfo &vehicle)
    {
        
        uint64_t vehicle_id;
        if (!index_.search_by_plate(license_plate, vehicle_id))
        {
            return false;
        }

        return get_vehicle(vehicle_id, vehicle);
    }

    std::vector<VehicleInfo> get_driver_vehicles(uint64_t driver_id)
    {
        return db_.get_vehicles_by_owner(driver_id);
    }

    bool update_odometer(uint64_t vehicle_id, double new_reading)
    {
        VehicleInfo vehicle;
        if (!get_vehicle(vehicle_id, vehicle))
        {
            return false;
        }

        if (new_reading < vehicle.current_odometer)
        {
            return false; 
        }

        vehicle.current_odometer = new_reading;

        check_maintenance_due(vehicle);

        return update_vehicle(vehicle);
    }
    void refresh_all_alerts()
    {
        processed_alerts_.clear();

        while (!alert_queue_.empty())
        {
            alert_queue_.extract_min();
        }
    }
    uint64_t add_maintenance_record(uint64_t vehicle_id,
                                    uint64_t driver_id,
                                    MaintenanceType type,
                                    double odometer_reading,
                                    const std::string &service_center,
                                    const std::string &description,
                                    double total_cost)
    {
        uint64_t maintenance_id = generate_maintenance_id();

        MaintenanceRecord record;
        memset(&record, 0, sizeof(MaintenanceRecord));
        record.maintenance_id = maintenance_id;
        record.vehicle_id = vehicle_id;
        record.driver_id = driver_id;
        record.type = type;
        record.service_date = get_current_timestamp();
        record.odometer_reading = odometer_reading;
        
        if (!service_center.empty())
            strncpy(record.service_center, service_center.c_str(), sizeof(record.service_center) - 1);
        
        if (!description.empty())
            strncpy(record.description, description.c_str(), sizeof(record.description) - 1);
        
        record.total_cost = total_cost;

        calculate_next_service(record);

        if (!db_.create_maintenance(record))
        {
            return 0;
        }

        VehicleInfo vehicle;
        if (get_vehicle(vehicle_id, vehicle))
        {
            vehicle.last_maintenance_date = record.service_date;
            vehicle.last_service_odometer = odometer_reading;
            vehicle.next_maintenance_due = record.next_service_date;
            update_vehicle(vehicle);
        }

        clear_maintenance_alerts_for_vehicle_and_type(vehicle_id, type);

        std::cout << "\n✓ Maintenance record added and alerts updated" << std::endl;

        return maintenance_id;
    }
    std::vector<MaintenanceRecord> get_vehicle_maintenance_history(uint64_t vehicle_id)
    {
        return db_.get_maintenance_by_vehicle(vehicle_id);
    }

    void refresh_maintenance_alerts()
    {
        alert_queue_.clear();

        auto all_drivers = db_.get_all_drivers();
        for (const auto &driver : all_drivers)
        {
            auto vehicles = get_driver_vehicles(driver.driver_id);

            for (const auto &vehicle : vehicles)
            {
                check_maintenance_alerts(vehicle);
            }
        }
    }

    std::vector<MaintenanceAlert> get_top_alerts(int count = 10)
    {
        return alert_queue_.get_top_k(count);
    }

    MaintenanceAlert get_next_alert()
    {
        if (alert_queue_.empty())
        {
            throw std::runtime_error("No maintenance alerts");
        }
        return alert_queue_.peek();
    }

    void acknowledge_alert(uint64_t alert_id)
    {
        
        if (!alert_queue_.empty())
        {
            auto top = alert_queue_.peek();
            if (top.alert_id == alert_id)
            {
                alert_queue_.extract_min();
            }
        }
    }

private:
    uint64_t generate_vehicle_id()
    {
        return next_vehicle_id_++;
    }

    uint64_t generate_maintenance_id()
    {
        return next_maintenance_id_++;
    }

    uint64_t generate_alert_id()
    {
        return next_alert_id_++;
    }

    uint64_t get_current_timestamp()
    {
        return static_cast<uint64_t>(time(nullptr));
    }

    void calculate_next_service(MaintenanceRecord &record)
    {
        uint64_t current_time = get_current_timestamp();
        const uint64_t NANOSECONDS_PER_DAY = 86400ULL * 1000000000ULL;

        switch (record.type)
        {
        case MaintenanceType::OIL_CHANGE:
            record.next_service_date = current_time + (90 * 86400); 
            record.next_service_odometer = record.odometer_reading + 5000;        
            break;
        case MaintenanceType::TIRE_ROTATION:
            record.next_service_date = current_time + (180 * 86400); 
            record.next_service_odometer = record.odometer_reading + 10000;
            break;
        case MaintenanceType::BRAKE_SERVICE:
            record.next_service_date = current_time + (365 * 86400); 
            record.next_service_odometer = record.odometer_reading + 20000;
            break;
        case MaintenanceType::ENGINE_CHECK:
            record.next_service_date = current_time + (365 * 86400); 
            record.next_service_odometer = record.odometer_reading + 15000;
            break;
        default:
            record.next_service_date = current_time + (180 * 86400);
            record.next_service_odometer = record.odometer_reading + 10000;
            break;
        }
    }
    void check_maintenance_due(const VehicleInfo &vehicle)
    {
        
        check_maintenance_alerts(vehicle);
    }

    std::string get_maintenance_type_string(MaintenanceType type)
    {
        switch (type)
        {
        case MaintenanceType::OIL_CHANGE:
            return "Oil Change";
        case MaintenanceType::TIRE_ROTATION:
            return "Tire Rotation";
        case MaintenanceType::BRAKE_SERVICE:
            return "Brake Service";
        case MaintenanceType::ENGINE_CHECK:
            return "Engine Check";
        case MaintenanceType::TRANSMISSION:
            return "Transmission Service";
        default:
            return "General Service";
        }
    }

    uint8_t get_maintenance_severity(MaintenanceType type)
    {
        switch (type)
        {
        case MaintenanceType::BRAKE_SERVICE:
        case MaintenanceType::ENGINE_CHECK:
            return 4; 
        case MaintenanceType::OIL_CHANGE:
        case MaintenanceType::TRANSMISSION:
            return 3; 
        default:
            return 2; 
        }
    }
};
#endif 