// File Location: include/sdm_types.hpp
// Smart Drive Manager - Core Type Definitions

#ifndef SDM_TYPES_HPP
#define SDM_TYPES_HPP

#include <cstdint>
#include <cstring>
#include <string>

// Use packed structs to avoid padding issues
#pragma pack(push, 1)

// ============================================================================
// ENUMERATIONS
// ============================================================================

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

// ============================================================================
// DATABASE HEADER
// ============================================================================

struct SDMHeader
{
    char magic[8];          // 8 bytes
    uint32_t version;       // 4 bytes (total: 12)
    uint64_t total_size;    // 8 bytes (total: 20)
    uint64_t created_time;  // 8 bytes (total: 28)
    uint64_t last_modified; // 8 bytes (total: 36)
    char creator_info[64];  // 64 bytes (total: 100)

    // Offsets to main sections
    uint64_t driver_table_offset;       // 8 (108)
    uint64_t vehicle_table_offset;      // 8 (116)
    uint64_t trip_table_offset;         // 8 (124)
    uint64_t maintenance_table_offset;  // 8 (132)
    uint64_t expense_table_offset;      // 8 (140)
    uint64_t document_table_offset;     // 8 (148)
    uint64_t incident_table_offset;     // 8 (156)

    // Index file offsets
    uint64_t primary_index_offset;      // 8 (164)
    uint64_t secondary_index_offset;    // 8 (172)

    // Counts
    uint32_t max_drivers;               // 4 (176)
    uint32_t max_vehicles;              // 4 (180)
    uint32_t max_trips;                 // 4 (184)

    // Padding to make exactly 4096 bytes
    // Total so far: 184 bytes, need 4096 - 184 = 3912 bytes padding
    uint8_t reserved[3912];

    SDMHeader() : version(0x00010000), total_size(0), created_time(0),
                  last_modified(0), driver_table_offset(0), vehicle_table_offset(0),
                  trip_table_offset(0), maintenance_table_offset(0),
                  expense_table_offset(0), document_table_offset(0),
                  incident_table_offset(0), primary_index_offset(0),
                  secondary_index_offset(0), max_drivers(10000),
                  max_vehicles(50000), max_trips(10000000)
    {
        std::strncpy(magic, "SDMDB001", 8);
        std::memset(creator_info, 0, sizeof(creator_info));
        std::memset(reserved, 0, sizeof(reserved));
    }
};

static_assert(sizeof(SDMHeader) == 4096, "SDMHeader must be 4096 bytes");

// ============================================================================
// USER/DRIVER STRUCTURES
// ============================================================================

struct DriverProfile
{
    uint64_t driver_id;          // 8 bytes
    char username[64];           // 64 (72)
    char password_hash[65];    // 64 (136)
    UserRole role;               // 1 (137)
    
    // Personal info
    char full_name[128];         // 128 (265)
    char email[128];             // 128 (393)
    char phone[32];              // 32 (425)
    char license_number[32];     // 32 (457)
    uint64_t license_expiry;     // 8 (465)
    
    // Statistics
    uint64_t total_trips;        // 8 (473)
    double total_distance;       // 8 (481)
    double total_fuel_consumed;  // 8 (489)
    uint32_t safety_score;       // 4 (493)
    uint32_t harsh_events_count; // 4 (497)
    
    // Timestamps
    uint64_t created_time;       // 8 (505)
    uint64_t last_login;         // 8 (513)
    uint8_t is_active;           // 1 (514)
    
    // Linked list pointers
    uint64_t trip_history_head;  // 8 (522)
    uint64_t trip_history_tail;  // 8 (530)
    
    // Padding to make exactly 1024 bytes
    // Total so far: 530 bytes, need 1024 - 530 = 494 bytes padding
    uint8_t reserved[493];

    DriverProfile() : driver_id(0), role(UserRole::DRIVER), license_expiry(0),
                      total_trips(0), total_distance(0), total_fuel_consumed(0),
                      safety_score(1000), harsh_events_count(0), created_time(0),
                      last_login(0), is_active(1), trip_history_head(0),
                      trip_history_tail(0)
    {
        std::memset(username, 0, sizeof(username));
        std::memset(password_hash, 0, sizeof(password_hash));
        std::memset(full_name, 0, sizeof(full_name));
        std::memset(email, 0, sizeof(email));
        std::memset(phone, 0, sizeof(phone));
        std::memset(license_number, 0, sizeof(license_number));
        std::memset(reserved, 0, sizeof(reserved));
    }
};

static_assert(sizeof(DriverProfile) == 1024, "DriverProfile must be 1024 bytes");

// ============================================================================
// VEHICLE STRUCTURES
// ============================================================================

struct VehicleInfo
{
    uint64_t vehicle_id;         // 8 bytes
    uint64_t owner_driver_id;    // 8 (16)
    
    char license_plate[32];      // 32 (48)
    char make[64];               // 64 (112)
    char model[64];              // 64 (176)
    uint32_t year;               // 4 (180)
    VehicleType type;            // 1 (181)
    char color[32];              // 32 (213)
    char vin[32];                // 32 (245)
    
    // Technical specs
    uint32_t engine_capacity;    // 4 (249)
    double fuel_tank_capacity;   // 8 (257)
    char fuel_type[16];          // 16 (273)
    
    // Odometer
    double current_odometer;     // 8 (281)
    double last_service_odometer;// 8 (289)
    
    // Insurance
    char insurance_provider[64]; // 64 (353)
    char insurance_policy[64];   // 64 (417)
    uint64_t insurance_expiry;   // 8 (425)
    
    // Registration
    uint64_t registration_expiry;// 8 (433)
    
    // Maintenance tracking
    uint64_t last_maintenance_date;  // 8 (441)
    uint64_t next_maintenance_due;   // 8 (449)
    
    uint64_t created_time;       // 8 (457)
    uint8_t is_active;           // 1 (458)
    
    // Padding to make exactly 1024 bytes
    // Total so far: 458 bytes, need 1024 - 458 = 566 bytes padding
    uint8_t reserved[566];

    VehicleInfo() : vehicle_id(0), owner_driver_id(0), year(0),
                    type(VehicleType::SEDAN), engine_capacity(0),
                    fuel_tank_capacity(0), current_odometer(0),
                    last_service_odometer(0), insurance_expiry(0),
                    registration_expiry(0), last_maintenance_date(0),
                    next_maintenance_due(0), created_time(0), is_active(1)
    {
        std::memset(license_plate, 0, sizeof(license_plate));
        std::memset(make, 0, sizeof(make));
        std::memset(model, 0, sizeof(model));
        std::memset(color, 0, sizeof(color));
        std::memset(vin, 0, sizeof(vin));
        std::memset(fuel_type, 0, sizeof(fuel_type));
        std::memset(insurance_provider, 0, sizeof(insurance_provider));
        std::memset(insurance_policy, 0, sizeof(insurance_policy));
        std::memset(reserved, 0, sizeof(reserved));
    }
};

static_assert(sizeof(VehicleInfo) == 1024, "VehicleInfo must be 1024 bytes");

// ============================================================================
// TRIP STRUCTURES
// ============================================================================

struct TripRecord
{
    uint64_t trip_id;            // 8 bytes
    uint64_t driver_id;          // 8 (16)
    uint64_t vehicle_id;         // 8 (24)
    
    // Time
    uint64_t start_time;         // 8 (32)
    uint64_t end_time;           // 8 (40)
    uint32_t duration;           // 4 (44)
    
    // Location
    double start_latitude;       // 8 (52)
    double start_longitude;      // 8 (60)
    double end_latitude;         // 8 (68)
    double end_longitude;        // 8 (76)
    char start_address[128];     // 128 (204)
    char end_address[128];       // 128 (332)
    
    // Metrics
    double distance;             // 8 (340)
    double avg_speed;            // 8 (348)
    double max_speed;            // 8 (356)
    double fuel_consumed;        // 8 (364)
    double fuel_efficiency;      // 8 (372)
    
    // Events
    uint16_t harsh_braking_count;        // 2 (374)
    uint16_t rapid_acceleration_count;   // 2 (376)
    uint16_t speeding_count;             // 2 (378)
    uint16_t sharp_turn_count;           // 2 (380)
    
    // GPS data
    uint64_t gps_data_offset;    // 8 (388)
    uint32_t gps_data_count;     // 4 (392)
    
    // Notes
    char notes[256];             // 256 (648)
    
    // Padding to make exactly 1024 bytes
    // Total so far: 648 bytes, need 1024 - 648 = 376 bytes padding
    uint8_t reserved[376];

    TripRecord() : trip_id(0), driver_id(0), vehicle_id(0), start_time(0),
                   end_time(0), duration(0), start_latitude(0), start_longitude(0),
                   end_latitude(0), end_longitude(0), distance(0), avg_speed(0),
                   max_speed(0), fuel_consumed(0), fuel_efficiency(0),
                   harsh_braking_count(0), rapid_acceleration_count(0),
                   speeding_count(0), sharp_turn_count(0), gps_data_offset(0),
                   gps_data_count(0)
    {
        std::memset(start_address, 0, sizeof(start_address));
        std::memset(end_address, 0, sizeof(end_address));
        std::memset(notes, 0, sizeof(notes));
        std::memset(reserved, 0, sizeof(reserved));
    }
};

static_assert(sizeof(TripRecord) == 1024, "TripRecord must be 1024 bytes");

struct GPSWaypoint
{
    uint64_t timestamp;
    double latitude;
    double longitude;
    float speed;    // km/h
    float altitude; // meters
    float accuracy; // meters
    uint8_t satellites;
    uint8_t reserved[3];

    GPSWaypoint() : timestamp(0), latitude(0), longitude(0), speed(0),
                    altitude(0), accuracy(0), satellites(0)
    {
        std::memset(reserved, 0, sizeof(reserved));
    }
};

// ============================================================================
// MAINTENANCE STRUCTURES
// ============================================================================

struct MaintenanceRecord
{
    uint64_t maintenance_id;     // 8 bytes
    uint64_t vehicle_id;         // 8 (16)
    uint64_t driver_id;          // 8 (24)
    
    MaintenanceType type;        // 1 (25)
    uint64_t service_date;       // 8 (33)
    double odometer_reading;     // 8 (41)
    
    char service_center[128];    // 128 (169)
    char technician[64];         // 64 (233)
    char description[192];       // 192 (425)
    
    // Cost
    double labor_cost;           // 8 (433)
    double parts_cost;           // 8 (441)
    double total_cost;           // 8 (449)
    char currency[8];            // 8 (457)
    
    // Parts replaced
    char parts_replaced[192];    // 192 (649)
    
    // Next service
    uint64_t next_service_date;  // 8 (657)
    double next_service_odometer;// 8 (665)
    
    // Documents
    uint64_t receipt_doc_id;     // 8 (673)
    
    char notes[191];             // 191 (864)
    
    // Padding to make exactly 1024 bytes
    // Total: 864 bytes, need 1024 - 864 = 160 bytes padding
    uint8_t reserved[160];

    MaintenanceRecord() : maintenance_id(0), vehicle_id(0), driver_id(0),
                          type(MaintenanceType::GENERAL_SERVICE), service_date(0),
                          odometer_reading(0), labor_cost(0), parts_cost(0),
                          total_cost(0), next_service_date(0),
                          next_service_odometer(0), receipt_doc_id(0)
    {
        std::memset(service_center, 0, sizeof(service_center));
        std::memset(technician, 0, sizeof(technician));
        std::memset(description, 0, sizeof(description));
        std::memset(currency, 0, sizeof(currency));
        std::memset(parts_replaced, 0, sizeof(parts_replaced));
        std::memset(notes, 0, sizeof(notes));
        std::memset(reserved, 0, sizeof(reserved));
    }
};

static_assert(sizeof(MaintenanceRecord) == 1024, "MaintenanceRecord must be 1024 bytes");

// ============================================================================
// EXPENSE STRUCTURES
// ============================================================================

struct ExpenseRecord
{
    uint64_t expense_id;         // 8 bytes
    uint64_t driver_id;          // 8 (16)
    uint64_t vehicle_id;         // 8 (24)
    uint64_t trip_id;            // 8 (32)
    
    ExpenseCategory category;    // 1 (33)
    uint64_t expense_date;       // 8 (41)
    
    double amount;               // 8 (49)
    char currency[8];            // 8 (57)
    char description[256];       // 256 (313)
    
    // Fuel-specific
    double fuel_quantity;        // 8 (321)
    double fuel_price_per_unit;  // 8 (329)
    char fuel_station[128];      // 128 (457)
    
    // Payment
    char payment_method[32];     // 32 (489)
    char receipt_number[64];     // 64 (553)
    
    // Tax
    uint8_t is_tax_deductible;   // 1 (554)
    double tax_amount;           // 8 (562)
    
    // Document
    uint64_t receipt_doc_id;     // 8 (570)
    
    char notes[256];             // 256 (826)
    
    // Padding to make exactly 1024 bytes
    // Total: 826 bytes, need 1024 - 826 = 198 bytes padding
    uint8_t reserved[198];

    ExpenseRecord() : expense_id(0), driver_id(0), vehicle_id(0), trip_id(0),
                      category(ExpenseCategory::OTHER), expense_date(0),
                      amount(0), fuel_quantity(0), fuel_price_per_unit(0),
                      is_tax_deductible(0), tax_amount(0), receipt_doc_id(0)
    {
        std::memset(currency, 0, sizeof(currency));
        std::memset(description, 0, sizeof(description));
        std::memset(fuel_station, 0, sizeof(fuel_station));
        std::memset(payment_method, 0, sizeof(payment_method));
        std::memset(receipt_number, 0, sizeof(receipt_number));
        std::memset(notes, 0, sizeof(notes));
        std::memset(reserved, 0, sizeof(reserved));
    }
};

static_assert(sizeof(ExpenseRecord) == 1024, "ExpenseRecord must be 1024 bytes");

// ============================================================================
// DOCUMENT STRUCTURES
// ============================================================================

struct DocumentMetadata
{
    uint64_t document_id;        // 8 bytes
    uint64_t owner_id;           // 8 (16)
    uint8_t owner_type;          // 1 (17)
    
    char filename[256];          // 256 (273)
    char mime_type[64];          // 64 (337)
    uint64_t file_size;          // 8 (345)
    uint64_t upload_date;        // 8 (353)
    uint64_t expiry_date;        // 8 (361)
    
    // Storage
    uint64_t data_offset;        // 8 (369)
    uint32_t data_blocks;        // 4 (373)
    
    char description[256];       // 256 (629)
    char tags[128];              // 128 (757)
    
    // Padding to make exactly 1024 bytes
    // Total: 757 bytes, need 1024 - 757 = 267 bytes padding
    uint8_t reserved[267];

    DocumentMetadata() : document_id(0), owner_id(0), owner_type(0),
                         file_size(0), upload_date(0), expiry_date(0),
                         data_offset(0), data_blocks(0)
    {
        std::memset(filename, 0, sizeof(filename));
        std::memset(mime_type, 0, sizeof(mime_type));
        std::memset(description, 0, sizeof(description));
        std::memset(tags, 0, sizeof(tags));
        std::memset(reserved, 0, sizeof(reserved));
    }
};

static_assert(sizeof(DocumentMetadata) == 1024, "DocumentMetadata must be 1024 bytes");

// ============================================================================
// INCIDENT STRUCTURES
// ============================================================================

struct IncidentReport
{
    uint64_t incident_id;        // 8 bytes
    uint64_t driver_id;          // 8 (16)
    uint64_t vehicle_id;         // 8 (24)
    uint64_t trip_id;            // 8 (32)
    
    IncidentType type;           // 1 (33)
    uint64_t incident_time;      // 8 (41)
    
    // Location
    double latitude;             // 8 (49)
    double longitude;            // 8 (57)
    char location_address[256];  // 256 (313)
    
    // Details
    char description[512];       // 512 (825)
    char police_report_number[64]; // 64 (889)
    char insurance_claim_number[64]; // 64 (953)
    
    // Parties involved
    char other_party_info[256];  // 256 (1209)
    char witness_info[256];      // 256 (1465)
    
    // Damage/cost
    double estimated_damage;     // 8 (1473)
    double insurance_payout;     // 8 (1481)
    char currency[8];            // 8 (1489)
    
    // Documents
    uint64_t photo_doc_ids[5];   // 40 (1529)
    uint64_t report_doc_id;      // 8 (1537)
    
    // Status
    uint8_t is_resolved;         // 1 (1538)
    uint64_t resolved_date;      // 8 (1546)
    
    char notes[256];             // 256 (1802)
    
    // Padding to make exactly 2048 bytes
    // Total: 1802 bytes, need 2048 - 1802 = 246 bytes padding
    uint8_t reserved[246];

    IncidentReport() : incident_id(0), driver_id(0), vehicle_id(0), trip_id(0),
                       type(IncidentType::ACCIDENT), incident_time(0),
                       latitude(0), longitude(0), estimated_damage(0),
                       insurance_payout(0), report_doc_id(0), is_resolved(0),
                       resolved_date(0)
    {
        std::memset(location_address, 0, sizeof(location_address));
        std::memset(description, 0, sizeof(description));
        std::memset(police_report_number, 0, sizeof(police_report_number));
        std::memset(insurance_claim_number, 0, sizeof(insurance_claim_number));
        std::memset(other_party_info, 0, sizeof(other_party_info));
        std::memset(witness_info, 0, sizeof(witness_info));
        std::memset(currency, 0, sizeof(currency));
        std::memset(photo_doc_ids, 0, sizeof(photo_doc_ids));
        std::memset(notes, 0, sizeof(notes));
        std::memset(reserved, 0, sizeof(reserved));
    }
};

static_assert(sizeof(IncidentReport) == 2048, "IncidentReport must be 2048 bytes");

// ============================================================================
// SESSION STRUCTURES
// ============================================================================

struct SessionInfo
{
    char session_id[64];         // 64 bytes
    uint64_t driver_id;          // 8 (72)
    uint64_t login_time;         // 8 (80)
    uint64_t last_activity;      // 8 (88)
    uint32_t operations_count;   // 4 (92)
    char ip_address[64];         // 64 (156)
    uint8_t reserved[36];        // 36 (192 - exact size, no padding needed)

    SessionInfo() : driver_id(0), login_time(0), last_activity(0),
                    operations_count(0)
    {
        std::memset(session_id, 0, sizeof(session_id));
        std::memset(ip_address, 0, sizeof(ip_address));
        std::memset(reserved, 0, sizeof(reserved));
    }
};

// ============================================================================
// STATISTICS STRUCTURES
// ============================================================================

struct DatabaseStats
{
    uint64_t total_drivers;              // 8 bytes
    uint64_t active_drivers;             // 8 (16)
    uint64_t total_vehicles;             // 8 (24)
    uint64_t total_trips;                // 8 (32)
    uint64_t total_distance;             // 8 (40)
    uint64_t total_expenses;             // 8 (48)
    uint64_t total_maintenance_records;  // 8 (56)
    uint64_t total_documents;            // 8 (64)
    uint64_t total_incidents;            // 8 (72)
    uint64_t database_size;              // 8 (80)
    uint64_t used_space;                 // 8 (88)
    double fragmentation;                // 8 (96)
    uint32_t active_sessions;            // 4 (100)
    
    // Padding to make 128 bytes total
    uint8_t reserved[28];

    DatabaseStats() : total_drivers(0), active_drivers(0), total_vehicles(0),
                      total_trips(0), total_distance(0), total_expenses(0),
                      total_maintenance_records(0), total_documents(0),
                      total_incidents(0), database_size(0), used_space(0),
                      fragmentation(0), active_sessions(0)
    {
        std::memset(reserved, 0, sizeof(reserved));
    }
};

// ============================================================================
// COMPUTER VISION STRUCTURES (NEW FEATURE)
// ============================================================================

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

// Object detection record
struct ObjectDetection
{
    uint64_t detection_id;       // 8 bytes
    uint64_t trip_id;            // 8 (16)
    uint64_t timestamp;          // 8 (24)
    
    DetectionType type;          // 1 (25)
    float confidence;            // 4 (29)
    
    // Bounding box
    float bbox_x;                // 4 (33)
    float bbox_y;                // 4 (37)
    float bbox_width;            // 4 (41)
    float bbox_height;           // 4 (45)
    
    // Distance estimation
    float estimated_distance;    // 4 (49)
    float relative_speed;        // 4 (53)
    
    // Location
    double latitude;             // 8 (61)
    double longitude;            // 8 (69)
    
    // Camera info
    uint8_t camera_id;           // 1 (70)
    
    // Alert triggered
    uint8_t alert_triggered;     // 1 (71)
    char alert_message[128];     // 128 (199)
    
    // Padding to make exactly 256 bytes
    // Total: 199 bytes, need 256 - 199 = 57 bytes padding
    uint8_t reserved[57];

    ObjectDetection() : detection_id(0), trip_id(0), timestamp(0),
                        type(DetectionType::VEHICLE), confidence(0),
                        bbox_x(0), bbox_y(0), bbox_width(0), bbox_height(0),
                        estimated_distance(0), relative_speed(0),
                        latitude(0), longitude(0), camera_id(0),
                        alert_triggered(0)
    {
        std::memset(alert_message, 0, sizeof(alert_message));
        std::memset(reserved, 0, sizeof(reserved));
    }
};

static_assert(sizeof(ObjectDetection) == 256, "ObjectDetection must be 256 bytes");

// Driver behavior detection record
struct DriverBehaviorDetection
{
    uint64_t detection_id;       // 8 bytes
    uint64_t trip_id;            // 8 (16)
    uint64_t driver_id;          // 8 (24)
    uint64_t timestamp;          // 8 (32)
    
    DriverState state;           // 1 (33)
    float confidence;            // 4 (37)
    uint32_t duration;           // 4 (41)
    
    // Face detection
    uint8_t face_detected;       // 1 (42)
    float head_pitch;            // 4 (46)
    float head_yaw;              // 4 (50)
    float head_roll;             // 4 (54)
    
    // Eye tracking
    uint8_t eyes_detected;       // 1 (55)
    float eye_closure_ratio;     // 4 (59)
    uint8_t blink_count;         // 1 (60)
    
    // Attention metrics
    float attention_score;       // 4 (64)
    uint8_t looking_at_road;     // 1 (65)
    
    // Alert
    uint8_t alert_triggered;     // 1 (66)
    char alert_type[64];         // 64 (130)
    
    // Frame reference
    char frame_filename[128];    // 128 (258)
    
    // Padding to make exactly 512 bytes
    // Total: 258 bytes, need 512 - 258 = 254 bytes padding
    uint8_t reserved[254];

    DriverBehaviorDetection() : detection_id(0), trip_id(0), driver_id(0),
                                timestamp(0), state(DriverState::NORMAL),
                                confidence(0), duration(0), face_detected(0),
                                head_pitch(0), head_yaw(0), head_roll(0),
                                eyes_detected(0), eye_closure_ratio(0),
                                blink_count(0), attention_score(1.0),
                                looking_at_road(1), alert_triggered(0)
    {
        std::memset(alert_type, 0, sizeof(alert_type));
        std::memset(frame_filename, 0, sizeof(frame_filename));
        std::memset(reserved, 0, sizeof(reserved));
    }
};

static_assert(sizeof(DriverBehaviorDetection) == 512, "DriverBehaviorDetection must be 512 bytes");

// Vision analytics summary
struct VisionAnalytics
{
    uint64_t trip_id;                    // 8 bytes
    
    // Object detection stats
    uint32_t total_vehicles_detected;    // 4 (12)
    uint32_t total_pedestrians_detected; // 4 (16)
    uint32_t total_cyclists_detected;    // 4 (20)
    uint32_t total_traffic_signs_detected; // 4 (24)
    uint32_t total_obstacles_detected;   // 4 (28)
    
    // Collision warnings
    uint32_t forward_collision_warnings; // 4 (32)
    uint32_t lane_departure_warnings;    // 4 (36)
    uint32_t blind_spot_warnings;        // 4 (40)
    
    // Driver behavior stats
    uint32_t drowsiness_events;          // 4 (44)
    uint32_t distraction_events;         // 4 (48)
    uint32_t phone_usage_events;         // 4 (52)
    uint32_t total_attention_lapses;     // 4 (56)
    
    // Safety score impact
    float vision_safety_score;           // 4 (60)
    
    // Padding to make exactly 320 bytes
    // Total: 60 bytes, need 320 - 60 = 260 bytes padding
    uint8_t reserved[260];

    VisionAnalytics() : trip_id(0), total_vehicles_detected(0),
                        total_pedestrians_detected(0), total_cyclists_detected(0),
                        total_traffic_signs_detected(0), total_obstacles_detected(0),
                        forward_collision_warnings(0), lane_departure_warnings(0),
                        blind_spot_warnings(0), drowsiness_events(0),
                        distraction_events(0), phone_usage_events(0),
                        total_attention_lapses(0), vision_safety_score(100.0)
    {
        std::memset(reserved, 0, sizeof(reserved));
    }
};

static_assert(sizeof(VisionAnalytics) == 320, "VisionAnalytics must be 320 bytes");

#pragma pack(pop)  // Restore default alignment

#endif // SDM_TYPES_HPP