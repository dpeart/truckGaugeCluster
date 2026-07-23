#ifndef INTERRUPT_HANDLERS_H
#define INTERRUPT_HANDLERS_H

#include <Arduino.h>

// Public globals updated by tasks
extern volatile float g_speedMph;
extern volatile float g_rpm;
extern volatile uint32_t g_odometerTenths;

// Setup function to initialize tasks + interrupts
void initInterruptHandlers();

// ISR functions (declared so DAQ.ino can attachInterrupt)
void IRAM_ATTR vss_isr();
void IRAM_ATTR rpm_isr();

extern bool DEBUG_SIMULATION_MODE;

#endif
