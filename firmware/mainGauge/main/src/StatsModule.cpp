#include "StatsModule.h"
#include "screens.h" // EEZ Studio generated objects
#include "lvgl.h"
#include "esp_log.h"
#include <cmath>
#include "p4_modes.h" // for p4_get_mode()

static const char *TAG_STATS = "StatsModule";

// ---------------------------------------------------------------------
// X-Axis Dynamic Time Label Callback
// ---------------------------------------------------------------------
static void stats_chart_draw_cb(lv_event_t *e)
{
    lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(e);
    if (!dsc) return;

    // Intercept primary X axis tick drawing
    if (lv_obj_draw_part_check_type(dsc, &lv_chart_class, LV_CHART_DRAW_PART_TICK_LABEL))
    {
        if (dsc->id == LV_CHART_AXIS_PRIMARY_X && dsc->text != NULL)
        {
            // dsc->value is ALREADY the exact X-axis scale value configured by lv_chart_set_range!
            // E.g., if X-axis range is 0..11, dsc->value will be 0, 2, 5, 8, 11, etc.
            int val = dsc->value;

            // Handle decimal values if dsc->value is passed as scaled fixed-point
            if (val < 0) val = 0;

            // Format tick directly (e.g. "0s", "2.5s", "10s" or "10.2s")
            // If dsc->value is integer seconds:
            snprintf(dsc->text, dsc->text_length, "%ds", val);
        }
    }
}

StatsModule::StatsModule()
{
    state = IDLE;
    startMs = 0;
    lastUpdateMs = 0;
    distanceMeters = 0.0f;
    nextSampleDistanceMeters = 50.0f * 0.3048f; // 50 ft in meters

    zeroToSixtyMs = 0.0f;
    quarterMileTrapSpeed = 0.0f;
    quarterMileMs = 0.0f;
    totalRunMs = 0;

    speedChart.clear();
    distanceChart.clear();

    writeIndex = 0;
    xStepPixels = 0;

    currentMaxSpeed = 1;
    currentMaxDistance = 1;

    for (int i = 0; i < LVGL_POINT_COUNT; ++i)
    {
        speedBuf[i] = 0;
        distanceBuf[i] = 0;
        speedPoints[i].x = 0;
        speedPoints[i].y = 0;
        distancePoints[i].x = 0;
        distancePoints[i].y = 0;
    }
}

static inline lv_coord_t value_to_y_pixel(float value, float maxValue, int heightPx)
{
    if (maxValue <= 0.0001f)
        return 0;
    if (value < 0.0f)
        value = 0.0f;
    if (value > maxValue)
        value = maxValue;

    float scaled = (value / maxValue) * (heightPx * 0.9f);
    return (lv_coord_t)std::roundf(scaled);
}

void StatsModule::lvglInit()
{
    if (objects.stats_chart == nullptr)
    {
        ESP_LOGW(TAG_STATS, "lvglInit: stats_chart is missing!");
        return;
    }

    // 1. Force Scatter mode for dynamic scaling
    lv_chart_set_type(objects.stats_chart, LV_CHART_TYPE_SCATTER);

    // 2. Setup background grid (4 horizontal, 5 vertical divisions)
    lv_chart_set_div_line_count(objects.stats_chart, 4, 5);

    // 3. Grid line styling
    lv_obj_set_style_line_color(objects.stats_chart, lv_color_hex(0x666666), LV_PART_MAIN);
    lv_obj_set_style_line_opa(objects.stats_chart, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_line_width(objects.stats_chart, 2, LV_PART_MAIN);

    // 4. Add padding so axis label text isn't clipped
    lv_obj_set_style_pad_left(objects.stats_chart, 40, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(objects.stats_chart, 25, LV_PART_MAIN);

    // 5. Configure Axis Ticks (5 major ticks on X and Y, 2 minor ticks)
    lv_chart_set_axis_tick(objects.stats_chart, LV_CHART_AXIS_PRIMARY_Y, 10, 5, 5, 2, true, 40);
    lv_chart_set_axis_tick(objects.stats_chart, LV_CHART_AXIS_PRIMARY_X, 10, 5, 5, 2, true, 25);

    // 6. Register draw event callback to format X-axis time labels on the fly
    lv_obj_add_event_cb(objects.stats_chart, stats_chart_draw_cb, LV_EVENT_DRAW_PART_BEGIN, this);

    ESP_LOGI(TAG_STATS, "lvglInit: setting up line buffers");

    if (objects.speed_chart == nullptr || objects.distance_chart == nullptr)
    {
        ESP_LOGW(TAG_STATS, "lvglInit: speed_chart or distance_chart object missing!");
        return;
    }

    int w_speed = lv_obj_get_width(objects.speed_chart);
    int h_speed = lv_obj_get_height(objects.speed_chart);
    if (w_speed <= 0) w_speed = 1;
    if (h_speed <= 0) h_speed = 1;

    xStepPixels = w_speed / (LVGL_POINT_COUNT > 1 ? (LVGL_POINT_COUNT - 1) : 1);

    for (int i = 0; i < LVGL_POINT_COUNT; ++i)
    {
        speedPoints[i].x = i * xStepPixels;
        speedPoints[i].y = 0;
        distancePoints[i].x = i * xStepPixels;
        distancePoints[i].y = 0;
    }

    lv_line_set_points(objects.speed_chart, speedPoints, LVGL_POINT_COUNT);
    lv_line_set_y_invert(objects.speed_chart, true);
    lv_obj_set_style_line_width(objects.speed_chart, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_line_color(objects.speed_chart, lv_color_hex(0xff0019ff), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_line_set_points(objects.distance_chart, distancePoints, LVGL_POINT_COUNT);
    lv_line_set_y_invert(objects.distance_chart, true);
    lv_obj_set_style_line_width(objects.distance_chart, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_line_color(objects.distance_chart, lv_color_hex(0xff9134b9), LV_PART_MAIN | LV_STATE_DEFAULT);

    for (int i = 0; i < LVGL_POINT_COUNT; ++i)
    {
        speedBuf[i] = 0;
        distanceBuf[i] = 0;
    }

    writeIndex = 0;
}

void StatsModule::start()
{
    ESP_LOGI(TAG_STATS, "start() called - Transitioning state -> ARMED");

    state = ARMED;
    running = true;
    hasStopped = false;

    writeIndex = 0;
    currentMaxSpeed = 1;
    currentMaxDistance = 1;

    historyCount = 0;
    zeroToSixtyMs = 0.0f;
    quarterMileTrapSpeed = 0.0f;
    quarterMileMs = 0.0f;
    accumulatedDistanceMeters = 0.0f;
    totalRunMs = 0;
    lastTickMs = 0;

    // Clear UI text labels for fresh run
    if (objects.zero_to_sixty_time)
        lv_label_set_text(objects.zero_to_sixty_time, "--.-- s");
    if (objects.quartermiletime)
        lv_label_set_text(objects.quartermiletime, "--.-- s");
    if (objects.quartermilespeed)
        lv_label_set_text(objects.quartermilespeed, "---.- mph");

    for (int i = 0; i < LVGL_POINT_COUNT; ++i)
    {
        speedBuf[i] = 0;
        distanceBuf[i] = 0;
        speedPoints[i].x = 0;
        speedPoints[i].y = 0;
        distancePoints[i].x = 0;
        distancePoints[i].y = 0;
    }

    // Reset line points on screen
    if (objects.speed_chart)
        lv_line_set_points(objects.speed_chart, speedPoints, 0);
    if (objects.distance_chart)
        lv_line_set_points(objects.distance_chart, distancePoints, 0);

    if (!stats_timer)
    {
        stats_timer = lv_timer_create(
            [](lv_timer_t *t)
            {
                StatsModule *self = static_cast<StatsModule *>(t->user_data);
                if (!self->isRunning())
                    return;
                self->processTelemetryTick();
            },
            100,
            this);
    }
}

void StatsModule::stop()
{
    ESP_LOGI(TAG_STATS, "stop() called - Resetting state -> IDLE");
    state = IDLE;
    running = false;

    if (stats_timer != nullptr)
    {
        ESP_LOGI(TAG_STATS, "Deleting stats_timer");
        lv_timer_del(stats_timer);
        stats_timer = nullptr;
    }
}

void StatsModule::processTelemetryTick()
{
    // 1. Fetch live telemetry packet
    GaugePacket pkt;
    gauge_state_get(pkt);

    float speed_mph = pkt.speed;

    // Get precise hardware time in milliseconds
    uint32_t now_ms = static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);

    // Calculate time delta in seconds since last tick
    float dt_sec = (lastTickMs > 0) ? (now_ms - lastTickMs) / 1000.0f : 0.1f;
    lastTickMs = now_ms;

    // 2. State machine logic
    switch (state)
    {
    case ARMED:
        // Mark vehicle as stopped once speed drops below threshold
        if (speed_mph <= 0.5f)
        {
            if (!hasStopped)
            {
                ESP_LOGI(TAG_STATS, "Vehicle stopped. Ready for launch!");
                hasStopped = true;
            }
        }

        // Only launch on a rising edge (was stopped, now moving > 0.5 mph)
        if (hasStopped && speed_mph > 0.5f)
        {
            ESP_LOGI(TAG_STATS, "LAUNCH DETECTED! (speed=%.2f mph) Transitioning ARMED -> CAPTURING", speed_mph);

            state = CAPTURING;
            startMs = now_ms;

            accumulatedDistanceMeters = 0.0f;
            zeroToSixtyMs = 0.0f;
            quarterMileTrapSpeed = 0.0f;
            quarterMileMs = 0.0f;
            totalRunMs = 0;
        }
        break;

    case CAPTURING:
    {
        // Smooth high-resolution distance calculation via speed integration
        float speed_mps = speed_mph * 0.44704f;
        accumulatedDistanceMeters += speed_mps * dt_sec;
        float distance_m = accumulatedDistanceMeters;

        uint32_t elapsedMs = now_ms - startMs;
        totalRunMs = elapsedMs; // Store precise wall-clock duration

        // -------------------------------------------------------------
        // FAIL SAFE 1: Vehicle Stopped Mid-Run (Aborted)
        // -------------------------------------------------------------
        if (elapsedMs > 2000 && speed_mph <= 0.5f)
        {
            ESP_LOGW(TAG_STATS, "RUN ABORTED: Vehicle stopped before 1/4 mile mark.");
            state = DONE;

            if (objects.quartermiletime)
                lv_label_set_text(objects.quartermiletime, "ABORT");
            if (objects.quartermilespeed)
                lv_label_set_text(objects.quartermilespeed, "DNF");
            break;
        }

        // -------------------------------------------------------------
        // FAIL SAFE 2: RAM Buffer Full or Timeout reached (60s max run)
        // -------------------------------------------------------------
        if (historyCount >= MAX_RUN_SAMPLES || elapsedMs >= 60000)
        {
            ESP_LOGW(TAG_STATS, "RUN TIMEOUT / BUFFER FULL: Ending capture automatically.");
            state = DONE;

            if (objects.quartermiletime)
                lv_label_set_text(objects.quartermiletime, "TIMEOUT");
            if (objects.quartermilespeed)
                lv_label_set_text(objects.quartermilespeed, "DNF");
            break;
        }

        // -------------------------------------------------------------
        // SAFE RECORDING: Bounds check passed, record sample
        // -------------------------------------------------------------
        speedHistory[historyCount] = speed_mph;
        distanceHistory[historyCount] = distance_m;
        historyCount++;

        // Update live graph compression
        lvglUpdateCharts();

        // -------------------------------------------------------------
        // SUCCESS CONDITIONS: 0-60 & 1/4 Mile Checks
        // -------------------------------------------------------------
        if (speed_mph >= 60.0f && zeroToSixtyMs == 0.0f)
        {
            zeroToSixtyMs = static_cast<float>(elapsedMs);
            ESP_LOGI(TAG_STATS, "0-60 MPH HIT! Time: %.2f sec", zeroToSixtyMs / 1000.0f);

            char buf[32];
            snprintf(buf, sizeof(buf), "%.2f s", zeroToSixtyMs / 1000.0f);
            if (objects.zero_to_sixty_time)
                lv_label_set_text(objects.zero_to_sixty_time, buf);
        }

        if (distance_m >= 402.336f)
        {
            quarterMileTrapSpeed = speed_mph;
            quarterMileMs = static_cast<float>(elapsedMs);

            ESP_LOGI(TAG_STATS, "1/4 MILE COMPLETE! ET: %.2f s | Trap: %.1f mph",
                     quarterMileMs / 1000.0f, quarterMileTrapSpeed);

            state = DONE;

            char buf[32];
            // 1/4 Mile ET Label
            snprintf(buf, sizeof(buf), "%.2f s", quarterMileMs / 1000.0f);
            if (objects.quartermiletime)
                lv_label_set_text(objects.quartermiletime, buf);

            // 1/4 Mile Trap Speed Label
            snprintf(buf, sizeof(buf), "%.1f mph", quarterMileTrapSpeed);
            if (objects.quartermilespeed)
                lv_label_set_text(objects.quartermilespeed, buf);
        }
        break;
    }

    case DONE:
    case IDLE:
    default:
        break;
    }
}

void StatsModule::pushPoint(float speed, float distance)
{
    int newSpeed = static_cast<int>(std::roundf(speed));
    int newDistance = static_cast<int>(std::roundf(distance));

    int oldSpeed = speedBuf[writeIndex];
    int oldDistance = distanceBuf[writeIndex];

    speedBuf[writeIndex] = newSpeed;
    distanceBuf[writeIndex] = newDistance;

    if (newSpeed > currentMaxSpeed)
    {
        currentMaxSpeed = newSpeed;
    }
    else if (oldSpeed == currentMaxSpeed)
    {
        int m = 1;
        for (int i = 0; i < LVGL_POINT_COUNT; ++i)
        {
            if (speedBuf[i] > m)
                m = speedBuf[i];
        }
        currentMaxSpeed = (m > 0) ? m : 1;
    }

    if (newDistance > currentMaxDistance)
    {
        currentMaxDistance = newDistance;
    }
    else if (oldDistance == currentMaxDistance)
    {
        int m = 1;
        for (int i = 0; i < LVGL_POINT_COUNT; ++i)
        {
            if (distanceBuf[i] > m)
                m = distanceBuf[i];
        }
        currentMaxDistance = (m > 0) ? m : 1;
    }

    writeIndex++;
    if (writeIndex >= LVGL_POINT_COUNT)
        writeIndex = 0;
}

void StatsModule::updateAxisScaling()
{
}

void StatsModule::lvglUpdateCharts()
{
    if (objects.speed_chart == nullptr || objects.distance_chart == nullptr || historyCount == 0)
    {
        return;
    }

    // 1. Calculate running max for dynamic Y-axis scaling
    float maxSpd = 1.0f;
    float maxDst = 1.0f;
    for (int i = 0; i < historyCount; ++i)
    {
        if (speedHistory[i] > maxSpd)
            maxSpd = speedHistory[i];
        if (distanceHistory[i] > maxDst)
            maxDst = distanceHistory[i];
    }

    float speedMax = maxSpd * 1.1f; // ~90% fill
    float distMax = maxDst * 1.1f;

    // 2. Get line widget display dimensions
    int w_speed = lv_obj_get_width(objects.speed_chart);
    int h_speed = lv_obj_get_height(objects.speed_chart);
    if (w_speed <= 0) w_speed = 1;
    if (h_speed <= 0) h_speed = 1;

    int w_dist = lv_obj_get_width(objects.distance_chart);
    int h_dist = lv_obj_get_height(objects.distance_chart);
    if (w_dist <= 0) w_dist = 1;
    if (h_dist <= 0) h_dist = 1;

    // 3. Determine how many points to plot
    int pointsToPlot = (historyCount < LVGL_POINT_COUNT) ? historyCount : LVGL_POINT_COUNT;

    for (int i = 0; i < pointsToPlot; ++i)
    {
        int x_spd = (pointsToPlot > 1) ? (i * w_speed / (pointsToPlot - 1)) : 0;
        int x_dst = (pointsToPlot > 1) ? (i * w_dist / (pointsToPlot - 1)) : 0;

        int srcIdx;
        if (historyCount <= LVGL_POINT_COUNT)
        {
            srcIdx = i;
        }
        else
        {
            srcIdx = (i * (historyCount - 1)) / (LVGL_POINT_COUNT - 1);
        }

        speedPoints[i].x = x_spd;
        speedPoints[i].y = value_to_y_pixel(speedHistory[srcIdx], speedMax, h_speed);

        distancePoints[i].x = x_dst;
        distancePoints[i].y = value_to_y_pixel(distanceHistory[srcIdx], distMax, h_dist);
    }

    // 4. Update line objects
    lv_line_set_points(objects.speed_chart, speedPoints, pointsToPlot);
    lv_line_set_points(objects.distance_chart, distancePoints, pointsToPlot);

    // 5. Update parent lv_chart Axis Ranges
    if (objects.stats_chart != nullptr)
    {
        // Y-Axis Range
        lv_chart_set_range(
            objects.stats_chart,
            LV_CHART_AXIS_PRIMARY_Y,
            0,
            static_cast<lv_coord_t>(std::ceil(speedMax)));

        // X-Axis Range: Exact hardware duration in seconds
        float totalSeconds = totalRunMs / 1000.0f;
        if (totalSeconds < 0.1f) totalSeconds = 0.1f;

        lv_chart_set_range(
            objects.stats_chart,
            LV_CHART_AXIS_PRIMARY_X,
            0,
            static_cast<lv_coord_t>(std::ceil(totalSeconds)));

        // Trigger redrawing of grid lines & tick text
        lv_obj_invalidate(objects.stats_chart);
    }
}