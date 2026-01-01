#ifndef SDM_TYPES_HPP
#define SDM_TYPES_HPP

#include <cstdint>
#include <cstring>
#include <string>
#include <iostream>
using namespace std;

#pragma pack(push, 1)

enum class UserRole : uint8_t
{
    DRIVER = 0,
    ADMIN = 1,
    FLEET_MANAGER = 2
};

enum class VehicleType : uint8_t
{
    SEDAN = 0,
    SUV = 1,
    TRUCK = 2,
    VAN = 3,
    MOTORCYCLE = 4
};

enum class MaintenanceType : uint8_t
{
    OIL_CHANGE = 0,
    TIRE_ROTATION = 1,
    BRAKE_SERVICE = 2,
    ENGINE_CHECK = 3,
    TRANSMISSION = 4,
    GENERAL_SERVICE = 5
};

enum class ExpenseCategory : uint8_t
{
    FUEL = 0,
    MAINTENANCE = 1,
    INSURANCE = 2,
    TOLL = 3,
    PARKING = 4,
    OTHER = 5
};

enum class IncidentType : uint8_t
{
    ACCIDENT = 0,
    BREAKDOWN = 1,
    THEFT = 2,
    VANDALISM = 3,
    TRAFFIC_VIOLATION = 4
};

enum class DrivingEventType : uint8_t
{
    HARSH_BRAKING = 0,
    RAPID_ACCELERATION = 1,
    SPEEDING = 2,
    SHARP_TURN = 3,
    IDLE_EXCESSIVE = 4
};

struct SDMHeader
{
    char magic[8];
    uint32_t version;
    uint64_t total_size;
    uint64_t created_time;
    uint64_t last_modified;
    char creator_info[64];

    uint64_t driver_table_offset;
    uint64_t vehicle_table_offset;
    uint64_t trip_table_offset;
    uint64_t maintenance_table_offset;
    uint64_t expense_table_offset;
    uint64_t document_table_offset;
    uint64_t incident_table_offset;

    uint64_t primary_index_offset;
    uint64_t secondary_index_offset;

    uint32_t max_drivers;
    uint32_t max_vehicles;
    uint32_t max_trips;

    uint8_t reserved[3912];

    SDMHeader() : version(0x00010000), total_size(0), created_time(0),
                  last_modified(0), driver_table_offset(0), vehicle_table_offset(0),
                  trip_table_offset(0), maintenance_table_offset(0),
                  expense_table_offset(0), document_table_offset(0),
                  incident_table_offset(0), primary_index_offset(0),
                  secondary_index_offset(0), max_drivers(10000),
                  max_vehicles(50000), max_trips(10000000)
    {
        strncpy(magic, "SDMDB001", 8);
        memset(creator_info, 0, sizeof(creator_info));
        memset(reserved, 0, sizeof(reserved));
    }
};

static_assert(sizeof(SDMHeader) == 4096, "SDMHeader must be 4096 bytes");

struct DriverProfile
{
    uint64_t driver_id;
    char username[64];
    char password_hash[65];
    UserRole role;
    

    char full_name[128];
    char email[128];
    char phone[32];
    char license_number[32];
    uint64_t license_expiry;
    

    uint64_t total_trips;
    double total_distance;
    double total_fuel_consumed;
    uint32_t safety_score;
    uint32_t harsh_events_count;
    

    uint64_t created_time;
    uint64_t last_login;
    uint8_t is_active;
    

    uint64_t trip_history_head;
    uint64_t trip_history_tail;
    
    uint8_t reserved[493];

    DriverProfile() : driver_id(0), role(UserRole::DRIVER), license_expiry(0),
                      total_trips(0), total_distance(0), total_fuel_consumed(0),
                      safety_score(1000), harsh_events_count(0), created_time(0),
                      last_login(0), is_active(1), trip_history_head(0),
                      trip_history_tail(0)
    {
        memset(username, 0, sizeof(username));
        memset(password_hash, 0, sizeof(password_hash));
        memset(full_name, 0, sizeof(full_name));
        memset(email, 0, sizeof(email));
        memset(phone, 0, sizeof(phone));
        memset(license_number, 0, sizeof(license_number));
        memset(reserved, 0, sizeof(reserved));
    }
};

static_assert(sizeof(DriverProfile) == 1024, "DriverProfile must be 1024 bytes");

struct VehicleInfo
{
    uint64_t vehicle_id;
    uint64_t owner_driver_id;
    
    char license_plate[32];
    char make[64];
    char model[64];
    uint32_t year;
    VehicleType type;
    char color[32];
    char vin[32];
    

    uint32_t engine_capacity;
    double fuel_tank_capacity;
    char fuel_type[16];
    

    double current_odometer;
    double last_service_odometer;
    

    char insurance_provider[64];
    char insurance_policy[64];
    uint64_t insurance_expiry;
    

    uint64_t registration_expiry;
    

    uint64_t last_maintenance_date;
    uint64_t next_maintenance_due;
    
    uint64_t created_time;
    uint8_t is_active;
    

    uint8_t reserved[566];

    VehicleInfo() : vehicle_id(0), owner_driver_id(0), year(0),
                    type(VehicleType::SEDAN), engine_capacity(0),
                    fuel_tank_capacity(0), current_odometer(0),
                    last_service_odometer(0), insurance_expiry(0),
                    registration_expiry(0), last_maintenance_date(0),
                    next_maintenance_due(0), created_time(0), is_active(1)
    {
        memset(license_plate, 0, sizeof(license_plate));
        memset(make, 0, sizeof(make));
        memset(model, 0, sizeof(model));
        memset(color, 0, sizeof(color));
        memset(vin, 0, sizeof(vin));
        memset(fuel_type, 0, sizeof(fuel_type));
        memset(insurance_provider, 0, sizeof(insurance_provider));
        memset(insurance_policy, 0, sizeof(insurance_policy));
        memset(reserved, 0, sizeof(reserved));
    }
};

static_assert(sizeof(VehicleInfo) == 1024, "VehicleInfo must be 1024 bytes");

struct TripRecord
{
    uint64_t trip_id;
    uint64_t driver_id;
    uint64_t vehicle_id;
    

    uint64_t start_time;
    uint64_t end_time;
    uint32_t duration;
    

    double start_latitude;
    double start_longitude;
    double end_latitude;
    double end_longitude;
    char start_address[128];
    char end_address[128];
    

    double distance;
    double avg_speed;
    double max_speed;
    double fuel_consumed;
    double fuel_efficiency;
    

    uint16_t harsh_braking_count;
    uint16_t rapid_acceleration_count;
    uint16_t speeding_count;
    uint16_t sharp_turn_count;
    

    uint64_t gps_data_offset;
    uint32_t gps_data_count;
    

    char notes[256];
    

    uint8_t reserved[376];

    TripRecord() : trip_id(0), driver_id(0), vehicle_id(0), start_time(0),
                   end_time(0), duration(0), start_latitude(0), start_longitude(0),
                   end_latitude(0), end_longitude(0), distance(0), avg_speed(0),
                   max_speed(0), fuel_consumed(0), fuel_efficiency(0),
                   harsh_braking_count(0), rapid_acceleration_count(0),
                   speeding_count(0), sharp_turn_count(0), gps_data_offset(0),
                   gps_data_count(0)
    {
        memset(start_address, 0, sizeof(start_address));
        memset(end_address, 0, sizeof(end_address));
        memset(notes, 0, sizeof(notes));
        memset(reserved, 0, sizeof(reserved));
    }
};

static_assert(sizeof(TripRecord) == 1024, "TripRecord must be 1024 bytes");

struct GPSWaypoint
{
    uint64_t timestamp;
    double latitude;
    double longitude;
    float speed;
    float altitude;
    float accuracy;
    uint8_t satellites;
    uint8_t reserved[3];

    GPSWaypoint() : timestamp(0), latitude(0), longitude(0), speed(0),
                    altitude(0), accuracy(0), satellites(0)
    {
        memset(reserved, 0, sizeof(reserved));
    }
};

struct MaintenanceRecord
{
    uint64_t maintenance_id;
    uint64_t vehicle_id;
    uint64_t driver_id;
    
    MaintenanceType type;
    uint64_t service_date;
    double odometer_reading;
    
    char service_center[128];
    char technician[64];
    char description[192];
    

    double labor_cost;
    double parts_cost;
    double total_cost;
    char currency[8];
    

    char parts_replaced[192];
    

    uint64_t next_service_date;
    double next_service_odometer;
    

    uint64_t receipt_doc_id;
    
    char notes[191];
    

    uint8_t reserved[160];

    MaintenanceRecord() : maintenance_id(0), vehicle_id(0), driver_id(0),
                          type(MaintenanceType::GENERAL_SERVICE), service_date(0),
                          odometer_reading(0), labor_cost(0), parts_cost(0),
                          total_cost(0), next_service_date(0),
                          next_service_odometer(0), receipt_doc_id(0)
    {
        memset(service_center, 0, sizeof(service_center));
        memset(technician, 0, sizeof(technician));
        memset(description, 0, sizeof(description));
        memset(currency, 0, sizeof(currency));
        memset(parts_replaced, 0, sizeof(parts_replaced));
        memset(notes, 0, sizeof(notes));
        memset(reserved, 0, sizeof(reserved));
    }
};

static_assert(sizeof(MaintenanceRecord) == 1024, "MaintenanceRecord must be 1024 bytes");

struct ExpenseRecord
{
    uint64_t expense_id;
    uint64_t driver_id;
    uint64_t vehicle_id;
    uint64_t trip_id;
    
    ExpenseCategory category;
    uint64_t expense_date;
    
    double amount;
    char currency[8];
    char description[256];
    

    double fuel_quantity;
    double fuel_price_per_unit;
    char fuel_station[128];
    

    char payment_method[32];
    char receipt_number[64];
    

    uint8_t is_tax_deductible;
    double tax_amount;
    

    uint64_t receipt_doc_id;
    
    char notes[256];
    

    uint8_t reserved[198];

    ExpenseRecord() : expense_id(0), driver_id(0), vehicle_id(0), trip_id(0),
                      category(ExpenseCategory::OTHER), expense_date(0),
                      amount(0), fuel_quantity(0), fuel_price_per_unit(0),
                      is_tax_deductible(0), tax_amount(0), receipt_doc_id(0)
    {
        memset(currency, 0, sizeof(currency));
        memset(description, 0, sizeof(description));
        memset(fuel_station, 0, sizeof(fuel_station));
        memset(payment_method, 0, sizeof(payment_method));
        memset(receipt_number, 0, sizeof(receipt_number));
        memset(notes, 0, sizeof(notes));
        memset(reserved, 0, sizeof(reserved));
    }
};

static_assert(sizeof(ExpenseRecord) == 1024, "ExpenseRecord must be 1024 bytes");

struct DocumentMetadata
{
    uint64_t document_id;
    uint64_t owner_id;
    uint8_t owner_type;
    
    char filename[256];
    char mime_type[64];
    uint64_t file_size;
    uint64_t upload_date;
    uint64_t expiry_date;
    

    uint64_t data_offset;
    uint32_t data_blocks;
    
    char description[256];
    char tags[128];
    
    uint8_t reserved[267];

    DocumentMetadata() : document_id(0), owner_id(0), owner_type(0),
                         file_size(0), upload_date(0), expiry_date(0),
                         data_offset(0), data_blocks(0)
    {
        memset(filename, 0, sizeof(filename));
        memset(mime_type, 0, sizeof(mime_type));
        memset(description, 0, sizeof(description));
        memset(tags, 0, sizeof(tags));
        memset(reserved, 0, sizeof(reserved));
    }
};

static_assert(sizeof(DocumentMetadata) == 1024, "DocumentMetadata must be 1024 bytes");

struct IncidentReport
{
    uint64_t incident_id;
    uint64_t driver_id;
    uint64_t vehicle_id;
    uint64_t trip_id;
    
    IncidentType type;
    uint64_t incident_time;
    

    double latitude;
    double longitude;
    char location_address[256];
    

    char description[512];
    char police_report_number[64];
    char insurance_claim_number[64];
    

    char other_party_info[256];
    char witness_info[256];
    

    double estimated_damage;
    double insurance_payout;
    char currency[8];
    

    uint64_t photo_doc_ids[5];
    uint64_t report_doc_id;
    

    uint8_t is_resolved;
    uint64_t resolved_date;
    
    char notes[256];
    

    uint8_t reserved[246];

    IncidentReport() : incident_id(0), driver_id(0), vehicle_id(0), trip_id(0),
                       type(IncidentType::ACCIDENT), incident_time(0),
                       latitude(0), longitude(0), estimated_damage(0),
                       insurance_payout(0), report_doc_id(0), is_resolved(0),
                       resolved_date(0)
    {
        memset(location_address, 0, sizeof(location_address));
        memset(description, 0, sizeof(description));
        memset(police_report_number, 0, sizeof(police_report_number));
        memset(insurance_claim_number, 0, sizeof(insurance_claim_number));
        memset(other_party_info, 0, sizeof(other_party_info));
        memset(witness_info, 0, sizeof(witness_info));
        memset(currency, 0, sizeof(currency));
        memset(photo_doc_ids, 0, sizeof(photo_doc_ids));
        memset(notes, 0, sizeof(notes));
        memset(reserved, 0, sizeof(reserved));
    }
};

static_assert(sizeof(IncidentReport) == 2048, "IncidentReport must be 2048 bytes");

struct SessionInfo
{
    char session_id[64];
    uint64_t driver_id;
    uint64_t login_time;
    uint64_t last_activity;
    uint32_t operations_count;
    char ip_address[64];
    uint8_t reserved[36];

    SessionInfo() : driver_id(0), login_time(0), last_activity(0),
                    operations_count(0)
    {
        memset(session_id, 0, sizeof(session_id));
        memset(ip_address, 0, sizeof(ip_address));
        memset(reserved, 0, sizeof(reserved));
    }
};

struct DatabaseStats
{
    uint64_t total_drivers;
    uint64_t active_drivers;
    uint64_t total_vehicles;
    uint64_t total_trips;
    uint64_t total_distance;
    uint64_t total_expenses;
    uint64_t total_maintenance_records;
    uint64_t total_documents;
    uint64_t total_incidents;
    uint64_t database_size;
    uint64_t used_space;
    double fragmentation;
    uint32_t active_sessions;
    

    uint8_t reserved[28];

    DatabaseStats() : total_drivers(0), active_drivers(0), total_vehicles(0),
                      total_trips(0), total_distance(0), total_expenses(0),
                      total_maintenance_records(0), total_documents(0),
                      total_incidents(0), database_size(0), used_space(0),
                      fragmentation(0), active_sessions(0)
    {
        memset(reserved, 0, sizeof(reserved));
    }
};

enum class DetectionType : uint8_t
{
    VEHICLE = 0,
    PEDESTRIAN = 1,
    CYCLIST = 2,
    TRAFFIC_SIGN = 3,
    TRAFFIC_LIGHT = 4,
    LANE_MARKING = 5,
    OBSTACLE = 6,
    ANIMAL = 7
};

enum class DriverState : uint8_t
{
    NORMAL = 0,
    DROWSY = 1,
    DISTRACTED = 2,
    USING_PHONE = 3,
    NOT_LOOKING_AHEAD = 4,
    EYES_CLOSED = 5
};

struct ObjectDetection
{
    uint64_t detection_id;
    uint64_t trip_id;
    uint64_t timestamp;
    
    DetectionType type;
    float confidence;
    

    float bbox_x;
    float bbox_y;
    float bbox_width;
    float bbox_height;
    

    float estimated_distance;
    float relative_speed;
    

    double latitude;
    double longitude;
    

    uint8_t camera_id;
    

    uint8_t alert_triggered;
    char alert_message[128];
    

    uint8_t reserved[57];

    ObjectDetection() : detection_id(0), trip_id(0), timestamp(0),
                        type(DetectionType::VEHICLE), confidence(0),
                        bbox_x(0), bbox_y(0), bbox_width(0), bbox_height(0),
                        estimated_distance(0), relative_speed(0),
                        latitude(0), longitude(0), camera_id(0),
                        alert_triggered(0)
    {
        memset(alert_message, 0, sizeof(alert_message));
        memset(reserved, 0, sizeof(reserved));
    }
};

static_assert(sizeof(ObjectDetection) == 256, "ObjectDetection must be 256 bytes");

struct DriverBehaviorDetection
{
    uint64_t detection_id;
    uint64_t trip_id;
    uint64_t driver_id;
    uint64_t timestamp;
    
    DriverState state;
    float confidence;
    uint32_t duration;
    

    uint8_t face_detected;
    float head_pitch;
    float head_yaw;
    float head_roll;
    

    uint8_t eyes_detected;
    float eye_closure_ratio;
    uint8_t blink_count;
    

    float attention_score;
    uint8_t looking_at_road;
    

    uint8_t alert_triggered;
    char alert_type[64];
    

    char frame_filename[128];
    

    uint8_t reserved[254];

    DriverBehaviorDetection() : detection_id(0), trip_id(0), driver_id(0),
                                timestamp(0), state(DriverState::NORMAL),
                                confidence(0), duration(0), face_detected(0),
                                head_pitch(0), head_yaw(0), head_roll(0),
                                eyes_detected(0), eye_closure_ratio(0),
                                blink_count(0), attention_score(1.0),
                                looking_at_road(1), alert_triggered(0)
    {
        memset(alert_type, 0, sizeof(alert_type));
        memset(frame_filename, 0, sizeof(frame_filename));
        memset(reserved, 0, sizeof(reserved));
    }
};

static_assert(sizeof(DriverBehaviorDetection) == 512, "DriverBehaviorDetection must be 512 bytes");

struct VisionAnalytics
{
    uint64_t trip_id;
    

    uint32_t total_vehicles_detected;
    uint32_t total_pedestrians_detected;
    uint32_t total_cyclists_detected;
    uint32_t total_traffic_signs_detected;
    uint32_t total_obstacles_detected;
    

    uint32_t forward_collision_warnings;
    uint32_t lane_departure_warnings;
    uint32_t blind_spot_warnings;
    

    uint32_t drowsiness_events;
    uint32_t distraction_events;
    uint32_t phone_usage_events;
    uint32_t total_attention_lapses;
    

    float vision_safety_score;
    

    uint8_t reserved[260];

    VisionAnalytics() : trip_id(0), total_vehicles_detected(0),
                        total_pedestrians_detected(0), total_cyclists_detected(0),
                        total_traffic_signs_detected(0), total_obstacles_detected(0),
                        forward_collision_warnings(0), lane_departure_warnings(0),
                        blind_spot_warnings(0), drowsiness_events(0),
                        distraction_events(0), phone_usage_events(0),
                        total_attention_lapses(0), vision_safety_score(100.0)
    {
        memset(reserved, 0, sizeof(reserved));
    }
};

static_assert(sizeof(VisionAnalytics) == 320, "VisionAnalytics must be 320 bytes");

#pragma pack(pop)  

#endif