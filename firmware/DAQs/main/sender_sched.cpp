#include "sender_sched.h"
#include "vcan_protocol.h"
#include "vcan_sender.h"
#include "daq_cache.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "Globals.h"

static const char *TAG = "sender_sched";
#define MAX_PGNS 32

typedef struct
{
    uint32_t pgn;
    uint8_t priority;
    uint32_t min_interval_ms;
    uint32_t last_sent_ms;
    bool enabled;
} sched_entry_t;

static sched_entry_t s_table[MAX_PGNS];
static int s_count = 0;

static inline uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000ULL); }

void sender_sched_init(void)
{
    memset(s_table, 0, sizeof(s_table));
    s_count = 0;
    ESP_LOGI(TAG, "sender scheduler initialized");
}

void sender_sched_register(uint32_t pgn, uint8_t priority, uint32_t min_interval_ms)
{
    if (s_count >= MAX_PGNS)
    {
        ESP_LOGI(TAG, "sender_sched_register: table full, cannot register PGN 0x%04X", (unsigned)pgn);
        return;
    }
    s_table[s_count].pgn = pgn;
    s_table[s_count].priority = priority;
    s_table[s_count].min_interval_ms = min_interval_ms;
    s_table[s_count].last_sent_ms = 0;
    s_table[s_count].enabled = true;
    s_count++;
    ESP_LOGI(TAG, "registered PGN 0x%04X prio=%u interval=%ums", (unsigned)pgn, priority, (unsigned)min_interval_ms);
}

static int cmp_sched(const void *a, const void *b)
{
    const sched_entry_t *pa = (const sched_entry_t *)a;
    const sched_entry_t *pb = (const sched_entry_t *)b;
    if (pa->priority != pb->priority)
        return (int)pa->priority - (int)pb->priority;
    if (pa->last_sent_ms < pb->last_sent_ms)
        return -1;
    if (pa->last_sent_ms > pb->last_sent_ms)
        return 1;
    return 0;
}

void send_pgn_now(uint32_t pgn)
{
    auto &sender = vcan::Sender::instance();

    // Retrieve thread-safe DAQ cache snapshot
    daq_cache_t cache{};
    if (daq_cache_get(&cache) != ESP_OK)
    {
        return; // Cache lock or retrieval failed
    }

    switch (pgn)
    {
    case PGN_HEARTBEAT:
    {
        pgn_heartbeat_t msg{};
        static uint8_t hb_seq = 0;
        msg.seq = ++hb_seq;
        msg.protocol_version = 1;
        msg.uptime_ms = now_ms();
        sender.send(msg);
        break;
    }
    case PGN_ENGINE_CORE:
    {
        pgn_engine_core_t msg{};
        msg.rpm          = (uint16_t)cache.rpm;          // [0..1]
        msg.speed        = (int16_t)cache.speed;         // [2..3]
        msg.gear_position = (int16_t)cache.gearPosition; // [4..5]
        msg.coolant_temp = (int16_t)cache.coolantTemp;   // [6..7]
        sender.send(msg);
        break;
    }
    case PGN_PRESSURES:
    {
        pgn_pressures_t msg{};
        msg.oil_pressure  = (int16_t)cache.oilPressure;  // [0..1]
        msg.fuel_pressure = (int16_t)cache.fuelPressure; // [2..3]
        msg.boost_pressure = (int16_t)cache.boostPressure;// [4..5]
        msg.battery_level = (int16_t)cache.batteryLevel; // [6..7]
        sender.send(msg);
        break;
    }
    case PGN_TEMPS:
    {
        pgn_temps_t msg{};
        msg.oil_temp     = (int16_t)cache.oilTemp;     // [0..1]
        msg.trans_temp   = (int16_t)cache.transTemp;   // [2..3] Fixed!
        msg.ambient_temp = (int16_t)cache.ambientTemp; // [4..5]
        msg.ia_temp      = (int16_t)cache.iaTemp;      // [6..7]
        sender.send(msg);
        break;
    }
    case PGN_EXHAUST_DIG:
    {
        pgn_exhaust_dig_t msg{};
        msg.eg_temp      = (int32_t)cache.egTemp;      // [0..3]
        msg.fuel_level   = (int16_t)cache.fuelLevel;   // [6..7]
        msg.digital_pins = (uint16_t)cache.digitalPins; // [8..9]
        sender.send(msg);
        break;
    }
    case PGN_CRUISE_ODO:
    {
        pgn_cruise_odo_t msg{};
        msg.cruise_active    = 0;                             // [0..1]
        msg.cruise_set_value = 0;                             // [2..3]
        msg.odometer_tenths  = (uint32_t)cache.odometerTenths;// [4..7]
        sender.send(msg);
        break;
    }
    case PGN_GPS_POS:
    {
        pgn_gps_pos_t msg{};
        msg.lat = cache.lat; // [0..3]
        msg.lon = cache.lon; // [4..7]
        sender.send(msg);
        break;
    }
    case PGN_GPS_MOTION:
    {
        pgn_gps_motion_t msg{};
        msg.gps_speed    = (uint32_t)cache.gpsSpeed;   // [0..3]
        msg.gps_altitude = (uint16_t)cache.gpsAltitude;// [4..5]
        msg.heading_deg  = (int16_t)cache.headingDeg;  // [6..7]
        sender.send(msg);
        break;
    }
    case PGN_GPS_STATUS:
    {
        pgn_gps_status_t msg{};
        msg.gps_fix       = cache.gpsFix;                       // [0]
        msg.gps_sat_count = cache.gpsSatCount;                 // [1]
        memcpy(msg.compass4, cache.compass4, sizeof(msg.compass4)); // [2..5]
        sender.send(msg);
        break;
    }
    case PGN_GNSS_TIME:
    {
        pgn_gnss_time_t msg{};
        msg.year   = cache.gnssYear;   // [0..1]
        msg.month  = cache.gnssMonth;  // [2]
        msg.day    = cache.gnssDay;    // [3]
        msg.hour   = cache.gnssHour;   // [4]
        msg.minute = cache.gnssMinute; // [5]
        msg.second = cache.gnssSecond; // [6]
        msg.flags  = 0;                // [7]
        sender.send(msg);
        break;
    }
    // case PGN_IMU_DYNAMICS:
    // {
    //     pgn_imu_dynamics_t msg{};
    //     msg.accel_x = (int16_t)cache.accelX; // [0..1]
    //     msg.accel_y = (int16_t)cache.accelY; // [2..3]
    //     msg.accel_z = (int16_t)cache.accelZ; // [4..5]
    //     sender.send(msg);
    //     break;
    // }
    default:
        break;
    }
}

void sender_sched_tick(void)
{
    uint32_t now = now_ms();

    sched_entry_t candidates[MAX_PGNS];
    int cand_count = 0;
    for (int i = 0; i < s_count; ++i)
    {
        if (!s_table[i].enabled)
            continue;
        uint32_t elapsed = now - s_table[i].last_sent_ms;
        if (elapsed >= s_table[i].min_interval_ms)
        {
            candidates[cand_count++] = s_table[i];
        }
    }

    if (cand_count == 0)
        return;

    qsort(candidates, cand_count, sizeof(sched_entry_t), cmp_sched);

    const int MAX_SEND_PER_TICK = 3;
    int sends = 0;

    for (int ci = 0; ci < cand_count && sends < MAX_SEND_PER_TICK; ++ci)
    {
        uint32_t pgn = candidates[ci].pgn;

        send_pgn_now(pgn);

        for (int i = 0; i < s_count; ++i)
        {
            if (s_table[i].pgn == pgn)
            {
                s_table[i].last_sent_ms = now;
                break;
            }
        }

        ESP_LOGI(TAG, "sent PGN 0x%04X prio=%u", (unsigned)pgn, candidates[ci].priority);
        sends++;
    }
}