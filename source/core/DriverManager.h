#ifndef DRIVERMANAGER_H
#define DRIVERMANAGER_H

#include "../../include/sdm_types.hpp"
#include "DatabaseManager.h"
#include "CacheManager.h"
#include "IndexManager.h"
#include <vector>
#include <algorithm>
#include <string>
#include <chrono>
#include <iostream>

class DriverManager
{
private:
    DatabaseManager &db_;
    CacheManager &cache_;
    IndexManager &index_;

public:
    DriverManager(DatabaseManager &db, CacheManager &cache, IndexManager &index)
        : db_(db), cache_(cache), index_(index) {}

    // ========================================================================
    // DRIVER PROFILE OPERATIONS
    // ========================================================================

    bool update_driver_profile(uint64_t driver_id,
                               const std::string &full_name,
                               const std::string &email,
                               const std::string &phone)
    {
        DriverProfile driver;
        if (!db_.read_driver(driver_id, driver))
        {
            return false;
        }

        strncpy(driver.full_name, full_name.c_str(), sizeof(driver.full_name) - 1);
        strncpy(driver.email, email.c_str(), sizeof(driver.email) - 1);
        strncpy(driver.phone, phone.c_str(), sizeof(driver.phone) - 1);

        if (db_.update_driver(driver))
        {
            cache_.invalidate_driver(driver_id);
            return true;
        }

        return false;
    }

    bool update_license_info(uint64_t driver_id,
                             const std::string &license_number,
                             uint64_t expiry_date)
    {
        DriverProfile driver;
        if (!db_.read_driver(driver_id, driver))
        {
            return false;
        }

        strncpy(driver.license_number, license_number.c_str(), sizeof(driver.license_number) - 1);
        driver.license_expiry = expiry_date;

        if (db_.update_driver(driver))
        {
            cache_.invalidate_driver(driver_id);
            return true;
        }

        return false;
    }

    bool get_driver_profile(uint64_t driver_id, DriverProfile &driver)
    {
        // Try cache first
        if (cache_.get_driver(driver_id, driver))
        {
            return true;
        }

        // Fetch from database
        if (db_.read_driver(driver_id, driver))
        {
            cache_.put_driver(driver_id, driver);
            return true;
        }

        return false;
    }

    // ========================================================================
    // DRIVER BEHAVIOR & SCORING
    // ========================================================================

    struct DriverBehaviorMetrics
    {
        uint64_t driver_id;
        uint32_t safety_score; // 0-1000

        // Trip statistics
        uint64_t total_trips;
        double total_distance;
        double total_duration; // hours

        // Driving events
        uint32_t harsh_braking_count;
        uint32_t rapid_acceleration_count;
        uint32_t speeding_violations;
        uint32_t sharp_turns;

        // Event rates (per 100km)
        double harsh_braking_rate;
        double acceleration_rate;
        double speeding_rate;

        // Fuel efficiency
        double avg_fuel_efficiency; // km/L

        // Speed statistics
        double avg_speed;
        double max_speed_recorded;

        // Time-of-day patterns
        uint32_t night_driving_trips; // 10pm - 6am
        uint32_t peak_hour_trips;     // 7-9am, 5-7pm

        // Vision analytics (if available)
        uint32_t drowsiness_events;
        uint32_t distraction_events;
        uint32_t collision_warnings;

        // Ranking
        uint32_t rank_in_fleet; // Position among all drivers
        double percentile;      // Top X%
    };

    DriverBehaviorMetrics get_driver_behavior(uint64_t driver_id)
    {
        DriverBehaviorMetrics metrics = {};
        metrics.driver_id = driver_id;

        DriverProfile driver;
        if (!get_driver_profile(driver_id, driver))
        {
            return metrics;
        }

        // Populate from driver profile
        metrics.safety_score = driver.safety_score;
        metrics.total_trips = driver.total_trips;
        metrics.total_distance = driver.total_distance;
        metrics.harsh_braking_count = driver.harsh_events_count;

        // Calculate rates
        if (metrics.total_distance > 0)
        {
            metrics.harsh_braking_rate = (metrics.harsh_braking_count / metrics.total_distance) * 100;
        }

        // Get ranking
        calculate_driver_ranking(driver_id, metrics);

        return metrics;
    }

    uint32_t calculate_safety_score(const DriverBehaviorMetrics &metrics)
    {
        uint32_t score = 1000; // Perfect score

        // Deduct for harsh events
        score -= std::min(score, (uint32_t)(metrics.harsh_braking_rate * 10));
        score -= std::min(score, (uint32_t)(metrics.acceleration_rate * 10));
        score -= std::min(score, (uint32_t)(metrics.speeding_rate * 15));

        // Deduct for vision-detected issues
        score -= std::min(score, metrics.drowsiness_events * 20);
        score -= std::min(score, metrics.distraction_events * 15);

        // Bonus for safe driving
        if (metrics.total_distance > 10000 && score > 900)
        {
            score += 50; // Experienced safe driver bonus
        }

        return std::min((uint32_t)1000, score);
    }

    // ========================================================================
    // DRIVER COMPARISON & RANKING
    // ========================================================================

    struct DriverRanking
    {
        uint64_t driver_id;
        std::string driver_name;
        uint32_t safety_score;
        double total_distance;
        uint64_t total_trips;
        double avg_speed;
        uint32_t rank;
        double percentile;
    };

    std::vector<DriverRanking> get_driver_leaderboard(int limit = 10, 
                                                      const std::string& sort_by = "score",
                                                      const std::string& time_period = "all")
    {
        std::vector<DriverRanking> rankings;

        auto all_drivers = db_.get_all_drivers();

        for (const auto &driver : all_drivers)
        {
            DriverRanking rank;
            rank.driver_id = driver.driver_id;
            rank.driver_name = std::string(driver.full_name, strnlen(driver.full_name, sizeof(driver.full_name)));
            rank.safety_score = driver.safety_score;
            rank.total_distance = driver.total_distance;
            rank.total_trips = driver.total_trips;
            
            // Calculate average speed from trips
            auto trips = db_.get_trips_by_driver(driver.driver_id, 100);
            double total_duration = 0;
            for (const auto& trip : trips) {
                total_duration += trip.duration;
            }
            if (total_duration > 0) {
                rank.avg_speed = (driver.total_distance / total_duration) * 3600;
            } else {
                rank.avg_speed = 0;
            }
            
            rankings.push_back(rank);
        }

        // Sort based on sort_by parameter
        if (sort_by == "score") {
            std::sort(rankings.begin(), rankings.end(),
                      [](const DriverRanking &a, const DriverRanking &b)
                      {
                          if (a.safety_score == b.safety_score)
                          {
                              return a.total_distance > b.total_distance;
                          }
                          return a.safety_score > b.safety_score;
                      });
        } else if (sort_by == "distance") {
            std::sort(rankings.begin(), rankings.end(),
                      [](const DriverRanking &a, const DriverRanking &b)
                      {
                          return a.total_distance > b.total_distance;
                      });
        } else if (sort_by == "trips") {
            std::sort(rankings.begin(), rankings.end(),
                      [](const DriverRanking &a, const DriverRanking &b)
                      {
                          return a.total_trips > b.total_trips;
                      });
        }

        // Assign ranks and percentiles
        for (size_t i = 0; i < rankings.size(); i++)
        {
            rankings[i].rank = i + 1;
            rankings[i].percentile = ((double)(rankings.size() - i) / rankings.size()) * 100.0;
        }

        // Return top N
        if (rankings.size() > limit)
        {
            rankings.resize(limit);
        }

        return rankings;
    }

    struct DriverComparison
    {
        DriverBehaviorMetrics driver1;
        DriverBehaviorMetrics driver2;

        std::string better_safety_score;
        std::string better_fuel_efficiency;
        std::string safer_driver;

        double score_difference;
        double distance_difference;
    };

    DriverComparison compare_drivers(uint64_t driver1_id, uint64_t driver2_id)
    {
        DriverComparison comparison;

        comparison.driver1 = get_driver_behavior(driver1_id);
        comparison.driver2 = get_driver_behavior(driver2_id);

        // Compare safety scores
        if (comparison.driver1.safety_score > comparison.driver2.safety_score)
        {
            comparison.better_safety_score = "Driver 1";
            comparison.safer_driver = "Driver 1";
        }
        else
        {
            comparison.better_safety_score = "Driver 2";
            comparison.safer_driver = "Driver 2";
        }

        comparison.score_difference = abs((int)comparison.driver1.safety_score -
                                          (int)comparison.driver2.safety_score);
        comparison.distance_difference = abs(comparison.driver1.total_distance -
                                             comparison.driver2.total_distance);

        // Compare fuel efficiency
        if (comparison.driver1.avg_fuel_efficiency > comparison.driver2.avg_fuel_efficiency)
        {
            comparison.better_fuel_efficiency = "Driver 1";
        }
        else
        {
            comparison.better_fuel_efficiency = "Driver 2";
        }

        return comparison;
    }

    // ========================================================================
    // LICENSE & DOCUMENT ALERTS
    // ========================================================================

    struct DocumentAlert
    {
        uint64_t driver_id;
        std::string driver_name;
        std::string alert_type;
        uint64_t expiry_date;
        uint32_t days_until_expiry;
        uint8_t severity; // 1=Info, 2=Warning, 3=Critical
    };

    std::vector<DocumentAlert> get_license_expiry_alerts(int days_threshold = 30)
    {
        std::vector<DocumentAlert> alerts;

        auto all_drivers = db_.get_all_drivers();
        uint64_t current_time = get_current_timestamp();

        for (const auto &driver : all_drivers)
        {
            if (driver.license_expiry > 0)
            {
                int64_t days_until = (driver.license_expiry - current_time) / 86400ULL;

                if (days_until <= days_threshold)
                {
                    DocumentAlert alert;
                    alert.driver_id = driver.driver_id;
                    alert.driver_name = driver.full_name;
                    alert.alert_type = "License Expiry";
                    alert.expiry_date = driver.license_expiry;
                    alert.days_until_expiry = days_until;

                    if (days_until <= 0)
                    {
                        alert.severity = 3; // Critical - expired
                    }
                    else if (days_until <= 7)
                    {
                        alert.severity = 3; // Critical - < 1 week
                    }
                    else if (days_until <= 14)
                    {
                        alert.severity = 2; // Warning - < 2 weeks
                    }
                    else
                    {
                        alert.severity = 1; // Info
                    }

                    alerts.push_back(alert);
                }
            }
        }

        return alerts;
    }

    // ========================================================================
    // DRIVER RECOMMENDATIONS
    // ========================================================================

    struct DriverRecommendation
    {
        std::string category;
        std::string recommendation;
        uint8_t priority; // 1=Low, 2=Medium, 3=High
        double potential_improvement;
    };

    std::vector<DriverRecommendation> get_improvement_recommendations(uint64_t driver_id)
    {
        std::vector<DriverRecommendation> recommendations;

        auto metrics = get_driver_behavior(driver_id);

        // Harsh braking recommendation
        if (metrics.harsh_braking_rate > 2.0)
        { // >2 per 100km
            DriverRecommendation rec;
            rec.category = "Braking Behavior";
            rec.recommendation = "Reduce harsh braking by anticipating stops earlier. "
                                 "This improves safety and fuel efficiency.";
            rec.priority = 3;
            rec.potential_improvement = 15.0; // 15% score improvement
            recommendations.push_back(rec);
        }

        // Speeding recommendation
        if (metrics.speeding_rate > 1.0)
        {
            DriverRecommendation rec;
            rec.category = "Speed Management";
            rec.recommendation = "Maintain speed limits to improve safety score and "
                                 "reduce fuel consumption.";
            rec.priority = 3;
            rec.potential_improvement = 20.0;
            recommendations.push_back(rec);
        }

        // Fuel efficiency recommendation
        if (metrics.avg_fuel_efficiency < 10.0)
        { // < 10 km/L
            DriverRecommendation rec;
            rec.category = "Fuel Efficiency";
            rec.recommendation = "Improve fuel efficiency by maintaining steady speeds "
                                 "and avoiding aggressive acceleration.";
            rec.priority = 2;
            rec.potential_improvement = 10.0;
            recommendations.push_back(rec);
        }

        // Vision-based recommendations
        if (metrics.distraction_events > 5)
        {
            DriverRecommendation rec;
            rec.category = "Driver Attention";
            rec.recommendation = "Minimize distractions while driving. Keep eyes on "
                                 "the road and avoid phone usage.";
            rec.priority = 3;
            rec.potential_improvement = 25.0;
            recommendations.push_back(rec);
        }

        return recommendations;
    }

    // ========================================================================
    // NEW METHODS FOR REQUEST HANDLER COMPATIBILITY
    // ========================================================================

    bool report_driver_event(uint64_t driver_id, 
                            const std::string& event_type,
                            const std::string& description,
                            int point_deduction = 0,
                            uint64_t trip_id = 0)
    {
        DriverProfile driver;
        if (!db_.read_driver(driver_id, driver))
        {
            return false;
        }

        // Apply point deduction if specified
        if (point_deduction > 0)
        {
            if (driver.safety_score > point_deduction)
            {
                driver.safety_score -= point_deduction;
            }
            else
            {
                driver.safety_score = 0;
            }
        }

        // Update driver in database
        if (!db_.update_driver(driver))
        {
            return false;
        }

        // Invalidate cache
        cache_.invalidate_driver(driver_id);

        // Log the event (could be saved to a separate event log)
        std::cout << "Driver event reported: " << event_type 
                  << " for driver " << driver_id 
                  << " - " << description << std::endl;
        
        if (point_deduction > 0)
        {
            std::cout << "Safety score reduced by " << point_deduction 
                      << " points. New score: " << driver.safety_score << std::endl;
        }

        return true;
    }

    // NEW: Get driver leaderboard with parameters as used in RequestHandler
    std::vector<DriverRanking> get_driver_leaderboard_with_params(int limit = 10,
                                                                  const std::string& sort_by = "score",
                                                                  const std::string& time_period = "all")
    {
        return get_driver_leaderboard(limit, sort_by, time_period);
    }

    // ========================================================================
    // HELPER FUNCTIONS
    // ========================================================================

private:
    uint64_t get_current_timestamp()
    {
        return static_cast<uint64_t>(time(nullptr));
    }
    
    void calculate_driver_ranking(uint64_t driver_id, DriverBehaviorMetrics &metrics)
    {
        auto leaderboard = get_driver_leaderboard(10000);

        for (const auto &rank : leaderboard)
        {
            if (rank.driver_id == driver_id)
            {
                metrics.rank_in_fleet = rank.rank;
                metrics.percentile = rank.percentile;
                break;
            }
        }
    }
};
#endif // DRIVERMANAGER_H