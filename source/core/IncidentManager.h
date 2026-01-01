#ifndef INCIDENTMANAGER_H
#define INCIDENTMANAGER_H

#include "../../include/sdm_types.hpp"
#include "DatabaseManager.h"
#include "CacheManager.h"
#include <vector>
#include <iostream>
#include <string>
#include <chrono>
#include <algorithm>
#include <cstring>
using namespace std;

class IncidentManager
{
private:
    DatabaseManager &db_;
    CacheManager &cache_;

    uint64_t next_incident_id_;

    void update_driver_safety_after_incident(uint64_t driver_id, IncidentType type)
    {
        DriverProfile driver;
        if (!db_.read_driver(driver_id, driver))
        {
            return;
        }

        uint32_t deduction = 0;
        switch (type)
        {
        case IncidentType::ACCIDENT:
            deduction = 150;
            cout << "\nSAFETY IMPACT: -150 points (Accident)" << endl;
            break;
        case IncidentType::BREAKDOWN:
            deduction = 0;
            break;
        case IncidentType::THEFT:
            deduction = 0;
            break;
        case IncidentType::VANDALISM:
            deduction = 0;
            break;
        case IncidentType::TRAFFIC_VIOLATION:
            deduction = 100;
            cout << "\nSAFETY IMPACT: -100 points (Traffic Violation)" << endl;
            break;
        default:
            deduction = 50;
            break;
        }

        if (driver.safety_score > deduction)
        {
            driver.safety_score -= deduction;
        }
        else
        {
            driver.safety_score = 0;
        }

        db_.update_driver(driver);
        cache_.invalidate_driver(driver_id);

        cout << "New Safety Score: " << driver.safety_score << "/1000" << endl;
    }

public:
    IncidentManager(DatabaseManager &db, CacheManager &cache)
        : db_(db), cache_(cache)
    {
        next_incident_id_ = db_.get_max_incident_id() + 1;
    }

    uint64_t report_incident(uint64_t driver_id,
                             uint64_t vehicle_id,
                             IncidentType type,
                             double latitude,
                             double longitude,
                             const string &location_address,
                             const string &description,
                             uint64_t trip_id = 0)
    {
        uint64_t incident_id = next_incident_id_++;

        IncidentReport incident;
        memset(&incident, 0, sizeof(IncidentReport));
        incident.incident_id = incident_id;
        incident.driver_id = driver_id;
        incident.vehicle_id = vehicle_id;
        incident.trip_id = trip_id;
        incident.type = type;
        incident.incident_time = get_current_timestamp();
        incident.latitude = latitude;
        incident.longitude = longitude;
        
        if (!location_address.empty())
            strncpy(incident.location_address, location_address.c_str(),
                    sizeof(incident.location_address) - 1);
        
        if (!description.empty())
            strncpy(incident.description, description.c_str(),
                    sizeof(incident.description) - 1);
        
        incident.is_resolved = 0;

        // Save to database
        if (!db_.create_incident(incident))
        {
            return 0;
        }

        update_driver_safety_after_incident(driver_id, type);

        return incident_id;
    }

    uint64_t report_accident(uint64_t driver_id, uint64_t vehicle_id,
                             double latitude, double longitude,
                             const string &description,
                             const string &other_party_info = "",
                             double estimated_damage = 0.0)
    {
        uint64_t incident_id = report_incident(driver_id, vehicle_id,
                                               IncidentType::ACCIDENT, latitude, longitude, "", description);

        // Update with additional accident details
        IncidentReport incident = get_incident_by_id(incident_id);
        if (incident.incident_id > 0)
        {
            strncpy(incident.other_party_info, other_party_info.c_str(),
                    sizeof(incident.other_party_info) - 1);
            incident.estimated_damage = estimated_damage;

            // Save updated incident
            if (!db_.update_incident(incident))
            {
                return 0;
            }
        }

        return incident_id;
    }

    uint64_t report_breakdown(uint64_t driver_id, uint64_t vehicle_id,
                              double latitude, double longitude,
                              const string &issue_description)
    {
        return report_incident(driver_id, vehicle_id, IncidentType::BREAKDOWN,
                               latitude, longitude, "", issue_description);
    }

    uint64_t report_theft(uint64_t driver_id, uint64_t vehicle_id,
                          double latitude, double longitude,
                          const string &description,
                          const string &police_report_number)
    {
        uint64_t incident_id = report_incident(driver_id, vehicle_id,
                                               IncidentType::THEFT, latitude, longitude, "", description);

        // Update with police report number
        IncidentReport incident = get_incident_by_id(incident_id);
        if (incident.incident_id > 0)
        {
            strncpy(incident.police_report_number, police_report_number.c_str(),
                    sizeof(incident.police_report_number) - 1);

            if (!db_.update_incident(incident))
            {
                return 0;
            }
        }

        return incident_id;
    }

    bool add_police_report(uint64_t incident_id, const string &report_number)
    {
        IncidentReport incident = get_incident_by_id(incident_id);
        if (incident.incident_id == 0)
        {
            return false;
        }

        strncpy(incident.police_report_number, report_number.c_str(),
                sizeof(incident.police_report_number) - 1);

        return db_.update_incident(incident);
    }

    bool add_insurance_claim(uint64_t incident_id,
                             const string &claim_number,
                             double payout_amount)
    {
        IncidentReport incident = get_incident_by_id(incident_id);
        if (incident.incident_id == 0)
        {
            return false;
        }

        strncpy(incident.insurance_claim_number, claim_number.c_str(),
                sizeof(incident.insurance_claim_number) - 1);
        incident.insurance_payout = payout_amount;

        return db_.update_incident(incident);
    }

    // NEW METHODS ADDED FOR REQUEST HANDLER COMPATIBILITY

    bool resolve_incident(uint64_t incident_id, bool resolved, const string &resolution_notes = "")
    {
        IncidentReport incident = get_incident_by_id(incident_id);
        if (incident.incident_id == 0)
        {
            return false;
        }

        incident.is_resolved = resolved ? 1 : 0;
        incident.resolved_date = resolved ? get_current_timestamp() : 0;
        if (!resolution_notes.empty())
        {
            strncpy(incident.notes, resolution_notes.c_str(),
                    sizeof(incident.notes) - 1);
        }

        return db_.update_incident(incident);
    }

    bool mark_resolved(uint64_t incident_id)
    {
        return resolve_incident(incident_id, true);
    }

    IncidentReport get_incident_by_id(uint64_t incident_id)
    {
        IncidentReport incident;
        if (db_.read_incident(incident_id, incident))
        {
            return incident;
        }
        return IncidentReport(); // Return empty incident if not found
    }

    vector<IncidentReport> get_driver_incidents(uint64_t driver_id, int limit = 100)
    {
        return db_.get_incidents_by_driver(driver_id);
    }

    vector<IncidentReport> get_incidents_by_vehicle(uint64_t driver_id, uint64_t vehicle_id)
    {
        // Filter incidents by driver and vehicle
        vector<IncidentReport> incidents = get_driver_incidents(driver_id, 1000);
        vector<IncidentReport> result;

        for (const auto &incident : incidents)
        {
            if (incident.vehicle_id == vehicle_id)
            {
                result.push_back(incident);
            }
        }

        return result;
    }

    vector<IncidentReport> get_incidents_by_type(uint64_t driver_id, IncidentType type)
    {
        // Filter incidents by driver and type
        vector<IncidentReport> incidents = get_driver_incidents(driver_id, 1000);
        vector<IncidentReport> result;

        for (const auto &incident : incidents)
        {
            if (incident.type == type)
            {
                result.push_back(incident);
            }
        }

        return result;
    }

    vector<IncidentReport> get_unresolved_incidents(uint64_t driver_id)
    {
        vector<IncidentReport> incidents = get_driver_incidents(driver_id, 1000);
        vector<IncidentReport> result;

        for (const auto &incident : incidents)
        {
            if (incident.is_resolved == 0)
            {
                result.push_back(incident);
            }
        }

        return result;
    }

    vector<IncidentReport> get_vehicle_incidents(uint64_t vehicle_id)
    {
        return db_.get_incidents_by_vehicle(vehicle_id);
    }

    // NEW: Get incident statistics as used in RequestHandler
    struct IncidentStats
    {
        uint64_t driver_id;
        uint32_t total_incidents;
        uint32_t total_accidents;
        uint32_t total_breakdowns;
        uint32_t total_thefts;
        uint32_t total_violations;
        uint32_t resolved_incidents;
        uint32_t unresolved_incidents;
        double total_damage_cost;
        double total_insurance_payout;
        uint32_t incident_free_days;
    };
    IncidentStats get_incident_statistics(uint64_t driver_id)
    {
        IncidentStats stats = {};
        stats.driver_id = driver_id;

        auto incidents = get_driver_incidents(driver_id, 10000);

        uint64_t last_incident_time = 0;

        for (const auto &incident : incidents)
        {
            stats.total_incidents++;

            switch (incident.type)
            {
            case IncidentType::ACCIDENT:
                stats.total_accidents++;
                break;
            case IncidentType::BREAKDOWN:
                stats.total_breakdowns++;
                break;
            case IncidentType::THEFT:
                stats.total_thefts++;
                break;
            case IncidentType::VANDALISM:
                // Count as incident but not specific category
                break;
            case IncidentType::TRAFFIC_VIOLATION:
                stats.total_violations++;
                break;
            default:
                break;
            }

            if (incident.is_resolved)
            {
                stats.resolved_incidents++;
            }
            else
            {
                stats.unresolved_incidents++;
            }

            stats.total_damage_cost += incident.estimated_damage;
            stats.total_insurance_payout += incident.insurance_payout;

            if (incident.incident_time > last_incident_time)
            {
                last_incident_time = incident.incident_time;
            }
        }

        if (last_incident_time > 0)
        {
            uint64_t current_time = get_current_timestamp();
            uint64_t days_since = (current_time > last_incident_time) ? 
                                 (current_time - last_incident_time) / 86400ULL : 0;
            stats.incident_free_days = days_since;
        }

        return stats;
    }

private:
    uint64_t get_current_timestamp()
    {
        return static_cast<uint64_t>(time(nullptr));
    }
};

#endif