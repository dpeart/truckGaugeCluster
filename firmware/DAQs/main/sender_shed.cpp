// sender_sched.cpp
#include "sender_sched.h"
#include "vcan_protocol.h"
#include "GNSS.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>      // for llround
#include <stdint.h>

#include "globals.h"
#include "vcan_sender.h"
#include "daq_cache.h"

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

// comparator: lower numeric priority value = higher priority. Tie-break by oldest last_sent_ms.
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

/* Build and send the PGN using existing vcan_send_* helpers.
   Reads a clean, thread-safe snapshot from the DAQ cache layer before building payloads.
*/
static void send_pgn_now(uint32_t pgn)
{
    // Snapshot the central DAQ cache atomically
    daq_cache_t cache;
    daq_get_snapshot(&cache);

    // Snapshot cache values into locals for dispatching
    uint16_t local_rpm = cache.rpm;
    int local_speed = cache.speed;
    int local_gear = cache.gearPosition;
    int local_coolant = cache.coolantTemp;

    int16_t local_oilP = (int16_t)cache.oilPressure;
    int16_t local_fuelP = (int16_t)cache.fuelPressure;
    int16_t local_boostP = (int16_t)cache.boostPressure;
    int16_t local_batt = (int16_t)cache.batteryLevel;

    int16_t local_oilT = (int16_t)cache.oilTemp;
    int16_t local_transT = (int16_t)cache.transTemp;
    int16_t local_ambient = (int16_t)cache.ambientTemp;
    int16_t local_iaT = (int16_t)cache.iaTemp;

    int16_t local_EGTemp = (int16_t)cache.EGTemp;
    int16_t local_fuelLvl = (int16_t)cache.fuelLevel;
    uint16_t local_dig = cache.digitalPins;

    bool local_cruiseActive = (cruiseActive != 0); // Preserved from control state
    int local_cruiseSet = (int)cruiseSetValue;     // Preserved from control state
    uint32_t local_odo = cache.odometerTenths;     // Sourced directly from DAQ cache (VSS or Sim)

    // --- GPS snapshot (from DAQ snapshot cache & GNSS module) ---
    double gps_lat = cache.latitude;
    double gps_lon = cache.longitude;
    int gps_heading_scaled = cache.headingScaled; // centi-degrees (heading * 100)
    bool gps_has_fix = cache.hasFix;
    const char *gps_compass8 = gps.getCompass8();

    ESP_LOGI(TAG, "GPS snapshot: heading_scaled=%d lat=%.6f lon=%.6f fix=%d",
             gps_heading_scaled, gps_lat, gps_lon, gps_has_fix);

    switch (pgn)
    {
    case PGN_HEARTBEAT:
    {
        static uint8_t hb_seq = 0;
        hb_seq++;
        uint32_t now = now_ms();
        vcan_send_heartbeat(hb_seq, now);
        break;
    }

    case PGN_ENGINE_CORE:
        vcan_send_engine_core((uint16_t)local_rpm, (int16_t)local_speed, (int16_t)local_gear, (int16_t)local_coolant);
        break;

    case PGN_PRESSURES:
        vcan_send_pressures(local_oilP, local_fuelP, local_boostP, local_batt);
        break;

    case PGN_TEMPS:
        vcan_send_temps(local_oilT, local_transT, local_ambient, local_iaT);
        break;

    case PGN_EXHAUST_DIG:
        vcan_send_exhaust_dig(local_EGTemp, local_fuelLvl, local_dig);
        break;

    case PGN_CRUISE_ODO:
        vcan_send_cruise_odo((uint16_t)local_cruiseActive, (uint16_t)local_cruiseSet, local_odo);
        break;

    case PGN_GPS_POS:
    {
        // Convert degrees -> 1e-7 degrees signed int32 (little-endian in vcan_send)
        // Use llround to avoid truncation
        int32_t lat_i = (int32_t)llround(gps_lat * 1e7);
        int32_t lon_i = (int32_t)llround(gps_lon * 1e7);

        ESP_LOGI(TAG, "Packing GPS POS: lat_deg=%.7f -> lat_i=%d, lon_deg=%.7f -> lon_i=%d",
                 gps_lat, lat_i, gps_lon, lon_i);

        vcan_send_gps_position(lat_i, lon_i);
        break;
    }

    case PGN_GPS_MOTION:
        // gps_heading_scaled is centi-degrees (heading * 100) per GNSS.h getHeadingScaled()
        // vcan_send_gps_motion expects (speed, altitude, heading_deg)
        vcan_send_gps_motion((uint16_t)cache.speedSog, (int16_t)cache.altitude, (int16_t)gps_heading_scaled);
        break;

    case PGN_GPS_STATUS:
        // send compass string if available (first 4 chars)
        if (gps_compass8 && gps_compass8[0] != '\0')
        {
            // vcan_send_gps_status expects a 4-char buffer; pass pointer (function copies 4 bytes)
            vcan_send_gps_status(gps_has_fix ? 1 : 0, 0, gps_compass8);
            ESP_LOGI(TAG, "Sending GPS_STATUS: fix=%d compass=\"%.4s\"", gps_has_fix ? 1 : 0, gps_compass8);
        }
        else
        {
            vcan_send_gps_status(gps_has_fix ? 1 : 0, 0, NULL);
            ESP_LOGI(TAG, "Sending GPS_STATUS: fix=%d compass=NULL", gps_has_fix ? 1 : 0);
        }
        break;

    case PGN_GNSS_TIME:
    {
        // Use GNSSModule cached local time getters (these exist in GNSS.h)
        int h = gps.getLocalHour();
        int m = gps.getLocalMinute();
        int s = gps.getLocalSecond();
        int mo = gps.getLocalMonth();
        int d = gps.getLocalDay();
        int y = gps.getLocalYear();

        ESP_LOGI(TAG, "Sending GNSS_TIME: %04d-%02d-%02d %02d:%02d:%02d", y, mo, d, h, m, s);

        vcan_send_gnss_time((uint16_t)y, (uint8_t)mo, (uint8_t)d, (uint8_t)h, (uint8_t)m, (uint8_t)s);
        break;
    }

    default:
        ESP_LOGI(TAG, "send_pgn_now: unhandled PGN 0x%04X", (unsigned)pgn);
        break;
    }
}

void sender_sched_tick(void)
{
    uint32_t now = now_ms();

    // Build candidate list
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

    // sort candidates by priority then age
    qsort(candidates, cand_count, sizeof(sched_entry_t), cmp_sched);

    // send up to N packets this tick to avoid bursts
    const int MAX_SEND_PER_TICK = 3;
    int sends = 0;

    for (int ci = 0; ci < cand_count && sends < MAX_SEND_PER_TICK; ++ci)
    {
        uint32_t pgn = candidates[ci].pgn;

        send_pgn_now(pgn);

        // update master table's last_sent_ms
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