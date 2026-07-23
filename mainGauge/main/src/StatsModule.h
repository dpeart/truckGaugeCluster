#pragma once
#include <stdint.h>
#include <vector>
#include "GaugePacket.h"
#include "lvgl.h"

class StatsModule
{
public:
    StatsModule();

    void start(); // User pressed stats_start
    void stop();  // Reset module back to IDLE
    void update(const GaugePacket &pkt, uint32_t nowMs);

    bool isRunning() const { return state != IDLE && state != DONE; }
    bool isDone() const { return state == DONE; }
    bool hasStopped = false;

    uint32_t totalRunMs = 0;

    // History buffer for dynamic X-axis compression (~100 seconds at 100ms ticks)
    static const int MAX_RUN_SAMPLES = 1000;
    float speedHistory[MAX_RUN_SAMPLES];
    float distanceHistory[MAX_RUN_SAMPLES];
    int historyCount = 0;

    // Final results
    float getZeroToSixtyTimeMs() const { return zeroToSixtyMs; }
    float getQuarterMileTrapSpeed() const { return quarterMileTrapSpeed; }
    float getQuarterMileMs() const { return quarterMileMs; }

    uint32_t startOdometerTenths = 0;
    float accumulatedDistanceMeters = 0.0f;
    uint32_t lastTickMs = 0;

    // Charts
    const std::vector<float> &getSpeedChart() const { return speedChart; }
    const std::vector<uint32_t> &getDistanceChart() const { return distanceChart; }

    // LVGL Integration
    void lvglInit();
    void lvglUpdateCharts(); // Updated: Takes no arguments

private:
    enum State
    {
        IDLE,
        ARMED, // Waiting for speed > 0
        CAPTURING,
        DONE
    };

    State state;

    // Timer & Execution state flags
    bool running = false;
    lv_timer_t *stats_timer = nullptr;

    // Timing
    uint32_t startMs;
    uint32_t lastUpdateMs;

    // Distance accumulator
    float distanceMeters;
    float nextSampleDistanceMeters;

    // Final results
    float zeroToSixtyMs;
    float quarterMileTrapSpeed;
    float quarterMileMs; // Added: Stores total ET for quarter mile

    // Charts
    std::vector<float> speedChart;       // mph at each 50 ft
    std::vector<uint32_t> distanceChart; // elapsed ms at each 50 ft

    // LVGL LINE INTEGRATION BUFFERS
    static const int LVGL_POINT_COUNT = 60;

    int speedBuf[LVGL_POINT_COUNT];
    int distanceBuf[LVGL_POINT_COUNT];
    int currentMaxSpeed = 1;
    int currentMaxDistance = 1;

    lv_point_t speedPoints[LVGL_POINT_COUNT];
    lv_point_t distancePoints[LVGL_POINT_COUNT];

    int writeIndex;
    int xStepPixels;

    // Private helper methods
    float gpsSpeedMps(const GaugePacket &pkt);
    void pushPoint(float speed, float distanceMeters);
    void updateAxisScaling();
    void processTelemetryTick(); // Called periodically by stats_timer
};