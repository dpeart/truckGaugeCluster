#include "src/ui/ui.h"
#include "src/updateUI.h"
#include "src/GaugePacket.h"

void gauge_ui_update(const GaugePacket *pkt, GaugeRuntimeState *state)
{
    // LERP smoothing
    state->lerp1 = lerp(state->lerp1, (float)pkt->oilTemp, 0.15f);
    state->lerp2 = lerp(state->lerp2, (float)pkt->boostPressure, 0.15f);

    // LVGL updates
    update_oil_temp_meter((int32_t)state->lerp1);
    update_boost_pressure_meter((int32_t)state->lerp2);
}
