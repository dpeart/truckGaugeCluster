#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>

// ---------------------------------------------------------
// External globals (from Globals.h)
// ---------------------------------------------------------
extern volatile float g_speedMph;
extern volatile float g_rpm;
extern volatile uint32_t g_odometerTenths;

extern bool DEBUG_SIMULATION_MODE;

// ---------------------------------------------------------
// Simulation helpers
// ---------------------------------------------------------
void simulateRPM();
void simulateVSS();

// ---------------------------------------------------------
// FreeRTOS tasks
// ---------------------------------------------------------
void vssTask(void *pvParameters);
void rpmTask(void *pvParameters);

// ---------------------------------------------------------
// Initialization
// ---------------------------------------------------------
void initInterruptHandlers();
