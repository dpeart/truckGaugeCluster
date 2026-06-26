#include "Gaugepacket.h"
#include "esp_log.h"
#include "p4_telemetry.h"
#include "p4_modes.h"

void handle_gauge_packet(const GaugePacket *pkt)
{
    // Auto-exit only for C6-side modes
    // if (current_mode == p4_mode_t::C6_OTA ||
    //     current_mode == p4_mode_t::C6_FACTORY_RESET ||
    //     current_mode == p4_mode_t::C6_REBOOT)
    // {
    //     ESP_LOGW("P4_MODE", "Telemetry resumed — leaving C6-side mode");
    //     current_mode = p4_mode_t::TELEMETRY;
    // }

    ESP_LOGI("P4_TELEM",
             "spd=%d rpm=%d gear=%d oilP=%d fuelP=%d boost=%d EGT=%d "
             "coolant=%d trans=%d amb=%d ia=%d "
             "accel=[%d,%d,%d] pins=0x%04X cruise=%u set=%u "
             "date=%04u-%02u-%02u time=%02u:%02u:%02u "
             "heading=%d compass=%s",
             pkt->speed,
             pkt->rpm,
             pkt->gearPosition,
             pkt->oilPressure,
             pkt->fuelPressure,
             pkt->boostPressure,
             pkt->EGTemp,
             pkt->coolantTemp,
             pkt->transTemp,
             pkt->ambientTemp,
             pkt->iaTemp,
             pkt->accelerationX,
             pkt->accelerationY,
             pkt->accelerationZ,
             pkt->digitalPins,
             pkt->cruiseActive,
             pkt->cruiseSetValue,
             pkt->year,
             pkt->month,
             pkt->day,
             pkt->hour,
             pkt->minute,
             pkt->second,
             pkt->headingDeg,
             pkt->compass8);
}
