#include <math.h>

// Source Addresses
#define ADDR_ECU_DAQ      0xFA
#define ADDR_DISPLAY_NODE 0xFB

// Protocol version
#define VCAN_PROTOCOL_VERSION 1

// Standard OpenECU-style Priorities
#define CAN_PRIORITY_HIGH   3
#define CAN_PRIORITY_MED    4
#define CAN_PRIORITY_LOW    6

// PGNs (must match transmitter)
#define PGN_ENGINE_CORE    0xFF01
#define PGN_PRESSURES      0xFF02
#define PGN_TEMPS          0xFF03
#define PGN_EXHAUST_DIG    0xFF04
#define PGN_IMU_DYNAMICS   0xFF05
#define PGN_CRUISE_ODO     0xFF06
#define PGN_GPS_POS        0xFF10
#define PGN_GPS_MOTION     0xFF11
#define PGN_GPS_STATUS     0xFF12
#define PGN_GNSS_TIME      0xFF13
#define PGN_HEARTBEAT      0xFF20

// Field offsets (wire layout) for sender wrappers
// Use these constants in both sender and receiver so packing cannot drift.
// All offsets are 0-based byte indices into the 8-byte payload.

// PGN_ENGINE_CORE payload layout (4 x int16 LE)
// [0..1] rpm; [2..3] speed; [4..5] gear_position; [6..7] coolant_temp
#define ENGINE_RPM_OFFSET           0
#define ENGINE_SPEED_OFFSET         2
#define ENGINE_GEAR_OFFSET          4
#define ENGINE_COOLANT_OFFSET       6

// PGN_PRESSURES payload layout (4 x int16 LE)
// [0..1] oil_pressure; [2..3] fuel_pressure; [4..5] boost_pressure; [6..7] battery_level
#define PRESSURE_OIL_OFFSET         0
#define PRESSURE_FUEL_OFFSET        2
#define PRESSURE_BOOST_OFFSET       4
#define PRESSURE_BATTERY_OFFSET     6

// PGN_TEMPS payload layout (4 x int16 LE)
// [0..1] oil_temp; [2..3] trans_temp; [4..5] ambient_temp; [6..7] ia_temp
#define TEMP_OIL_OFFSET             0
#define TEMP_TRANS_OFFSET           2
#define TEMP_AMBIENT_OFFSET         4
#define TEMP_IAT_OFFSET             6

// PGN_EXHAUST_DIG payload layout
// [0..1] eg_temp (int16 LE); [2..3] fuel_level (int16 LE); [4..5] digital_pins (uint16 LE); [6..7] reserved
#define EXHAUST_EGT_OFFSET          0
#define EXHAUST_FUEL_OFFSET         4
#define EXHAUST_DIGITAL_OFFSET      6

// PGN_IMU_DYNAMICS payload layout (3 x int16 LE)
// [0..1] accel_x; [2..3] accel_y; [4..5] accel_z; [6..7] reserved
#define IMU_ACCEL_X_OFFSET          0
#define IMU_ACCEL_Y_OFFSET          2
#define IMU_ACCEL_Z_OFFSET          4

// PGN_CRUISE_ODO payload layout
// [0..1] cruise_active (uint16 LE); [2..3] cruise_set_value (uint16 LE); [4..7] odometer_tenths (uint32 LE)
#define CRUISE_ACTIVE_OFFSET        0
#define CRUISE_SET_OFFSET           2
#define ODOMETER_TENTHS_OFFSET      4

// PGN_GPS_POS payload layout
// [0..3] lat (int32 LE); [4..7] lon (int32 LE)
#define GPS_POS_LAT_OFFSET          0
#define GPS_POS_LON_OFFSET          4

// PGN_GPS_MOTION payload layout
// [0..3] gps_speed (uint32 LE); [4..5] gps_altitude (uint16 LE); [6..7] heading_deg (int16 LE)
#define GPS_MOTION_SPEED_OFFSET     0
#define GPS_MOTION_ALT_OFFSET       4
#define GPS_MOTION_HEADING_OFFSET   6

// PGN_GPS_STATUS payload layout
// [0] gps_fix (uint8); [1] gps_sat_count (uint8); [2..5] compass4 (4 chars); [6..7] reserved
#define GPS_STATUS_FIX_OFFSET       0
#define GPS_STATUS_SAT_OFFSET       1
#define GPS_STATUS_COMPASS_OFFSET   2

// PGN_GNSS_TIME payload layout (8 bytes)
// [0..1] year (uint16 LE); [2] month; [3] day; [4] hour; [5] minute; [6] second; [7] flags/reserved
#define GNSS_YEAR_OFFSET            0
#define GNSS_MONTH_OFFSET           2
#define GNSS_DAY_OFFSET             3
#define GNSS_HOUR_OFFSET            4
#define GNSS_MINUTE_OFFSET          5
#define GNSS_SECOND_OFFSET          6
#define GNSS_FLAGS_OFFSET           7

// PGN_HEARTBEAT payload layout (example)
// [0] seq (uint8); [1] protocol_version (uint8); [2..5] uptime_ms (uint32 LE); [6..7] reserved
#define HB_SEQ_OFFSET               0
#define HB_VERSION_OFFSET           1
#define HB_UPTIME_OFFSET            2

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


// Inline Identifier Helpers (simplified OpenECU mapping)
static inline uint32_t vcan_build_id(uint8_t priority, uint16_t pgn, uint8_t src_addr) {
    return ((uint32_t)(priority & 0x07) << 26) |
           ((uint32_t)(pgn & 0xFFFF) << 8) |
           (uint32_t)(src_addr & 0xFF);
}
static inline uint8_t vcan_get_priority(uint32_t id) {
    return (uint8_t)((id >> 26) & 0x07);
}
static inline uint16_t vcan_get_pgn(uint32_t id) {
    return (uint16_t)((id >> 8) & 0xFFFF);
}
static inline uint8_t vcan_get_src(uint32_t id) {
    return (uint8_t)(id & 0xFF);
}

// Deterministic little-endian serializers (use these for wire format)
static inline void vcan_write_u16_le(uint8_t *buf, uint16_t v) {
    buf[0] = (uint8_t)(v & 0xFF);
    buf[1] = (uint8_t)((v >> 8) & 0xFF);
}
static inline void vcan_write_u32_le(uint8_t *buf, uint32_t v) {
    buf[0] = (uint8_t)(v & 0xFF);
    buf[1] = (uint8_t)((v >> 8) & 0xFF);
    buf[2] = (uint8_t)((v >> 16) & 0xFF);
    buf[3] = (uint8_t)((v >> 24) & 0xFF);
}

static inline uint16_t vcan_read_u16_le(const uint8_t *buf) {
    return (uint16_t)(buf[0] | (buf[1] << 8));
}
static inline uint32_t vcan_read_u32_le(const uint8_t *buf) {
    return (uint32_t)(buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24));
}

static inline int16_t vcan_read_s16_le(const uint8_t *b) { return (int16_t)vcan_read_u16_le(b); }
static inline int32_t vcan_read_s32_le(const uint8_t *b) { return (int32_t)vcan_read_u32_le(b); }
