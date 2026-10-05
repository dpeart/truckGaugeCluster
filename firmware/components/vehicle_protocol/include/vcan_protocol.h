#ifndef VCAN_PROTOCOL_H
#define VCAN_PROTOCOL_H

#include <stdint.h>

// Priority levels (0 = Highest, 7 = Lowest)
#define CAN_PRIORITY_HIGH  2
#define CAN_PRIORITY_MED   3
#define CAN_PRIORITY_LOW   6

#define PGN_HEARTBEAT    0x01
#define PGN_ENGINE_CORE  0x02
#define PGN_PRESSURES    0x03
#define PGN_TEMPS        0x04
#define PGN_EXHAUST_DIG  0x05
#define PGN_CRUISE_ODO   0x06
#define PGN_GPS_POS      0x07
#define PGN_GPS_MOTION   0x08
#define PGN_GPS_STATUS   0x09
#define PGN_GNSS_TIME    0x0A
#define PGN_IMU_DYNAMICS 0x0B

// ---------------------------------------------------------
// PGN Structures (Strict Byte Alignment matching LE Layouts)
// ---------------------------------------------------------

// PGN_HEARTBEAT
typedef struct {
    uint8_t seq;               // [0]
    uint8_t protocol_version;  // [1]
    uint32_t uptime_ms;        // [2..5]
    uint16_t reserved;         // [6..7]
} pgn_heartbeat_t;

// PGN_ENGINE_CORE
typedef struct {
    uint16_t rpm;              // [0..1]
    int16_t speed;             // [2..3]
    int16_t gear_position;     // [4..5]
    int16_t coolant_temp;      // [6..7]
} pgn_engine_core_t;

// PGN_PRESSURES
typedef struct {
    int16_t oil_pressure;      // [0..1]
    int16_t fuel_pressure;     // [2..3]
    int16_t boost_pressure;    // [4..5]
    int16_t battery_level;     // [6..7]
} pgn_pressures_t;

// PGN_TEMPS
typedef struct {
    int16_t oil_temp;          // [0..1]
    int16_t trans_temp;        // [2..3]
    int16_t ambient_temp;      // [4..5]
    int16_t ia_temp;           // [6..7]
} pgn_temps_t;

// PGN_EXHAUST_DIG
typedef struct {
    int32_t eg_temp;           // [0..3]
    int16_t fuel_level;        // [4..5] (Offset 6)
    uint16_t digital_pins;     // [6..7] (Offset 8)
} pgn_exhaust_dig_t;

// PGN_IMU_DYNAMICS
typedef struct {
    int16_t accel_x;           // [0..1]
    int16_t accel_y;           // [2..3]
    int16_t accel_z;           // [4..5]
    uint16_t reserved;         // [6..7]
} pgn_imu_dynamics_t;

// PGN_CRUISE_ODO
typedef struct {
    uint16_t cruise_active;    // [0..1]
    uint16_t cruise_set_value; // [2..3]
    uint32_t odometer_tenths;  // [4..7]
} pgn_cruise_odo_t;

// PGN_GPS_POS
typedef struct {
    int32_t lat;               // [0..3]
    int32_t lon;               // [4..7]
} pgn_gps_pos_t;

// PGN_GPS_MOTION
typedef struct {
    uint32_t gps_speed;        // [0..3]
    uint16_t gps_altitude;     // [4..5]
    int16_t heading_deg;       // [6..7]
} pgn_gps_motion_t;

// PGN_GPS_STATUS
typedef struct {
    uint8_t gps_fix;           // [0]
    uint8_t gps_sat_count;     // [1]
    uint8_t compass4[4];       // [2..5]
    uint16_t reserved;         // [6..7]
} pgn_gps_status_t;

// PGN_GNSS_TIME
typedef struct {
    uint16_t year;             // [0..1]
    uint8_t month;             // [2]
    uint8_t day;               // [3]
    uint8_t hour;              // [4]
    uint8_t minute;            // [5]
    uint8_t second;            // [6]
    uint8_t flags;             // [7]
} pgn_gnss_time_t;

// ---------------------------------------------------------
// Digital Input Card Channels
// ---------------------------------------------------------
#define DIG_OVER_DRIVE     1
#define DIG_TCC            2
#define DIG_LEFT           3
#define DIG_RIGHT          4
#define DIG_BRAKE          5
#define DIG_HEAD_LOW       6
#define DIG_HEAD_HIGH      7
#define DIG_RUNNING        8
#define DIG_WATER_FUEL     9
#define DIG_LOW_WASHER     10
#define DIG_CRUISE_ON      11
#define DIG_CRUISE_SET     12
#define DIG_CRUISE_RESUME  13
#define DIG_BRAKE_LIGHT    14
#define DIG_IGNITION       15

#define VCAN_BIT(n) (1U << ((n) - 1))

#endif // VCAN_PROTOCOL_H