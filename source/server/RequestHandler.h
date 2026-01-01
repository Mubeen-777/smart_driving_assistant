#ifndef REQUESTHANDLER_H
#define REQUESTHANDLER_H

#include "../../include/sdm_types.hpp"
#include "../../source/core/DatabaseManager.h"
#include "../../source/core/CacheManager.h"
#include "../../source/core/SessionManager.h"
#include "../../source/core/TripManager.h"
#include "../../source/core/VehicleManager.h"
#include "../../source/core/ExpenseManager.h"
#include "../../source/core/DriverManager.h"
#include "../../source/core/IncidentManager.h"
#include "../../source/data_structures/MinHeap.h"
#include "../../source/data_structures/Map.h"

#include "ResponseBuilder.h"
#include <string>
#include <map>
#include <vector>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <ctime>
using namespace std;

class SimpleJSON
{
public:
    static Map<string, string> parse(const string &json)
    {
        Map<string, string> result;

        string cleaned = json;
        cleaned.erase(remove(cleaned.begin(), cleaned.end(), '{'), cleaned.end());
        cleaned.erase(remove(cleaned.begin(), cleaned.end(), '}'), cleaned.end());
        cleaned.erase(remove(cleaned.begin(), cleaned.end(), '\"'), cleaned.end());

        istringstream ss(cleaned);
        string token;

        while (getline(ss, token, ','))
        {
            size_t colon_pos = token.find(':');
            if (colon_pos != string::npos)
            {
                string key = token.substr(0, colon_pos);
                string value = token.substr(colon_pos + 1);

                key.erase(0, key.find_first_not_of(" \t\r\n"));
                key.erase(key.find_last_not_of(" \t\r\n") + 1);
                value.erase(0, value.find_first_not_of(" \t\r\n"));
                value.erase(value.find_last_not_of(" \t\r\n") + 1);

                result.insert(key, value);
            }
        }

        return result;
    }

    static string get_value(const Map<string, string> &data,
                            const string &key,
                            const string &default_value = "")
    {
        string value;
        if (data.get(key, value))
        {
            return value;
        }
        return default_value;
    }
};

class RequestHandler
{
private:
    DatabaseManager &db_;
    CacheManager &cache_;
    SessionManager &session_;

    TripManager &trip_mgr_;
    VehicleManager &vehicle_mgr_;
    ExpenseManager &expense_mgr_;
    DriverManager &driver_mgr_;

    IncidentManager &incident_mgr_;

    ResponseBuilder response_builder_;

    uint64_t safe_stoull(const string& value, uint64_t default_val = 0) {
        if (value.empty()) return default_val;
        try {
            return stoull(value);
        } catch (...) {
            return default_val;
        }
    }

    int safe_stoi(const string& value, int default_val = 0) {
        if (value.empty()) return default_val;
        try {
            return stoi(value);
        } catch (...) {
            return default_val;
        }
    }

    Map<string, string> trip_to_map(const TripRecord &trip)
    {
        Map<string, string> result;
        result.insert("trip_id", to_string(trip.trip_id));
        result.insert("driver_id", to_string(trip.driver_id));
        result.insert("vehicle_id", to_string(trip.vehicle_id));
        result.insert("start_time", to_string(trip.start_time));
        result.insert("end_time", to_string(trip.end_time));
        result.insert("duration", to_string(trip.duration));
        result.insert("distance", to_string(trip.distance));
        result.insert("avg_speed", to_string(trip.avg_speed));
        result.insert("max_speed", to_string(trip.max_speed));
        result.insert("fuel_consumed", to_string(trip.fuel_consumed));
        result.insert("fuel_efficiency", to_string(trip.fuel_efficiency));
        result.insert("harsh_braking_count", to_string(trip.harsh_braking_count));
        result.insert("rapid_acceleration_count", to_string(trip.rapid_acceleration_count));
        result.insert("speeding_count", to_string(trip.speeding_count));
        result.insert("start_address", string(trip.start_address, strnlen(trip.start_address, sizeof(trip.start_address))));
        result.insert("end_address", string(trip.end_address, strnlen(trip.end_address, sizeof(trip.end_address))));
        return result;
    }

    Map<string, string> vehicle_to_map(const VehicleInfo &vehicle)
    {
        Map<string, string> result;
        result.insert("vehicle_id", to_string(vehicle.vehicle_id));
        result.insert("owner_driver_id", to_string(vehicle.owner_driver_id));
        result.insert("license_plate", string(vehicle.license_plate, strnlen(vehicle.license_plate, sizeof(vehicle.license_plate))));
        result.insert("make", string(vehicle.make, strnlen(vehicle.make, sizeof(vehicle.make))));
        result.insert("model", string(vehicle.model, strnlen(vehicle.model, sizeof(vehicle.model))));
        result.insert("year", to_string(vehicle.year));
        result.insert("type", to_string(static_cast<int>(vehicle.type)));
        result.insert("current_odometer", to_string(vehicle.current_odometer));
        result.insert("fuel_type", string(vehicle.fuel_type, strnlen(vehicle.fuel_type, sizeof(vehicle.fuel_type))));
        result.insert("vin", string(vehicle.vin, strnlen(vehicle.vin, sizeof(vehicle.vin))));
        return result;
    }

    Map<string, string> expense_to_map(const ExpenseRecord &expense)
    {
        Map<string, string> result;
        result.insert("expense_id", to_string(expense.expense_id));
        result.insert("driver_id", to_string(expense.driver_id));
        result.insert("vehicle_id", to_string(expense.vehicle_id));
        result.insert("trip_id", to_string(expense.trip_id));
        result.insert("category", to_string(static_cast<int>(expense.category)));
        result.insert("expense_date", to_string(expense.expense_date));
        result.insert("amount", to_string(expense.amount));
        result.insert("currency", string(expense.currency, strnlen(expense.currency, sizeof(expense.currency))));
        result.insert("description", string(expense.description, strnlen(expense.description, sizeof(expense.description))));
        result.insert("fuel_quantity", to_string(expense.fuel_quantity));
        result.insert("fuel_price_per_unit", to_string(expense.fuel_price_per_unit));
        result.insert("fuel_station", string(expense.fuel_station, strnlen(expense.fuel_station, sizeof(expense.fuel_station))));
        return result;
    }

    Map<string, string> incident_to_map(const IncidentReport &incident)
    {
        Map<string, string> result;
        result.insert("incident_id", to_string(incident.incident_id));
        result.insert("driver_id", to_string(incident.driver_id));
        result.insert("vehicle_id", to_string(incident.vehicle_id));
        result.insert("trip_id", to_string(incident.trip_id));
        result.insert("type", to_string(static_cast<int>(incident.type)));
        result.insert("incident_time", to_string(incident.incident_time));
        result.insert("latitude", to_string(incident.latitude));
        result.insert("longitude", to_string(incident.longitude));
        result.insert("location_address", string(incident.location_address, strnlen(incident.location_address, sizeof(incident.location_address))));
        result.insert("description", string(incident.description, strnlen(incident.description, sizeof(incident.description))));
        result.insert("is_resolved", to_string(incident.is_resolved));
        return result;
    }

    Map<string, string> budget_alert_to_map(const ExpenseManager::BudgetAlert &alert)
    {
        Map<string, string> result;
        result.insert("driver_id", to_string(alert.driver_id));
        result.insert("category", to_string(static_cast<int>(alert.category)));
        result.insert("limit", to_string(alert.limit));
        result.insert("spent", to_string(alert.spent));
        result.insert("percentage_used", to_string(alert.percentage_used));
        result.insert("over_budget", alert.over_budget ? "1" : "0");
        return result;
    }

    Map<string, string> driver_ranking_to_map(const DriverManager::DriverRanking &ranking)
    {
        Map<string, string> result;
        result.insert("driver_id", to_string(ranking.driver_id));
        result.insert("driver_name", ranking.driver_name);
        result.insert("safety_score", to_string(ranking.safety_score));
        result.insert("total_distance", to_string(ranking.total_distance));
        result.insert("total_trips", to_string(ranking.total_trips));
        result.insert("avg_speed", to_string(ranking.avg_speed));
        result.insert("rank", to_string(ranking.rank));
        result.insert("percentile", to_string(ranking.percentile));
        return result;
    }

    Map<string, string> driver_recommendation_to_map(const DriverManager::DriverRecommendation &rec)
    {
        Map<string, string> result;
        result.insert("category", rec.category);
        result.insert("recommendation", rec.recommendation);
        result.insert("priority", to_string(rec.priority));
        result.insert("potential_improvement", to_string(rec.potential_improvement));
        return result;
    }

public:
    RequestHandler(DatabaseManager &db, CacheManager &cache, SessionManager &session,
                   TripManager &trip_mgr, VehicleManager &vehicle_mgr,
                   ExpenseManager &expense_mgr, DriverManager &driver_mgr, IncidentManager &incident_mgr)
        : db_(db), cache_(cache), session_(session),
          trip_mgr_(trip_mgr), vehicle_mgr_(vehicle_mgr), expense_mgr_(expense_mgr),
          driver_mgr_(driver_mgr),
          incident_mgr_(incident_mgr) {}

    string handle_request(const string &request_data,
                          const string &client_ip)
    {
        try
        {

            size_t body_start = request_data.find("\r\n\r\n");
            string body = (body_start != string::npos)
                              ? request_data.substr(body_start + 4)
                              : request_data;

            auto params = SimpleJSON::parse(body);
            string operation = SimpleJSON::get_value(params, "operation");
            string session_id = SimpleJSON::get_value(params, "session_id");

            if (operation == "user_login")
            {
                return handle_login(params);
            }
            else if (operation == "user_register")
            {
                return handle_register(params);
            }
            else if (operation == "user_logout")
            {
                return handle_logout(params);
            }

            SessionInfo session;
            if (!session_.validate_session(session_id, session))
            {
                return response_builder_.error("UNAUTHORIZED",
                                               "Invalid or expired session. Please login again.");
            }

            session_.increment_operation_count(session_id);

            if (operation.find("trip_") == 0)
            {
                return handle_trip_operation(operation, params, session_id);
            }
            else if (operation.find("vehicle_") == 0)
            {
                return handle_vehicle_operation(operation, params, session_id);
            }
            else if (operation.find("expense_") == 0)
            {
                return handle_expense_operation(operation, params, session_id);
            }
            else if (operation.find("driver_") == 0)
            {
                return handle_driver_operation(operation, params, session_id);
            }
            else if (operation.find("document_") == 0)
            {
                return handle_document_operation(operation, params, session_id);
            }
            else if (operation.find("incident_") == 0)
            {
                return handle_incident_operation(operation, params, session_id);
            }

            else
            {
                return response_builder_.error("UNKNOWN_OPERATION",
                                               "Unknown operation: " + operation);
            }
        }
        catch (const exception &e)
        {
            return response_builder_.error("INTERNAL_ERROR",
                                           string("Error processing request: ") + e.what());
        }
    }

    string handle_login(const Map<string, string> &params)
    {
        string username = SimpleJSON::get_value(params, "username");
        string password = SimpleJSON::get_value(params, "password");

        if (username.empty() || password.empty())
        {
            return response_builder_.error("INVALID_PARAMS",
                                           "Username and password are required");
        }

        string session_id;
        DriverProfile driver;

        if (session_.login(username, password, session_id, driver))
        {
            return response_builder_.success("LOGIN_SUCCESS", {{"session_id", session_id},
                                                               {"driver_id", to_string(driver.driver_id)},
                                                               {"name", driver.full_name},
                                                               {"role", to_string(static_cast<int>(driver.role))}});
        }
        else
        {
            return response_builder_.error("LOGIN_FAILED",
                                           "Invalid username or password");
        }
    }

    string handle_register(const Map<string, string> &params)
    {
        string username = SimpleJSON::get_value(params, "username");
        string password = SimpleJSON::get_value(params, "password");
        string full_name = SimpleJSON::get_value(params, "full_name");
        string email = SimpleJSON::get_value(params, "email");
        string phone = SimpleJSON::get_value(params, "phone");

        if (username.empty() || password.empty() || full_name.empty())
        {
            return response_builder_.error("INVALID_PARAMS",
                                           "Username, password, and full name are required");
        }

        if (session_.register_user(username, password, full_name, email, phone))
        {
            return response_builder_.success("REGISTER_SUCCESS", {{"message", "Account created successfully. Please login."}});
        }
        else
        {
            return response_builder_.error("REGISTER_FAILED",
                                           "Username already exists or registration failed");
        }
    }

    string handle_logout(const Map<string, string> &params)
    {
        string session_id = SimpleJSON::get_value(params, "session_id");

        session_.logout(session_id);

        return response_builder_.success("LOGOUT_SUCCESS", {{"message", "Logged out successfully"}});
    }

    string handle_trip_operation(const string &operation,
                                 const Map<string, string> &params,
                                 const string &session_id)
    {
        DriverProfile driver;
        if (!session_.get_driver_from_session(session_id, driver))
        {
            return response_builder_.error("SESSION_ERROR", "Could not retrieve driver info");
        }

        if (operation == "trip_start")
        {
            uint64_t vehicle_id = stoull(SimpleJSON::get_value(params, "vehicle_id", "0"));
            double start_lat = stod(SimpleJSON::get_value(params, "latitude", "0"));
            double start_lon = stod(SimpleJSON::get_value(params, "longitude", "0"));
            string address = SimpleJSON::get_value(params, "address", "");

            uint64_t trip_id = trip_mgr_.start_trip(driver.driver_id, vehicle_id,
                                                    start_lat, start_lon, address);

            if (trip_id > 0)
            {
                return response_builder_.success("TRIP_STARTED", {{"trip_id", to_string(trip_id)},
                                                                  {"message", "Trip started successfully"}});
            }
            else
            {
                return response_builder_.error("TRIP_START_FAILED",
                                               "Failed to start trip");
            }
        }
        else if (operation == "trip_log_gps")
        {
            uint64_t trip_id = stoull(SimpleJSON::get_value(params, "trip_id", "0"));
            double lat = stod(SimpleJSON::get_value(params, "latitude", "0"));
            double lon = stod(SimpleJSON::get_value(params, "longitude", "0"));
            float speed = stof(SimpleJSON::get_value(params, "speed", "0"));

            if (trip_mgr_.log_gps_point(trip_id, lat, lon, speed, 0.0f, 5.0f))
            {
                return response_builder_.success("GPS_LOGGED", {{"message", "GPS point logged"}});
            }
            else
            {
                return response_builder_.error("GPS_LOG_FAILED",
                                               "Failed to log GPS point");
            }
        }
        else if (operation == "trip_end")
        {
            uint64_t trip_id = stoull(SimpleJSON::get_value(params, "trip_id", "0"));
            double end_lat = stod(SimpleJSON::get_value(params, "latitude", "0"));
            double end_lon = stod(SimpleJSON::get_value(params, "longitude", "0"));
            string address = SimpleJSON::get_value(params, "address", "");

            if (trip_mgr_.end_trip(trip_id, end_lat, end_lon, address))
            {
                return response_builder_.success("TRIP_ENDED", {{"message", "Trip ended successfully"}});
            }
            else
            {
                return response_builder_.error("TRIP_END_FAILED",
                                               "Failed to end trip");
            }
        }
        else if (operation == "trip_get_history")
        {
            int limit = safe_stoi(SimpleJSON::get_value(params, "limit", "10"), 10);
            int offset = safe_stoi(SimpleJSON::get_value(params, "offset", "0"), 0);
            uint64_t vehicle_id = safe_stoull(SimpleJSON::get_value(params, "vehicle", "0"), 0);
            string status = SimpleJSON::get_value(params, "status", "all");
            string start_date_str = SimpleJSON::get_value(params, "startDate", "");
            string end_date_str = SimpleJSON::get_value(params, "endDate", "");

            uint64_t start_time = 0;
            uint64_t end_time = 0;

            if (start_date_str.length() >= 10) {
                struct tm tm = {0};
                if (strptime(start_date_str.c_str(), "%Y-%m-%d", &tm)) {
                    start_time = static_cast<uint64_t>(mktime(&tm));
                }
            }
            if (end_date_str.length() >= 10) {
                struct tm tm = {0};
                if (strptime(end_date_str.c_str(), "%Y-%m-%d", &tm)) {
                    
                    tm.tm_hour = 23;
                    tm.tm_min = 59;
                    tm.tm_sec = 59;
                    end_time = static_cast<uint64_t>(mktime(&tm));
                }
            }

            auto trips = trip_mgr_.search_trips(driver.driver_id, vehicle_id, status, 
                                              start_time, end_time, limit, offset);

            vector<Map<string, string>> trip_maps;
            for (const auto &trip : trips)
            {
                trip_maps.push_back(trip_to_map(trip));
            }

            return response_builder_.success_with_array("TRIP_HISTORY", "trips", trip_maps);
        }
        else if (operation == "trip_get_statistics")
        {
            auto stats = trip_mgr_.get_driver_statistics(driver.driver_id);

            return response_builder_.success("TRIP_STATISTICS", {{"total_trips", to_string(stats.total_trips)},
                                                                 {"total_distance", to_string(stats.total_distance)},
                                                                 {"avg_speed", to_string(stats.avg_speed)},
                                                                 {"safety_score", to_string(stats.safety_score)}});
        }
        else if (operation == "trip_get_active")
        {
            
            auto active_trip = trip_mgr_.get_active_trip(driver.driver_id);

            if (active_trip.trip_id > 0)
            {
                Map<string, string> trip_map;
                trip_map["trip_id"] = to_string(active_trip.trip_id);
                trip_map["driver_id"] = to_string(active_trip.driver_id);
                trip_map["vehicle_id"] = to_string(active_trip.vehicle_id);
                trip_map["start_time"] = to_string(active_trip.start_time);
                trip_map["start_address"] = string(active_trip.start_address, strnlen(active_trip.start_address, sizeof(active_trip.start_address)));

                Map<string, string> response_map;
                
                return response_builder_.success("ACTIVE_TRIP_FOUND", trip_map);
            }
            else
            {
                return response_builder_.success("NO_ACTIVE_TRIP", {{"trip", "null"}});
            }
        }

        return response_builder_.error("UNKNOWN_OPERATION",
                                       "Unknown trip operation: " + operation);
    }

    string handle_vehicle_operation(const string &operation,
                                    const Map<string, string> &params,
                                    const string &session_id)
    {
        DriverProfile driver;
        if (!session_.get_driver_from_session(session_id, driver))
        {
            return response_builder_.error("SESSION_ERROR", "Could not retrieve driver info");
        }

        if (operation == "vehicle_add")
        {
            string plate = SimpleJSON::get_value(params, "license_plate");
            string make = SimpleJSON::get_value(params, "make");
            string model = SimpleJSON::get_value(params, "model");
            uint32_t year = stoi(SimpleJSON::get_value(params, "year", "2020"));
            int type = stoi(SimpleJSON::get_value(params, "type", "0"));
            string vin = SimpleJSON::get_value(params, "vin", "");

            uint64_t vehicle_id = vehicle_mgr_.add_vehicle(
                plate, make, model, year, static_cast<VehicleType>(type),
                driver.driver_id, vin);

            if (vehicle_id > 0)
            {
                return response_builder_.success("VEHICLE_ADDED", {{"vehicle_id", to_string(vehicle_id)},
                                                                   {"message", "Vehicle added successfully"}});
            }
            else
            {
                return response_builder_.error("VEHICLE_ADD_FAILED",
                                               "Failed to add vehicle (plate may already exist)");
            }
        }
        else if (operation == "vehicle_get_list")
        {
            auto vehicles = vehicle_mgr_.get_driver_vehicles(driver.driver_id);

            vector<Map<string, string>> vehicle_maps;
            for (const auto &vehicle : vehicles)
            {
                vehicle_maps.push_back(vehicle_to_map(vehicle));
            }

            return response_builder_.success_with_array("VEHICLE_LIST", "vehicles", vehicle_maps);
        }
        else if (operation == "vehicle_update_odometer")
        {
            uint64_t vehicle_id = stoull(SimpleJSON::get_value(params, "vehicle_id", "0"));
            double reading = stod(SimpleJSON::get_value(params, "odometer", "0"));

            if (vehicle_mgr_.update_odometer(vehicle_id, reading))
            {
                return response_builder_.success("ODOMETER_UPDATED", {{"message", "Odometer updated successfully"}});
            }
            else
            {
                return response_builder_.error("ODOMETER_UPDATE_FAILED",
                                               "Failed to update odometer");
            }
        }
        else if (operation == "vehicle_add_maintenance")
        {
            uint64_t vehicle_id = stoull(SimpleJSON::get_value(params, "vehicle_id", "0"));
            int type = stoi(SimpleJSON::get_value(params, "type", "0"));
            double odometer = stod(SimpleJSON::get_value(params, "odometer", "0"));
            string center = SimpleJSON::get_value(params, "service_center");
            string description = SimpleJSON::get_value(params, "description");
            double cost = stod(SimpleJSON::get_value(params, "cost", "0"));

            uint64_t maintenance_id = vehicle_mgr_.add_maintenance_record(
                vehicle_id, driver.driver_id, static_cast<MaintenanceType>(type),
                odometer, center, description, cost);

            if (maintenance_id > 0)
            {
                return response_builder_.success("MAINTENANCE_ADDED", {{"maintenance_id", to_string(maintenance_id)},
                                                                       {"message", "Maintenance record added"}});
            }
            else
            {
                return response_builder_.error("MAINTENANCE_ADD_FAILED",
                                               "Failed to add maintenance record");
            }
        }
        else if (operation == "vehicle_get_alerts")
        {
            auto alerts = vehicle_mgr_.get_top_alerts(10);

            vector<Map<string, string>> alert_maps;
            for (const auto &alert : alerts)
            {
                Map<string, string> alert_map;
                alert_map["vehicle_id"] = to_string(alert.vehicle_id);
                alert_map["alert_id"] = to_string(alert.alert_id);
                alert_map["description"] = string(alert.description, strnlen(alert.description, sizeof(alert.description)));
                alert_map["priority"] = to_string(alert.priority);
                alert_map["due_timestamp"] = to_string(alert.due_timestamp);
                alert_map["severity"] = to_string(alert.severity);
                alert_maps.push_back(alert_map);
            }

            return response_builder_.success_with_array("MAINTENANCE_ALERTS", "alerts", alert_maps);
        }
        else if (operation == "vehicle_get_maintenance_history")
        {
            uint64_t vehicle_id = stoull(SimpleJSON::get_value(params, "vehicle_id", "0"));

            if (vehicle_id == 0)
            {
                return response_builder_.error("INVALID_PARAMS", "vehicle_id is required");
            }

            auto maintenance = vehicle_mgr_.get_vehicle_maintenance_history(vehicle_id);

            vector<Map<string, string>> maintenance_maps;
            for (const auto &m : maintenance)
            {
                Map<string, string> m_map;
                m_map["maintenance_id"] = to_string(m.maintenance_id);
                m_map["vehicle_id"] = to_string(m.vehicle_id);
                m_map["type"] = to_string(static_cast<int>(m.type));
                m_map["service_date"] = to_string(m.service_date);
                m_map["odometer_reading"] = to_string(m.odometer_reading);
                m_map["service_center"] = string(m.service_center, strnlen(m.service_center, sizeof(m.service_center)));
                m_map["description"] = string(m.description, strnlen(m.description, sizeof(m.description)));
                m_map["total_cost"] = to_string(m.total_cost);
                m_map["currency"] = string(m.currency, strnlen(m.currency, sizeof(m.currency)));
                maintenance_maps.push_back(m_map);
            }

            return response_builder_.success_with_array("MAINTENANCE_HISTORY", "maintenance", maintenance_maps);
        }

        return response_builder_.error("UNKNOWN_OPERATION",
                                       "Unknown vehicle operation: " + operation);
    }

    string handle_expense_operation(const string &operation,
                                    const Map<string, string> &params,
                                    const string &session_id)
    {
        DriverProfile driver;
        if (!session_.get_driver_from_session(session_id, driver))
        {
            return response_builder_.error("SESSION_ERROR", "Could not retrieve driver info");
        }

        if (operation == "expense_add")
        {
            uint64_t vehicle_id = stoull(SimpleJSON::get_value(params, "vehicle_id", "0"));
            int category = stoi(SimpleJSON::get_value(params, "category", "0"));
            double amount = stod(SimpleJSON::get_value(params, "amount", "0"));
            string description = SimpleJSON::get_value(params, "description");
            uint64_t trip_id = stoull(SimpleJSON::get_value(params, "trip_id", "0"));

            uint64_t expense_id = expense_mgr_.add_expense(
                driver.driver_id, vehicle_id, static_cast<ExpenseCategory>(category),
                amount, description, trip_id);

            if (expense_id > 0)
            {
                return response_builder_.success("EXPENSE_ADDED", {{"expense_id", to_string(expense_id)},
                                                                   {"message", "Expense added successfully"}});
            }
            else
            {
                return response_builder_.error("EXPENSE_ADD_FAILED",
                                               "Failed to add expense");
            }
        }
        else if (operation == "expense_add_fuel")
        {
            uint64_t vehicle_id = stoull(SimpleJSON::get_value(params, "vehicle_id", "0"));
            uint64_t trip_id = stoull(SimpleJSON::get_value(params, "trip_id", "0"));
            double quantity = stod(SimpleJSON::get_value(params, "quantity", "0"));
            double price_per_unit = stod(SimpleJSON::get_value(params, "price_per_unit", "0"));
            string station = SimpleJSON::get_value(params, "station");

            uint64_t expense_id = expense_mgr_.add_fuel_expense(
                driver.driver_id, vehicle_id, trip_id,
                quantity, price_per_unit, station);

            if (expense_id > 0)
            {
                return response_builder_.success("FUEL_EXPENSE_ADDED", {{"expense_id", to_string(expense_id)},
                                                                        {"message", "Fuel expense added"}});
            }
            else
            {
                return response_builder_.error("FUEL_EXPENSE_FAILED",
                                               "Failed to add fuel expense");
            }
        }
        else if (operation == "expense_get_list")
        {
            int limit = stoi(SimpleJSON::get_value(params, "limit", "100"));
            int category = stoi(SimpleJSON::get_value(params, "category", "-1"));
            uint64_t vehicle_id = stoull(SimpleJSON::get_value(params, "vehicle_id", "0"));

            vector<ExpenseRecord> expenses;
            if (category >= 0)
            {
                expenses = expense_mgr_.get_expenses_by_category(driver.driver_id, static_cast<ExpenseCategory>(category));
            }
            else
            {
                expenses = expense_mgr_.get_driver_expenses(driver.driver_id, limit);
            }

            if (vehicle_id > 0)
            {
                expenses.erase(remove_if(expenses.begin(), expenses.end(),
                                         [vehicle_id](const ExpenseRecord &e)
                                         { return e.vehicle_id != vehicle_id; }),
                               expenses.end());
            }

            vector<Map<string, string>> expense_maps;
            for (const auto &expense : expenses)
            {
                expense_maps.push_back(expense_to_map(expense));
            }

            return response_builder_.success_with_array("EXPENSE_LIST", "expenses", expense_maps);
        }
        else if (operation == "expense_get_summary")
        {
            string start_str = SimpleJSON::get_value(params, "start_date", "0");
            string end_str = SimpleJSON::get_value(params, "end_date", "0");

            uint64_t start_date = 0;
            uint64_t end_date = get_current_timestamp();

            if (start_str != "0" && !start_str.empty()) start_date = stoull(start_str);
            if (end_str != "0" && !end_str.empty()) end_date = stoull(end_str);

            auto summary = expense_mgr_.get_expense_summary_simple(driver.driver_id, start_date, end_date);

            return response_builder_.success("EXPENSE_SUMMARY", {
                {"total_expenses", to_string(summary.total_expenses)},
                {"fuel_expenses", to_string(summary.fuel_expenses)},
                {"maintenance_expenses", to_string(summary.maintenance_expenses)},
                {"insurance_expenses", to_string(summary.insurance_expenses)},
                {"toll_expenses", to_string(summary.toll_expenses)},
                {"parking_expenses", to_string(summary.parking_expenses)},
                {"other_expenses", to_string(summary.other_expenses)},
                {"total_transactions", to_string(summary.total_transactions)},
                {"avg_daily_expense", to_string(summary.average_daily_expense)}
            });
        }
        else if (operation == "expense_set_budget")
        {
            int category = stoi(SimpleJSON::get_value(params, "category", "0"));
            double limit = stod(SimpleJSON::get_value(params, "monthly_limit", "0"));

            if (expense_mgr_.set_budget_limit(driver.driver_id,
                                              static_cast<ExpenseCategory>(category), limit))
            {
                return response_builder_.success("BUDGET_SET", {{"message", "Budget limit set successfully"}});
            }
            else
            {
                return response_builder_.error("BUDGET_SET_FAILED",
                                               "Failed to set budget limit");
            }
        }
        else if (operation == "expense_get_budget_alerts")
        {
            auto alerts = expense_mgr_.get_budget_alerts(driver.driver_id);

            vector<Map<string, string>> alert_maps;
            for (const auto &alert : alerts)
            {
                alert_maps.push_back(budget_alert_to_map(alert));
            }

            return response_builder_.success_with_array("BUDGET_ALERTS", "alerts", alert_maps);
        }
        else if (operation == "expense_update")
        {
            uint64_t expense_id = stoull(SimpleJSON::get_value(params, "expense_id", "0"));
            uint64_t vehicle_id = stoull(SimpleJSON::get_value(params, "vehicle_id", "0"));
            int category = stoi(SimpleJSON::get_value(params, "category", "0"));
            double amount = stod(SimpleJSON::get_value(params, "amount", "0"));
            string description = SimpleJSON::get_value(params, "description");

            auto existing_expense = expense_mgr_.get_expense_by_id(expense_id);
            if (existing_expense.driver_id != driver.driver_id)
            {
                return response_builder_.error("UNAUTHORIZED", "You can only update your own expenses");
            }

            if (expense_mgr_.update_expense(expense_id, vehicle_id, static_cast<ExpenseCategory>(category),
                                            amount, description))
            {
                return response_builder_.success("EXPENSE_UPDATED", {{"message", "Expense updated successfully"}});
            }
            else
            {
                return response_builder_.error("EXPENSE_UPDATE_FAILED", "Failed to update expense");
            }
        }
        else if (operation == "expense_delete")
        {
            uint64_t expense_id = stoull(SimpleJSON::get_value(params, "expense_id", "0"));

            auto existing_expense = expense_mgr_.get_expense_by_id(expense_id);
            if (existing_expense.driver_id != driver.driver_id)
            {
                return response_builder_.error("UNAUTHORIZED", "You can only delete your own expenses");
            }

            if (expense_mgr_.delete_expense(expense_id))
            {
                return response_builder_.success("EXPENSE_DELETED", {{"message", "Expense deleted successfully"}});
            }
            else
            {
                return response_builder_.error("EXPENSE_DELETE_FAILED", "Failed to delete expense");
            }
        }

        return response_builder_.error("UNKNOWN_OPERATION",
                                       "Unknown expense operation: " + operation);
    }

    string handle_driver_operation(const string &operation,
                                   const Map<string, string> &params,
                                   const string &session_id)
    {
        DriverProfile driver;
        if (!session_.get_driver_from_session(session_id, driver))
        {
            return response_builder_.error("SESSION_ERROR", "Could not retrieve driver info");
        }

        if (operation == "driver_get_profile")
        {
            return response_builder_.success("DRIVER_PROFILE", {{"driver_id", to_string(driver.driver_id)},
                                                                {"name", string(driver.full_name, strnlen(driver.full_name, sizeof(driver.full_name)))},
                                                                {"email", string(driver.email, strnlen(driver.email, sizeof(driver.email)))},
                                                                {"phone", string(driver.phone, strnlen(driver.phone, sizeof(driver.phone)))},
                                                                {"safety_score", to_string(driver.safety_score)},
                                                                {"total_trips", to_string(driver.total_trips)},
                                                                {"total_distance", to_string(driver.total_distance)}});
        }
        else if (operation == "driver_update_profile")
        {
            string name = SimpleJSON::get_value(params, "full_name", driver.full_name);
            string email = SimpleJSON::get_value(params, "email", driver.email);
            string phone = SimpleJSON::get_value(params, "phone", driver.phone);

            if (driver_mgr_.update_driver_profile(driver.driver_id, name, email, phone))
            {
                return response_builder_.success("PROFILE_UPDATED", {{"message", "Profile updated successfully"}});
            }
            else
            {
                return response_builder_.error("PROFILE_UPDATE_FAILED",
                                               "Failed to update profile");
            }
        }
        else if (operation == "driver_get_behavior")
        {
            auto behavior = driver_mgr_.get_driver_behavior(driver.driver_id);

            return response_builder_.success("DRIVER_BEHAVIOR", {{"safety_score", to_string(behavior.safety_score)},
                                                                 {"total_trips", to_string(behavior.total_trips)},
                                                                 {"total_distance", to_string(behavior.total_distance)},
                                                                 {"harsh_braking_rate", to_string(behavior.harsh_braking_rate)},
                                                                 {"avg_speed", to_string(behavior.avg_speed)},
                                                                 {"rank", to_string(behavior.rank_in_fleet)},
                                                                 {"percentile", to_string(behavior.percentile)}});
        }
        else if (operation == "driver_get_leaderboard")
        {
            int limit = stoi(SimpleJSON::get_value(params, "limit", "10"));
            string sort_by = SimpleJSON::get_value(params, "sort_by", "score");
            string time_period = SimpleJSON::get_value(params, "time_period", "all");

            auto leaderboard = driver_mgr_.get_driver_leaderboard(limit, sort_by, time_period);

            vector<Map<string, string>> ranking_maps;
            for (const auto &ranking : leaderboard)
            {
                ranking_maps.push_back(driver_ranking_to_map(ranking));
            }

            return response_builder_.success_with_array("DRIVER_LEADERBOARD", "leaderboard", ranking_maps);
        }
        else if (operation == "driver_get_recommendations")
        {
            auto recommendations = driver_mgr_.get_improvement_recommendations(driver.driver_id);

            vector<Map<string, string>> rec_maps;
            for (const auto &rec : recommendations)
            {
                rec_maps.push_back(driver_recommendation_to_map(rec));
            }

            return response_builder_.success_with_array("DRIVER_RECOMMENDATIONS", "recommendations", rec_maps);
        }
        else if (operation == "driver_report_event")
        {
            string event_type = SimpleJSON::get_value(params, "event_type");
            string description = SimpleJSON::get_value(params, "description");
            int point_deduction = stoi(SimpleJSON::get_value(params, "point_deduction", "0"));
            uint64_t trip_id = stoull(SimpleJSON::get_value(params, "trip_id", "0"));

            if (driver_mgr_.report_driver_event(driver.driver_id, event_type, description, point_deduction, trip_id))
            {
                return response_builder_.success("EVENT_REPORTED", {{"message", "Event reported successfully"}});
            }
            else
            {
                return response_builder_.error("EVENT_REPORT_FAILED", "Failed to report event");
            }
        }

        return response_builder_.error("UNKNOWN_OPERATION",
                                       "Unknown driver operation: " + operation);
    }

    string handle_document_operation(const string &operation,
                                     const Map<string, string> &params,
                                     const string &session_id)
    {

        return response_builder_.success("DOCUMENT_OPERATION", {{"message", "Document operations require binary data upload"}});
    }

    string handle_incident_operation(const string &operation,
                                     const Map<string, string> &params,
                                     const string &session_id)
    {
        DriverProfile driver;
        if (!session_.get_driver_from_session(session_id, driver))
        {
            return response_builder_.error("SESSION_ERROR", "Could not retrieve driver info");
        }

        if (operation == "incident_report")
        {
            uint64_t vehicle_id = stoull(SimpleJSON::get_value(params, "vehicle_id", "0"));
            int type = stoi(SimpleJSON::get_value(params, "type", "0"));
            double lat = stod(SimpleJSON::get_value(params, "latitude", "0"));
            double lon = stod(SimpleJSON::get_value(params, "longitude", "0"));
            string description = SimpleJSON::get_value(params, "description");

            uint64_t incident_id = incident_mgr_.report_incident(
                driver.driver_id, vehicle_id, static_cast<IncidentType>(type),
                lat, lon, "", description);

            if (incident_id > 0)
            {
                return response_builder_.success("INCIDENT_REPORTED", {{"incident_id", to_string(incident_id)},
                                                                       {"message", "Incident reported successfully"}});
            }
            else
            {
                return response_builder_.error("INCIDENT_REPORT_FAILED",
                                               "Failed to report incident");
            }
        }
        else if (operation == "incident_get_list")
        {
            int limit = stoi(SimpleJSON::get_value(params, "limit", "100"));
            int type = stoi(SimpleJSON::get_value(params, "type", "-1"));
            string status = SimpleJSON::get_value(params, "status", "all");
            uint64_t vehicle_id = stoull(SimpleJSON::get_value(params, "vehicle_id", "0"));
            uint64_t start_date = stoull(SimpleJSON::get_value(params, "start_date", "0"));
            uint64_t end_date = stoull(SimpleJSON::get_value(params, "end_date", to_string(get_current_timestamp())));

            vector<IncidentReport> incidents;

            if (type >= 0)
            {
                incidents = incident_mgr_.get_incidents_by_type(driver.driver_id, static_cast<IncidentType>(type));
            }
            else if (vehicle_id > 0)
            {
                incidents = incident_mgr_.get_incidents_by_vehicle(driver.driver_id, vehicle_id);
            }
            else
            {
                incidents = incident_mgr_.get_driver_incidents(driver.driver_id, limit);
            }

            if (status != "all")
            {
                int status_code = 0;
                if (status == "resolved")
                    status_code = 1;
                else if (status == "investigating")
                    status_code = 2;

                incidents.erase(remove_if(incidents.begin(), incidents.end(),
                                          [status_code](const IncidentReport &incident)
                                          { return incident.is_resolved != status_code; }),
                                incidents.end());
            }

            if (start_date > 0 || end_date > 0)
            {
                auto it = remove_if(incidents.begin(), incidents.end(),
                                    [start_date, end_date](const IncidentReport &incident)
                                    {
                                        
                                        if (start_date > 0 && incident.incident_time < start_date)
                                        {
                                            return true;
                                        }

                                        if (end_date > 0 && incident.incident_time > end_date)
                                        {
                                            return true;
                                        }

                                        return false;
                                    });

                incidents.erase(it, incidents.end());
            }

            vector<Map<string, string>> incident_maps;
            for (const auto &incident : incidents)
            {
                incident_maps.push_back(incident_to_map(incident));
            }

            return response_builder_.success_with_array("INCIDENT_LIST", "incidents", incident_maps);
        }
        else if (operation == "incident_get_statistics")
        {
            auto stats = incident_mgr_.get_incident_statistics(driver.driver_id);

            return response_builder_.success("INCIDENT_STATISTICS", {{"total_incidents", to_string(stats.total_incidents)},
                                                                     {"total_accidents", to_string(stats.total_accidents)},
                                                                     {"total_breakdowns", to_string(stats.total_breakdowns)},
                                                                     {"unresolved_incidents", to_string(stats.unresolved_incidents)},
                                                                     {"incident_free_days", to_string(stats.incident_free_days)}});
        }
        else if (operation == "incident_resolve")
        {
            uint64_t incident_id = stoull(SimpleJSON::get_value(params, "incident_id", "0"));
            bool resolved = SimpleJSON::get_value(params, "resolved") == "true" ||
                            SimpleJSON::get_value(params, "resolved") == "1";
            string resolution_notes = SimpleJSON::get_value(params, "resolution_notes", "");

            auto incident = incident_mgr_.get_incident_by_id(incident_id);
            if (incident.driver_id != driver.driver_id)
            {
                return response_builder_.error("UNAUTHORIZED", "You can only resolve your own incidents");
            }

            if (incident_mgr_.resolve_incident(incident_id, resolved, resolution_notes))
            {
                return response_builder_.success("INCIDENT_RESOLVED", {{"message", "Incident resolved successfully"}});
            }
            else
            {
                return response_builder_.error("INCIDENT_RESOLVE_FAILED",
                                               "Failed to resolve incident");
            }
        }

        return response_builder_.error("UNKNOWN_OPERATION",
                                       "Unknown incident operation: " + operation);
    }

private:
    uint64_t get_current_timestamp()
    {
        return static_cast<uint64_t>(time(nullptr));
    }
};
#endif