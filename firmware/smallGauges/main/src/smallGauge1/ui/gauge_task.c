#include "src/ui/ui.h"
#include "src/updateUI.h"
#include "src/GaugePacket.h"

void gauge_task_impl(const GaugePacket *pkt)
{
    static float coolant_lerp = 0.0f;
    static float oil_lerp     = 0.0f;
    static float fuel_lerp    = 0.0f;

    coolant_lerp = lerp(coolant_lerp, (float)pkt->coolantTemp, 0.15f);
    oil_lerp     = lerp(oil_lerp,     (float)pkt->oilPressure, 0.15f);
    fuel_lerp    = lerp(fuel_lerp,    (float)pkt->fuelLevel,   0.15f);

    update_coolant_meter((int32_t)coolant_lerp);
    update_oil_pressure_meter((int32_t)oil_lerp);
    update_fuel_arc((int32_t)fuel_lerp);
}
