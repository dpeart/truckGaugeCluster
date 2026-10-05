#ifndef VCAN_SENDER_H
#define VCAN_SENDER_H

#include "esp_err.h"
#include "vcan_protocol.h"
#include <mutex>
#include <cstdint>

namespace vcan
{

    class Sender
    {
    public:
        static Sender &instance();

        esp_err_t init();

        esp_err_t sendRawPayload(uint16_t pgn, uint8_t src_addr, uint8_t priority, const void *payload, uint8_t len);

        // Overloads using clean pgn_*_t types
        // Inside vcan_sender.h in class Sender:

        esp_err_t send(const pgn_heartbeat_t &msg) { return sendRawPayload(PGN_HEARTBEAT, 0x00, CAN_PRIORITY_HIGH, &msg, sizeof(msg)); }
        esp_err_t send(const pgn_engine_core_t &msg) { return sendRawPayload(PGN_ENGINE_CORE, 0x00, CAN_PRIORITY_HIGH, &msg, sizeof(msg)); }
        esp_err_t send(const pgn_pressures_t &msg) { return sendRawPayload(PGN_PRESSURES, 0x00, CAN_PRIORITY_HIGH, &msg, sizeof(msg)); }
        esp_err_t send(const pgn_temps_t &msg) { return sendRawPayload(PGN_TEMPS, 0x00, CAN_PRIORITY_MED, &msg, sizeof(msg)); }
        esp_err_t send(const pgn_exhaust_dig_t &msg) { return sendRawPayload(PGN_EXHAUST_DIG, 0x00, CAN_PRIORITY_MED, &msg, sizeof(msg)); }
        esp_err_t send(const pgn_cruise_odo_t &msg) { return sendRawPayload(PGN_CRUISE_ODO, 0x00, CAN_PRIORITY_LOW, &msg, sizeof(msg)); }
        esp_err_t send(const pgn_gps_pos_t &msg) { return sendRawPayload(PGN_GPS_POS, 0x00, CAN_PRIORITY_MED, &msg, sizeof(msg)); }

        // Add this missing line:
        esp_err_t send(const pgn_gps_motion_t &msg) { return sendRawPayload(PGN_GPS_MOTION, 0x00, CAN_PRIORITY_MED, &msg, sizeof(msg)); }

        esp_err_t send(const pgn_gps_status_t &msg) { return sendRawPayload(PGN_GPS_STATUS, 0x00, CAN_PRIORITY_LOW, &msg, sizeof(msg)); }
        esp_err_t send(const pgn_gnss_time_t &msg) { return sendRawPayload(PGN_GNSS_TIME, 0x00, CAN_PRIORITY_HIGH, &msg, sizeof(msg)); }

    private:
        Sender() = default;
        ~Sender() = default;

        Sender(const Sender &) = delete;
        Sender &operator=(const Sender &) = delete;

        std::mutex mutex_;
        bool initialized_ = false;
    };

} // namespace vcan

#endif // VCAN_SENDER_H