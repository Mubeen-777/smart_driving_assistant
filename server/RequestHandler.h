#ifndef REQUESTHANDLER_H
#define REQUESTHANDLER_H

#include "../../include/sdm_types.hpp"
#include "../../source/core/DatabaseManager.h"
#include "../../source/core/CacheManager.h"
#include "../../source/core/SessionManager.h"
#include "../../source/modules/TripManager.h"
#include "../../source/modules/VehicleManager.h"
#include "../../source/modules/ExpenseManager.h"
#include "../../source/modules/DriverManager.h"
#include "../../source/modules/DocumentManager.h"
#include "../../source/modules/IncidentManager.h"
#include "../../source/modules/AnalyticsEngine.h"
#include "../../source/modules/VisionManager.h"
#include "ResponseBuilder.h"
#include <string>
#include <map>
#include <sstream>
#include <algorithm>
#include <iostream>
using namespace std;

class SimpleJSON
{
public:
    static map<string, string> parse(const string &json)
    {
        map<string, string> result;

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

                result[key] = value;
            }
        }

        return result;
    }

    static string get_value(const map<string, string> &data,
                            const string &key,
                            const string &default_value = "")
    {
        auto it = data.find(key);
        return (it != data.end()) ? it->second : default_value;
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
    DocumentManager &document_mgr_;
    IncidentManager &incident_mgr_;
    AnalyticsEngine &analytics_;
    VisionManager *vision_mgr_;

    ResponseBuilder response_builder_;

public:
    RequestHandler(DatabaseManager &db, CacheManager &cache, SessionManager &session,
                   TripManager &trip_mgr, VehicleManager &vehicle_mgr,
                   ExpenseManager &expense_mgr, DriverManager &driver_mgr,
                   DocumentManager &document_mgr, IncidentManager &incident_mgr,
                   AnalyticsEngine &analytics, VisionManager *vision_mgr)
        : db_(db), cache_(cache), session_(session),
          trip_mgr_(trip_mgr), vehicle_mgr_(vehicle_mgr), expense_mgr_(expense_mgr),
          driver_mgr_(driver_mgr), document_mgr_(document_mgr),
          incident_mgr_(incident_mgr), analytics_(analytics), vision_mgr_(vision_mgr) {}

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
            else if (operation.find("analytics_") == 0)
            {
                return handle_analytics_operation(operation, params, session_id);
            }
            else if (operation.find("vision_") == 0)
            {
                return handle_vision_operation(operation, params, session_id);
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

    string handle_login(const map<string, string> &params)
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

    string handle_register(const map<string, string> &params)
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

    string handle_logout(const map<string, string> &params)
    {
        string session_id = SimpleJSON::get_value(params, "session_id");

        session_.logout(session_id);

        return response_builder_.success("LOGOUT_SUCCESS", {{"message", "Logged out successfully"}});
    }

    string handle_trip_operation(const string &operation,
                                 const map<string, string> &params,
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

            if (trip_mgr_.log_gps_point(trip_id, lat, lon, speed))
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
            int limit = stoi(SimpleJSON::get_value(params, "limit", "10"));

            auto trips = trip_mgr_.get_driver_trips(driver.driver_id, limit);

            return response_builder_.success("TRIP_HISTORY", {{"count", to_string(trips.size())},
                                                              {"message", "Retrieved " + to_string(trips.size()) + " trips"}});
        }
        else if (operation == "trip_get_statistics")
        {
            auto stats = trip_mgr_.get_driver_statistics(driver.driver_id);

            return response_builder_.success("TRIP_STATISTICS", {{"total_trips", to_string(stats.total_trips)},
                                                                 {"total_distance", to_string(stats.total_distance)},
                                                                 {"avg_speed", to_string(stats.avg_speed)},
                                                                 {"safety_score", to_string(stats.safety_score)}});
        }

        return response_builder_.error("UNKNOWN_OPERATION",
                                       "Unknown trip operation: " + operation);
    }

    string handle_vehicle_operation(const string &operation,
                                    const map<string, string> &params,
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

            return response_builder_.success("VEHICLE_LIST", {{"count", to_string(vehicles.size())},
                                                              {"message", "Retrieved " + to_string(vehicles.size()) + " vehicles"}});
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

            return response_builder_.success("MAINTENANCE_ALERTS", {{"count", to_string(alerts.size())},
                                                                    {"message", "Retrieved " + to_string(alerts.size()) + " alerts"}});
        }

        return response_builder_.error("UNKNOWN_OPERATION",
                                       "Unknown vehicle operation: " + operation);
    }

    string handle_expense_operation(const string &operation,
                                    const map<string, string> &params,
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
        else if (operation == "expense_get_summary")
        {
            uint64_t start_date = stoull(SimpleJSON::get_value(params, "start_date", "0"));
            uint64_t end_date = stoull(SimpleJSON::get_value(params, "end_date",
                                                             to_string(get_current_timestamp())));

            auto summary = expense_mgr_.get_expense_summary(driver.driver_id, start_date, end_date);

            return response_builder_.success("EXPENSE_SUMMARY", {{"total_expenses", to_string(summary.total_expenses)},
                                                                 {"fuel_expenses", to_string(summary.fuel_expenses)},
                                                                 {"maintenance_expenses", to_string(summary.maintenance_expenses)},
                                                                 {"total_transactions", to_string(summary.total_transactions)},
                                                                 {"avg_daily_expense", to_string(summary.average_daily_expense)}});
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

            return response_builder_.success("BUDGET_ALERTS", {{"count", to_string(alerts.size())},
                                                               {"message", "Retrieved " + to_string(alerts.size()) + " budget alerts"}});
        }

        return response_builder_.error("UNKNOWN_OPERATION",
                                       "Unknown expense operation: " + operation);
    }

    string handle_driver_operation(const string &operation,
                                   const map<string, string> &params,
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
                                                                {"name", driver.full_name},
                                                                {"email", driver.email},
                                                                {"phone", driver.phone},
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

            auto leaderboard = driver_mgr_.get_driver_leaderboard(limit);

            return response_builder_.success("DRIVER_LEADERBOARD", {{"count", to_string(leaderboard.size())},
                                                                    {"message", "Retrieved top " + to_string(leaderboard.size()) + " drivers"}});
        }
        else if (operation == "driver_get_recommendations")
        {
            auto recommendations = driver_mgr_.get_improvement_recommendations(driver.driver_id);

            return response_builder_.success("DRIVER_RECOMMENDATIONS", {{"count", to_string(recommendations.size())},
                                                                        {"message", "Retrieved " + to_string(recommendations.size()) + " recommendations"}});
        }

        return response_builder_.error("UNKNOWN_OPERATION",
                                       "Unknown driver operation: " + operation);
    }

    string handle_document_operation(const string &operation,
                                     const map<string, string> &params,
                                     const string &session_id)
    {

        return response_builder_.success("DOCUMENT_OPERATION", {{"message", "Document operations require binary data upload"}});
    }

    string handle_incident_operation(const string &operation,
                                     const map<string, string> &params,
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
            auto incidents = incident_mgr_.get_driver_incidents(driver.driver_id);

            return response_builder_.success("INCIDENT_LIST", {{"count", to_string(incidents.size())},
                                                               {"message", "Retrieved " + to_string(incidents.size()) + " incidents"}});
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

        return response_builder_.error("UNKNOWN_OPERATION",
                                       "Unknown incident operation: " + operation);
    }

    string handle_analytics_operation(const string &operation,
                                      const map<string, string> &params,
                                      const string &session_id)
    {
        DriverProfile driver;
        if (!session_.get_driver_from_session(session_id, driver))
        {
            return response_builder_.error("SESSION_ERROR", "Could not retrieve driver info");
        }

        analytics_.initialize_driver_analytics(driver.driver_id);

        if (operation == "analytics_get_dashboard")
        {
            auto dashboard = analytics_.get_dashboard_metrics(driver.driver_id);

            return response_builder_.success("ANALYTICS_DASHBOARD", {{"today_distance", to_string(dashboard.today.total_distance)},
                                                                     {"today_trips", to_string(dashboard.today.total_trips)},
                                                                     {"week_distance", to_string(dashboard.this_week.total_distance)},
                                                                     {"month_distance", to_string(dashboard.this_month.total_distance)},
                                                                     {"month_expenses", to_string(dashboard.this_month.total_expenses)},
                                                                     {"safety_score", to_string(dashboard.this_month.safety_score)},
                                                                     {"maintenance_alerts", to_string(dashboard.pending_maintenance_alerts)},
                                                                     {"budget_alerts", to_string(dashboard.budget_alerts)}});
        }
        else if (operation == "analytics_get_monthly")
        {
            int year = stoi(SimpleJSON::get_value(params, "year", "2024"));
            int month = stoi(SimpleJSON::get_value(params, "month", "1"));

            auto monthly = analytics_.get_monthly_report(driver.driver_id, year, month);

            return response_builder_.success("MONTHLY_ANALYTICS", {{"total_distance", to_string(monthly.total_distance)},
                                                                   {"total_trips", to_string(monthly.total_trips)},
                                                                   {"total_expenses", to_string(monthly.total_expenses)},
                                                                   {"avg_speed", to_string(monthly.avg_speed)},
                                                                   {"fuel_efficiency", to_string(monthly.avg_fuel_efficiency)},
                                                                   {"safety_score", to_string(monthly.safety_score)}});
        }
        else if (operation == "analytics_get_yearly")
        {
            int year = stoi(SimpleJSON::get_value(params, "year", "2024"));

            auto yearly = analytics_.get_yearly_report(driver.driver_id, year);

            return response_builder_.success("YEARLY_ANALYTICS", {{"total_distance", to_string(yearly.total_distance)},
                                                                  {"total_trips", to_string(yearly.total_trips)},
                                                                  {"total_expenses", to_string(yearly.total_expenses)},
                                                                  {"avg_daily_distance", to_string(yearly.avg_daily_distance)},
                                                                  {"safety_score", to_string(yearly.safety_score)}});
        }
        else if (operation == "analytics_get_predictions")
        {
            auto predictions = analytics_.generate_predictions(driver.driver_id);

            return response_builder_.success("ANALYTICS_PREDICTIONS", {{"predicted_distance", to_string(predictions.predicted_distance)},
                                                                       {"predicted_fuel", to_string(predictions.predicted_fuel_consumption)},
                                                                       {"predicted_expenses", to_string(predictions.predicted_expenses)},
                                                                       {"predicted_safety_score", to_string(predictions.predicted_safety_score)},
                                                                       {"risk_factors_count", to_string(predictions.risk_factors.size())},
                                                                       {"cost_saving_opportunities", to_string(predictions.cost_saving_opportunities.size())}});
        }

        return response_builder_.error("UNKNOWN_OPERATION",
                                       "Unknown analytics operation: " + operation);
    }

    string handle_vision_operation(const string &operation,
                                   const map<string, string> &params,
                                   const string &session_id)
    {
        if (!vision_mgr_)
        {
            return response_builder_.error("VISION_UNAVAILABLE",
                                           "Vision system is not available");
        }

        if (operation == "vision_get_collision_warnings")
        {
            auto warnings = vision_mgr_->get_collision_warnings(50);

            return response_builder_.success("COLLISION_WARNINGS", {{"count", to_string(warnings.size())},
                                                                    {"message", "Retrieved " + to_string(warnings.size()) + " collision warnings"}});
        }
        else if (operation == "vision_get_drowsiness_alerts")
        {
            auto alerts = vision_mgr_->get_drowsiness_alerts(50);

            return response_builder_.success("DROWSINESS_ALERTS", {{"count", to_string(alerts.size())},
                                                                   {"message", "Retrieved " + to_string(alerts.size()) + " drowsiness alerts"}});
        }
        else if (operation == "vision_get_lane_warnings")
        {
            auto warnings = vision_mgr_->get_lane_warnings(50);

            return response_builder_.success("LANE_WARNINGS", {{"count", to_string(warnings.size())},
                                                               {"message", "Retrieved " + to_string(warnings.size()) + " lane warnings"}});
        }
        else if (operation == "vision_get_traffic_signs")
        {
            auto signs = vision_mgr_->get_traffic_signs(50);

            return response_builder_.success("TRAFFIC_SIGNS", {{"count", to_string(signs.size())},
                                                               {"message", "Retrieved " + to_string(signs.size()) + " traffic signs"}});
        }
        else if (operation == "vision_get_pedestrian_alerts")
        {
            auto alerts = vision_mgr_->get_pedestrian_alerts(50);

            return response_builder_.success("PEDESTRIAN_ALERTS", {{"count", to_string(alerts.size())},
                                                                   {"message", "Retrieved " + to_string(alerts.size()) + " pedestrian alerts"}});
        }

        return response_builder_.error("UNKNOWN_OPERATION",
                                       "Unknown vision operation: " + operation);
    }

private:
    uint64_t get_current_timestamp()
    {
        return chrono::system_clock::now().time_since_epoch().count();
    }
};
#endif