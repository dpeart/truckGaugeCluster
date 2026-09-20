#include "GaugePacket.h"
#include "updateUI.h"
#include "GaugeRuntimeState.h"
#include <string.h>
#include <stdio.h>

void gauge_ui_update(const GaugePacket *pkt, GaugeRuntimeState *state)
{
    // Ambient Temp
    if (pkt->ambientTemp != state->last_int1)
    {
        char temp_buf[12];
        snprintf(temp_buf, sizeof(temp_buf), "%d", pkt->ambientTemp);
        text_update_cb(objects.ambient_temp, temp_buf);
        state->last_int1 = pkt->ambientTemp;
    }

    // Compass / Heading
    if (strcmp(pkt->compass8, state->last_str1) != 0)
    {
        text_update_cb(objects.heading, pkt->compass8);
        strncpy(state->last_str1, pkt->compass8, sizeof(state->last_str1) - 1);
    }

    // Time
    char current_time_str[10];
    int display_hour = pkt->hour % 12;
    if (display_hour == 0) display_hour = 12;

    snprintf(current_time_str, sizeof(current_time_str), "%d:%02d",
             display_hour, pkt->minute);

    if (strcmp(current_time_str, state->last_str2) != 0)
    {
        text_update_cb(objects.time, current_time_str);
        strcpy(state->last_str2, current_time_str);
    }

    // Indicators
    updateIndicators(pkt);
}
