// small_gauge.c
// Small gauge module: consumes only the PGNs needed for the "last display"
// - digitalPins      <- PGN_EXHAUST_DIG (EXHAUST_DIGITAL_OFFSET, uint16 LE)
// - fuelLevel        <- PGN_EXHAUST_DIG (EXHAUST_FUEL_OFFSET, int16 LE, scaled)
// - batteryLevel     <- PGN_PRESSURES   (PRESSURE_BATTERY_OFFSET, int16 LE, scaled)
// - coolant/oil temp <- PGN_ENGINE_CORE (ENGINE_COOLANT_OFFSET, int16 LE)
// - ambientTemp      <- PGN_TEMPS       (TEMP_AMBIENT_OFFSET, int16 LE)
// - compass8         <- PGN_GPS_STATUS  (GPS_STATUS_COMPASS_OFFSET, 4 chars)
// - time (hour/min)  <- PGN_GNSS_TIME   (GNSS_HOUR_OFFSET / GNSS_MINUTE_OFFSET, uint8)
// - daq_uptime_ms    <- PGN_HEARTBEAT   (HB_UPTIME_OFFSET, uint32 LE)

#include "small_gauge.h"
#include "vcan_protocol.h"
#include "vcan_receiver.h"
#include "updateUI.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include <string.h>
#include "digitalPins.h"

static const char *TAG = "small_gauge";

typedef struct
{
    int16_t  fuelLevel;      // raw fuel (0–10000)
    int16_t  batteryLevel;   // raw battery (0–1000)
    int16_t  coolantTemp;    // raw coolant temp
    int16_t  ambientTemp;    // raw ambient temp

    uint16_t digitalPins;    // raw digital bitmap

    char     compass8[8];    // heading string
    uint8_t  hour;
    uint8_t  minute;

    uint32_t daq_uptime_ms;
    uint32_t last_update_ms;

} sg_state_t;

static sg_state_t s_state;
static SemaphoreHandle_t s_mutex = NULL;

/* Latest UI-ready packet */
static GaugePacket s_last_pkt;

static inline uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

const GaugePacket *small_gauge_get_last_packet(void)
{
    return &s_last_pkt;
}

/* --- Debug helper: decode GPS payloads on receive (temporary) --- */
static void debug_gps_decode(uint16_t pgn, uint8_t src, const uint8_t *payload, uint8_t len, void *ctx)
{
    // Log raw payload bytes
    ESP_LOGI("VCAN_DBG", "DBG RX PGN=0x%04X src=%u len=%u raw: %02X %02X %02X %02X %02X %02X %02X %02X",
             pgn, src, len,
             len>0?payload[0]:0, len>1?payload[1]:0, len>2?payload[2]:0, len>3?payload[3]:0,
             len>4?payload[4]:0, len>5?payload[5]:0, len>6?payload[6]:0, len>7?payload[7]:0);

    // Try common decodes for PGN_GPS_POS (signed int32 LE lat/lon 1e-7) and heading
    if (pgn == PGN_GPS_POS && len >= 8) {
        int32_t lat_i = (int32_t)((uint32_t)payload[0] | ((uint32_t)payload[1]<<8) | ((uint32_t)payload[2]<<16) | ((uint32_t)payload[3]<<24));
        int32_t lon_i = (int32_t)((uint32_t)payload[4] | ((uint32_t)payload[5]<<8) | ((uint32_t)payload[6]<<16) | ((uint32_t)payload[7]<<24));
        double lat = lat_i / 1e7;
        double lon = lon_i / 1e7;
        ESP_LOGI("VCAN_DBG", "DBG decoded PGN_GPS_POS lat=%.7f lon=%.7f (raw lat_i=%d lon_i=%d)", lat, lon, lat_i, lon_i);
    }
    // Some implementations put heading in a motion PGN or in first two bytes; attempt safe decode
    if (pgn == PGN_GPS_POS && len >= 2) {
        int16_t head_i = (int16_t)((uint16_t)payload[0] | ((uint16_t)payload[1]<<8));
        ESP_LOGI("VCAN_DBG", "DBG decoded PGN_GPS_POS heading_raw=%d -> %.2f deg", head_i, head_i / 100.0);
    }
    if (pgn == PGN_GPS_STATUS && len >= 4) {
        char comp[5] = {0};
        memcpy(comp, &payload[GPS_STATUS_COMPASS_OFFSET], 4);
        comp[4] = '\0';
        ESP_LOGI("VCAN_DBG", "DBG PGN_GPS_STATUS compass raw bytes: %02X %02X %02X %02X -> \"%s\"",
                 payload[GPS_STATUS_COMPASS_OFFSET], payload[GPS_STATUS_COMPASS_OFFSET+1],
                 payload[GPS_STATUS_COMPASS_OFFSET+2], payload[GPS_STATUS_COMPASS_OFFSET+3], comp);
    }
    if (pgn == PGN_GNSS_TIME && len >= (GNSS_MINUTE_OFFSET + 1)) {
        ESP_LOGI("VCAN_DBG", "DBG PGN_GNSS_TIME hour=%u minute=%u", payload[GNSS_HOUR_OFFSET], payload[GNSS_MINUTE_OFFSET]);
    }
}

/* --- PGN callbacks --- */

static void cb_engine_core(uint16_t pgn, uint8_t src,
                           const uint8_t *payload, uint8_t len, void *ctx)
{
    if (len < (ENGINE_COOLANT_OFFSET + 2)) return;

    int16_t coolant = vcan_read_s16_le(&payload[ENGINE_COOLANT_OFFSET]);

    if (xSemaphoreTake(s_mutex, 0) == pdTRUE)
    {
        s_state.coolantTemp    = coolant;
        s_state.last_update_ms = now_ms();
        xSemaphoreGive(s_mutex);
    }

    ESP_LOGI(TAG, "PGN_ENGINE_CORE: coolant=%d", coolant);
}

static void cb_heartbeat(uint16_t pgn, uint8_t src,
                         const uint8_t *payload, uint8_t len, void *ctx)
{
    if (len < (HB_UPTIME_OFFSET + 4)) return;

    uint32_t uptime = vcan_read_u32_le(&payload[HB_UPTIME_OFFSET]);

    if (xSemaphoreTake(s_mutex, 0) == pdTRUE)
    {
        s_state.daq_uptime_ms  = uptime;
        s_state.last_update_ms = now_ms();
        xSemaphoreGive(s_mutex);
    }

    ESP_LOGI(TAG, "PGN_HEARTBEAT: uptime=%u", uptime);
}

static void cb_exhaust_dig(uint16_t pgn, uint8_t src,
                           const uint8_t *payload, uint8_t len, void *ctx)
{
    /* require at least up through digital offset + 2 bytes */
    if (len < (EXHAUST_DIGITAL_OFFSET + 2)) return;

    int16_t  fuel = vcan_read_s16_le(&payload[EXHAUST_FUEL_OFFSET]);
    uint16_t dig  = vcan_read_u16_le(&payload[EXHAUST_DIGITAL_OFFSET]);

    if (xSemaphoreTake(s_mutex, 0) == pdTRUE)
    {
        s_state.fuelLevel      = fuel;
        s_state.digitalPins    = dig;
        s_state.last_update_ms = now_ms();
        xSemaphoreGive(s_mutex);
    }

    ESP_LOGI(TAG, "PGN_EXHAUST_DIG: fuel=%d dig=0x%04X", fuel, dig);
}

static void cb_pressures(uint16_t pgn, uint8_t src,
                         const uint8_t *payload, uint8_t len, void *ctx)
{
    if (len < (PRESSURE_BATTERY_OFFSET + 2)) return;

    int16_t battery = vcan_read_s16_le(&payload[PRESSURE_BATTERY_OFFSET]);

    if (xSemaphoreTake(s_mutex, 0) == pdTRUE)
    {
        s_state.batteryLevel   = battery;
        s_state.last_update_ms = now_ms();
        xSemaphoreGive(s_mutex);
    }

    ESP_LOGI(TAG, "PGN_PRESSURES: battery=%d", battery);
}

static void cb_temps(uint16_t pgn, uint8_t src,
                     const uint8_t *payload, uint8_t len, void *ctx)
{
    if (len < (TEMP_AMBIENT_OFFSET + 2)) return;

    int16_t ambient = vcan_read_s16_le(&payload[TEMP_AMBIENT_OFFSET]);

    if (xSemaphoreTake(s_mutex, 0) == pdTRUE)
    {
        s_state.ambientTemp    = ambient;
        s_state.last_update_ms = now_ms();
        xSemaphoreGive(s_mutex);
    }

    ESP_LOGI(TAG, "PGN_TEMPS: ambient=%d", ambient);
}

static void cb_gps_status(uint16_t pgn, uint8_t src,
                          const uint8_t *payload, uint8_t len, void *ctx)
{
    if (len < (GPS_STATUS_COMPASS_OFFSET + 4)) return;

    if (xSemaphoreTake(s_mutex, 0) == pdTRUE)
    {
        memcpy(s_state.compass8, &payload[GPS_STATUS_COMPASS_OFFSET], 4);
        s_state.compass8[4]     = '\0';
        s_state.last_update_ms  = now_ms();
        xSemaphoreGive(s_mutex);
    }

    ESP_LOGI(TAG, "PGN_GPS_STATUS: compass=%s", s_state.compass8);
}

static void cb_gnss_time(uint16_t pgn, uint8_t src,
                         const uint8_t *payload, uint8_t len, void *ctx)
{
    if (len < (GNSS_MINUTE_OFFSET + 1)) return;

    if (xSemaphoreTake(s_mutex, 0) == pdTRUE)
    {
        s_state.hour           = payload[GNSS_HOUR_OFFSET];
        s_state.minute         = payload[GNSS_MINUTE_OFFSET];
        s_state.last_update_ms = now_ms();
        xSemaphoreGive(s_mutex);
    }

    ESP_LOGI(TAG, "PGN_GNSS_TIME: hour=%u minute=%u", s_state.hour, s_state.minute);
}

/* --- Init / Deinit --- */

void small_gauge_init(void)
{
    if (!s_mutex)
        s_mutex = xSemaphoreCreateMutex();

    memset(&s_state, 0, sizeof(s_state));
    memset(&s_last_pkt, 0, sizeof(s_last_pkt));

    vcan_receiver_register_pgn(PGN_ENGINE_CORE, cb_engine_core, NULL);
    vcan_receiver_register_pgn(PGN_HEARTBEAT,   cb_heartbeat,   NULL);
    vcan_receiver_register_pgn(PGN_EXHAUST_DIG, cb_exhaust_dig, NULL);
    vcan_receiver_register_pgn(PGN_PRESSURES,   cb_pressures,   NULL);
    vcan_receiver_register_pgn(PGN_TEMPS,       cb_temps,       NULL);
    vcan_receiver_register_pgn(PGN_GPS_STATUS,  cb_gps_status,  NULL);
    vcan_receiver_register_pgn(PGN_GNSS_TIME,   cb_gnss_time,   NULL);

    // Register temporary debug decoder for GPS-related PGNs (use PGN_GPS_POS)
    vcan_receiver_register_pgn(PGN_GPS_POS,     debug_gps_decode, NULL);
    vcan_receiver_register_pgn(PGN_GPS_STATUS,  debug_gps_decode, NULL);
    vcan_receiver_register_pgn(PGN_GNSS_TIME,   debug_gps_decode, NULL);

    ESP_LOGI(TAG, "small gauge initialized");
}

void small_gauge_deinit(void)
{
    vcan_receiver_unregister_pgn(PGN_ENGINE_CORE, cb_engine_core, NULL);
    vcan_receiver_unregister_pgn(PGN_HEARTBEAT,   cb_heartbeat,   NULL);
    vcan_receiver_unregister_pgn(PGN_EXHAUST_DIG, cb_exhaust_dig, NULL);
    vcan_receiver_unregister_pgn(PGN_PRESSURES,   cb_pressures,   NULL);
    vcan_receiver_unregister_pgn(PGN_TEMPS,       cb_temps,       NULL);
    vcan_receiver_unregister_pgn(PGN_GPS_STATUS,  cb_gps_status,  NULL);
    vcan_receiver_unregister_pgn(PGN_GNSS_TIME,   cb_gnss_time,   NULL);

    // Unregister debug callbacks
    vcan_receiver_unregister_pgn(PGN_GPS_POS,     debug_gps_decode, NULL);
    vcan_receiver_unregister_pgn(PGN_GPS_STATUS,  debug_gps_decode, NULL);
    vcan_receiver_unregister_pgn(PGN_GNSS_TIME,   debug_gps_decode, NULL);

    if (s_mutex)
    {
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
    }
}

/* --- Build UI-ready packet --- */

void small_gauge_draw(void)
{
    sg_state_t local;

    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE)
        return;

    local = s_state;
    xSemaphoreGive(s_mutex);

    GaugePacket pkt;
    memset(&pkt, 0, sizeof(pkt));

    /* Digital-pin indicators */
    pkt.ind_water_in_fuel = (local.digitalPins & VCAN_BIT(DIG_WATER_FUEL)) != 0;
    pkt.ind_low_washer    = (local.digitalPins & VCAN_BIT(DIG_LOW_WASHER)) != 0;

    /* Threshold indicators
       Note: raw units are assumed to match sender conventions:
       - fuelLevel: 0..10000 => 0.00..100.00%
       - batteryLevel: 0..1000 => 0.00..10.00V
       - coolantTemp: raw degrees (e.g., Fahrenheit)
    */
    pkt.ind_low_fuel      = (local.fuelLevel    < 1250);   // < 12.50%
    pkt.ind_low_battery   = (local.batteryLevel < 1000);   // < 10.00V
    pkt.ind_engine_temp   = (local.coolantTemp  > 235 * INT_SCALING);    // > 240°F

    /* Info screen */
    pkt.ambientTemp = local.ambientTemp;

    strncpy(pkt.compass8, local.compass8, sizeof(pkt.compass8) - 1);
    pkt.compass8[sizeof(pkt.compass8) - 1] = '\0';

    pkt.hour   = local.hour;
    pkt.minute = local.minute;

    /* Optional raw digital pins */
    pkt.digitalPins = local.digitalPins;

    s_last_pkt = pkt;

    ESP_LOGI(TAG,
        "DRAW: WIF=%d washer=%d lowFuel=%d lowBatt=%d engTemp=%d ambient=%d compass=%s time=%02u:%02u dig=0x%04X",
        pkt.ind_water_in_fuel,
        pkt.ind_low_washer,
        pkt.ind_low_fuel,
        pkt.ind_low_battery,
        pkt.ind_engine_temp,
        pkt.ambientTemp,
        pkt.compass8,
        pkt.hour,
        pkt.minute,
        pkt.digitalPins
    );
}
