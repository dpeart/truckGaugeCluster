#include "src/ui/ui.h"
#include "src/updateUI.h"
#include "src/GaugePacket.h"

void gauge_task_impl(const GaugePacket *pkt)
{
    static float iat_lerp     = 0.0f;
    static float egt_lerp     = 0.0f;
    static float battery_lerp = 0.0f;

    iat_lerp     = lerp(iat_lerp,     (float)pkt->iaTemp,       0.15f);
    egt_lerp     = lerp(egt_lerp,     (float)pkt->EGTemp,       0.15f);
    battery_lerp = lerp(battery_lerp, (float)pkt->batteryLevel, 0.15f);

    update_iat_meter((int32_t)iat_lerp);
    update_egt_meter((int32_t)egt_lerp);
    update_battery_arc((int32_t)battery_lerp);
}
