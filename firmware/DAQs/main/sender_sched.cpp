// sender_sched.cpp
#include "sender_sched.h"
#include "vcan_protocol.h"
#include "DFRobot_GNSS.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>      // for llround
#include <stdint.h>

#include "Globals.h"
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
    if (daq_cache_get(&cache) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to get DAQ cache snapshot for PGN 0x%04X", (unsigned)pgn);
        return;
    }

    // Snapshot cache values into locals for dispatching
    uint16_t local_rpm    = cache.rpm;
    int local_speed       = cache.speed;
    int local_gear        = cache.gearPosition;
    int local_coolant     = cache.coolantTemp;

    int16_t local_oilP    = cache.oilPressure;
    int16_t local_fuelP   = cache.fuelPressure;
    int16_t local_boostP  = cache.boostPressure;
    int16_t local_batt    = cache.batteryLevel;

    int16_t local_oilT    = cache.oilTemp;
    int16_t local_transT  = cache.transTemp;
    int16_t local_ambient = cache.ambientTemp;
    int16_t local_iaT     = cache.iaTemp;

    int32_t local_EGTemp  = cache.egTemp;
    int16_t local_fuelLvl = cache.fuelLevel;
    uint16_t local_dig    = cache.digitalPins;

    uint16_t local_cruiseActive = cache.cruiseActive;
    uint16_t local_cruiseSet    = cache.cruiseSetValue;
    uint32_t local_odo          = cache.odometerTenths;

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
        vcan_send_cruise_odo(local_cruiseActive, local_cruiseSet, local_odo);
        break;

    case PGN_GPS_POS:
        vcan_send_gps_position(cache.lat, cache.lon);
        ESP_LOGI(TAG, "Packing GPS POS: lat_i=%d, lon_i=%d", cache.lat, cache.lon);
        break;

    case PGN_GPS_MOTION:
        vcan_send_gps_motion((uint32_t)cache.gpsSpeed, cache.gpsAltitude, cache.headingDeg);
        break;

    case PGN_GPS_STATUS:
        vcan_send_gps_status(cache.gpsFix, cache.gpsSatCount, cache.compass4);
        ESP_LOGI(TAG, "Sending GPS_STATUS: fix=%d sat=%d compass=\"%.4s\"", 
                 cache.gpsFix, cache.gpsSatCount, cache.compass4);
        break;

    case PGN_GNSS_TIME:
        ESP_LOGI(TAG, "Sending GNSS_TIME: %04d-%02d-%02d %02d:%02d:%02d", 
                 cache.gnssYear, cache.gnssMonth, cache.gnssDay, 
                 cache.gnssHour, cache.gnssMinute, cache.gnssSecond);

        vcan_send_gnss_time(cache.gnssYear, cache.gnssMonth, cache.gnssDay, 
                            cache.gnssHour, cache.gnssMinute, cache.gnssSecond);
        break;

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