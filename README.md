# truckGaugeCluster
ESP‑IDF + LVGL v8.4 Automotive Gauge Cluster powered by ESP‑NOW, EEZ Studio 0.26.0, and Sequent Microsystems DAQ hardware
This project implements a complete digital automotive gauge cluster using:

LVGL v8.4 for high‑performance embedded UI

EEZ Studio 0.26.0 for screen design and code generation

Sequent Microsystems ESP32‑Pi as the main DAQ + broadcast controller

Multiple Sequent Microsystems I²C DAQ boards for sensor acquisition

Six ESP32‑based displays receiving real‑time data via ESP‑NOW

A modular architecture supporting multiple gauge layouts (main gauge + small gauges)

The system reads vehicle sensor data, processes it on the ESP32‑Pi, and distributes it wirelessly to multiple displays that each render their own LVGL‑based gauge UI.

🚗 Project Overview
Main Features
Real‑time gauge rendering using LVGL v8.4

EEZ Studio‑generated screens for consistent, maintainable UI

ESP‑NOW broadcast from a central DAQ node

Six independent ESP32 display nodes

Modular gauge layouts (speed, tach, temps, pressures, indicators)

Custom condensed monospaced fonts for stable numeric rendering

Clean, deterministic ESP‑IDF project structure

🧰 Sequent Microsystems DAQ Hardware (I²C)
The DAQ node uses multiple Sequent Microsystems expansion boards connected over I²C to gather all vehicle sensor data.
These boards provide a robust, modular, automotive‑friendly hardware stack.

Boards used:
SM_16UNIVIN — 16‑channel universal analog input

SM_RTD — RTD temperature measurement board

SM_16DIGIN — 16‑channel digital input board

Adafruit FRAM I²C — non‑volatile storage for odometer/trip

Adafruit MCP9601 — thermocouple amplifier

Adafruit MPU6050 — accelerometer + gyro (for motion/tilt sensing)

These devices are accessed through standard I²C interfaces and integrated into the DAQ firmware running on the Sequent Microsystems ESP32‑Pi.

📡 System Architecture
1. DAQ / Broadcast Node (ESP32‑Pi)
Reads all vehicle sensor data via I²C

Packages values into a compact broadcast frame

Sends updates at fixed intervals via ESP‑NOW

No pairing required — all displays listen passively

2. Display Nodes (6 total)
Each display runs its own LVGL UI and receives the broadcast packet:

Speed gauge

Tachometer

Temperature gauges

Pressure gauges

Indicator lights

Odometer / trip display

Each node updates only the elements relevant to its screen.

🎨 UI / Gauge Design
All screens are built in EEZ Studio 0.26.0, exported to LVGL 8.4, and integrated into ESP‑IDF.

Highlights:
Clean, OEM‑style gauge visuals

Custom monospaced condensed fonts for stable digits

Regeneration‑proof code structure

Modular screen state structs for indicator pointers

High‑FPS rendering with minimal invalidation

Starting config for the Waveshare boards from here:
  https://github.com/traviscea/center-cluster-esp32-p4
