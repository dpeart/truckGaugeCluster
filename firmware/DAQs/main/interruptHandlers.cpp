#include "interruptHandlers.h"
#include <Arduino.h>
#include "Globals.h"

extern bool DEBUG_SIMULATION_MODE;

// Use your actual DAQ pin names
#define VSS_PIN PWM_SPEED
#define RPM_PIN PWM_TACH

// Critical section locks
portMUX_TYPE vssMux = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE rpmMux = portMUX_INITIALIZER_UNLOCKED;

// Tire + VSS constants
const float wheelDiameterInches = 32.5f;
static const float vssPulsesPerRevolution = 48.0f;

// RPM constants
static const float RPM_PULSES_PER_REV = 2.0f;   // real Cummins crank sensor

// Raw ISR counters
volatile uint32_t vssPulseCount = 0;
volatile uint32_t rpmPulseCount = 0;

// Computed values
volatile float g_speedMph = 0.0f;
volatile float g_rpm = 0.0f;
volatile uint32_t g_odometerTenths = 0;

static float odoAccumTenths = 0.0f;

// ISR timing
volatile uint32_t lastVssTime = 0;
volatile uint32_t vssPeriod = 0;

// ---------------------------------------------------------
// REAL ISR: VSS pulse timing + pulse count
// ---------------------------------------------------------
void IRAM_ATTR vss_isr() {
    uint32_t now = micros();
    portENTER_CRITICAL_ISR(&vssMux);
    vssPeriod = now - lastVssTime;
    lastVssTime = now;
    vssPulseCount++;
    portEXIT_CRITICAL_ISR(&vssMux);
}

// ---------------------------------------------------------
// REAL ISR: RPM pulse period measurement
// ---------------------------------------------------------
volatile uint32_t lastPulseTime = 0;
volatile uint32_t pulseInterval = 0;

void IRAM_ATTR rpmPulseISR() {
    uint32_t now = micros();
    uint32_t dt = now - lastPulseTime;

    // Reject impossible RPM (<5ms → >6000 RPM)
    if (dt < 5000) return;

    portENTER_CRITICAL_ISR(&rpmMux);
    pulseInterval = dt;
    lastPulseTime = now;
    portEXIT_CRITICAL_ISR(&rpmMux);
}

// ---------------------------------------------------------
// SIMULATION: Generate synthetic RPM pulses (virtual ISR)
// ---------------------------------------------------------
// ---------------------------------------------------------
// SIMULATION: Generate synthetic RPM pulses (virtual ISR)
// ---------------------------------------------------------
void simulateRPM() {
    static float theta = 0.0f;
    static float accumulator = 0.0f;

    // Full sine wave cycle every 5 seconds (500 ticks at 10 ms)
    theta += (2.0f * PI) / 500.0f;
    if (theta > 2.0f * PI)
        theta -= 2.0f * PI;

    // Target RPM sweep: 0 → 5000 → 0
    float simRpm = 2500.0f + 2500.0f * sinf(theta);  // center 2500, amplitude 2500

    // Avoid division by zero / insane periods at exact 0 RPM
    if (simRpm < 50.0f) {
        simRpm = 50.0f;
    }

    // Convert target RPM to pulse timing (2 pulses per revolution)
    float secondsPerRev   = 60.0f / simRpm;
    float secondsPerPulse = secondsPerRev / RPM_PULSES_PER_REV;  // RPM_PULSES_PER_REV = 2.0f
    float pulsesPerSec    = 1.0f / secondsPerPulse;

    // rpmTask runs every 10 ms → how many pulses should occur in this tick?
    float pulsesThisTick = pulsesPerSec * 0.010f;
    accumulator += pulsesThisTick;

    uint32_t pulses = (uint32_t)accumulator;
    accumulator -= pulses;

    if (pulses == 0) {
        return;  // no pulse this tick
    }

    // Synthetic pulse interval in microseconds
    uint32_t dt_us = (uint32_t)(secondsPerPulse * 1e6f);
    uint32_t now   = micros();

    // Emulate what the real ISR would have produced
    portENTER_CRITICAL_ISR(&rpmMux);
    pulseInterval = dt_us;
    lastPulseTime = now;
    portEXIT_CRITICAL_ISR(&rpmMux);
}


// ---------------------------------------------------------
// SIMULATION: Generate synthetic VSS pulses (virtual ISR)
// ---------------------------------------------------------
void simulateVSS() {
    static uint32_t t = 0;
    t++;

    const float wheelCircFeet =
        (PI * wheelDiameterInches) / 12.0f;

    const float maxRevsPerSec =
        (140.0f * 5280.0f / 3600.0f) / wheelCircFeet;

    const float maxPulsesPerSec =
        maxRevsPerSec * vssPulsesPerRevolution;

    float norm = 0.5f + 0.5f * sinf(t * 0.0039f);
    float pulsesPerSec = norm * maxPulsesPerSec;

    if (pulsesPerSec < 1.0f)
        pulsesPerSec = 1.0f;

    float pulsesThisTick = pulsesPerSec * 0.010f;
    uint32_t pulses = (uint32_t)pulsesThisTick;
    if (pulses == 0) pulses = 1;

    portENTER_CRITICAL(&vssMux);
    vssPulseCount += pulses;
    vssPeriod = (uint32_t)(1e6f / pulsesPerSec);
    portEXIT_CRITICAL(&vssMux);
}

// ---------------------------------------------------------
// VSS Task (speed + odometer) — UNCHANGED
// ---------------------------------------------------------
void vssTask(void *pvParameters) {

    const float wheelCircFeet =
        (PI * wheelDiameterInches) / 12.0f;
    const float feetPerPulse =
        wheelCircFeet / vssPulsesPerRevolution;

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(10));

        if (DEBUG_SIMULATION_MODE) {
            simulateVSS();
        }

        uint32_t period;
        uint32_t pulses;

        portENTER_CRITICAL(&vssMux);
        period = vssPeriod;
        pulses = vssPulseCount;
        vssPulseCount = 0;
        portEXIT_CRITICAL(&vssMux);

        float speed = 0.0f;
        if (period > 0) {
            float seconds = period / 1e6f;
            float pulsesPerSec = 1.0f / seconds;
            float feetPerSec = pulsesPerSec * feetPerPulse;
            speed = feetPerSec * 3600.0f / 5280.0f;
        }

        portENTER_CRITICAL(&vssMux);
        g_speedMph = speed;
        portEXIT_CRITICAL(&vssMux);

        if (pulses > 0) {
            float totalFeet = pulses * feetPerPulse;
            float miles = totalFeet / 5280.0f;
            float tenths = miles * 10.0f;

            odoAccumTenths += tenths;

            while (odoAccumTenths >= 1.0f) {
                g_odometerTenths++;
                odoAccumTenths -= 1.0f;
            }
        }
    }
}

// ---------------------------------------------------------
// RPM Task — UPDATED (pulse-period RPM)
// ---------------------------------------------------------
void rpmTask(void *pvParameters) {
    const TickType_t period = pdMS_TO_TICKS(10);
    TickType_t lastWake = xTaskGetTickCount();

    for (;;) {
        vTaskDelayUntil(&lastWake, period);

        if (DEBUG_SIMULATION_MODE) {
            simulateRPM();
        }

        uint32_t interval, lastTime;

        portENTER_CRITICAL(&rpmMux);
        interval = pulseInterval;
        lastTime = lastPulseTime;
        portEXIT_CRITICAL(&rpmMux);

        float rpm = 0.0f;

        // Engine stopped timeout
        if (micros() - lastTime > 500000) {
            rpm = 0.0f;
        }
        else if (interval >= 5000) { // same noise threshold as ISR
            float secondsPerPulse = interval / 1e6f;
            float secondsPerRev = secondsPerPulse * RPM_PULSES_PER_REV;
            rpm = 60.0f / secondsPerRev;
        }

        portENTER_CRITICAL(&rpmMux);
        g_rpm = rpm;
        portEXIT_CRITICAL(&rpmMux);
    }
}

// ---------------------------------------------------------
// Initialization
// ---------------------------------------------------------
void initInterruptHandlers() {

    if (!DEBUG_SIMULATION_MODE) {
        attachInterrupt(digitalPinToInterrupt(VSS_PIN), vss_isr, RISING);
        attachInterrupt(digitalPinToInterrupt(RPM_PIN), rpmPulseISR, RISING);
    } else {
        detachInterrupt(digitalPinToInterrupt(VSS_PIN));
        detachInterrupt(digitalPinToInterrupt(RPM_PIN));
    }

    xTaskCreatePinnedToCore(vssTask, "VSSTask", 4096, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(rpmTask, "RPMTask", 4096, NULL, 1, NULL, 1);
}
