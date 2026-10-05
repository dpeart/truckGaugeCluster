# Truck Gauge Cluster System Architecture & PGN Extension Guide

## Architecture Overview

The system uses a modular telemetry architecture over ESP-NOW/vCAN on ESP-IDF v5.5.4, splitting responsibilities cleanly between data acquisition, CAN packet distribution, receiver state management, and display rendering.

```
+-------------------+      +-----------------------+      +---------------------+
| Hardware / Inputs | ---> |     daq_cache.cpp     | ---> |  sender_sched.cpp   |
| (Sensors/Sim)     |      | (Thread-Safe Snapshot)|      | (PGN Packet Packer) |
+-------------------+      +-----------------------+      +---------------------+
                                                                     |
                                                               ESP-NOW Broadcast
                                                                     v
+-------------------+      +-----------------------+      +---------------------+
|    updateUI.c     | <--- |    small_gauge.cpp    | <--- |  vcan_receiver.cpp  |
|  (LVGL Needle UI) |      | (LERP & Gauge State)  |      |  (Callback Router)  |
+-------------------+      +-----------------------+      +---------------------+
```

---

## Telemetry Pipeline Layers

1. **`daq_cache.cpp` (Data Acquisition Cache)**
   * Hardware drivers and simulation generators sample sensor values into `daq_cache_t`.
   * Holds raw values scaled by `INT_SCALING` (100) where fixed-point math is required.
   * Thread safety is maintained via `g_cache_mutex`.

2. **`vcan_protocol.h` (Network Protocol Header)**
   * Defines standard PGN IDs (e.g., `PGN_TEMPS = 0x04`) and packed C-structures using exact byte offsets and integer sizing (`int16_t` / `int32_t`).

3. **`sender_sched.cpp` (Transmitter Scheduler)**
   * Periodically copies snapshots from `daq_cache_t`.
   * Maps internal cache variables into `vcan_protocol.h` structures and dispatches them via `vcan::Sender::instance().send()`.

4. **`vcan_receiver.cpp` & `small_gauge.cpp` (Receiver & State Handler)**
   * Receives ESP-NOW payload buffers, validates structure lengths (`len >= sizeof(pgn_struct_t)`), and executes registered PGN callbacks.
   * Promotes fields (`int16_t` -> `int32_t`), stores them in mutex-protected local state (`s_state`), and passes them through LERP smoothing filters (`lerp_f`).

5. **`updateUI.c` (LVGL Display Driver)**
   * Converts telemetry values using integer rounding math:
     
     $$\text{display\_val} = \frac{\text{raw\_val} + \frac{\text{INT\_SCALING}}{2}}{\text{INT\_SCALING}}$$

   * Compares calculated values against `cached_trans` or `cached_fuel_pressure` using `UPDATE_THRESHOLD` to prevent unnecessary LVGL draw calls.

---

## Step-by-Step: Adding a New PGN and Telemetry Value

This example demonstrates adding a new **Transmission Pressure** field (`trans_pressure`) to a new PGN (`PGN_TRANSMISSION_EXTRA = 0x0C`).

### Step 1: Update `vcan_protocol.h`
Define the new PGN constant and design a byte-aligned structure using explicit integer types (`int16_t`, `int32_t`).

```c
#define PGN_TRANSMISSION_EXTRA 0x0C

// PGN_TRANSMISSION_EXTRA
typedef struct {
    int16_t trans_pressure;   // [0..1]
    int16_t trans_clutch_temp; // [2..3]
    uint32_t reserved;         // [4..7]
} pgn_transmission_extra_t;
```

---

### Step 2: Extend `daq_cache_t` (`daq_cache.h` / `daq_cache.cpp`)
1. Add the variable to the snapshot struct in `daq_cache.h`:

```cpp
typedef struct {
    // ... existing fields ...
    int32_t transPressure; // Standard fixed-point scale (val * INT_SCALING)
} daq_cache_t;
```

2. Sample or simulate the value in `daq_cache.cpp`:

```cpp
// Inside inject_debug_simulation() or hardware sampling block:
cache->transPressure = (100 + (sim % 150)) * INT_SCALING; // 100 -> 250 PSI
```

---

### Step 3: Register and Pack in `sender_sched.cpp`
1. Register the PGN schedule in `sender_sched_init()`:

```cpp
sender_sched_register(PGN_TRANSMISSION_EXTRA, CAN_PRIORITY_MED, 100); // 100ms interval
```

2. Add a `switch` case in `send_pgn_now()` to map from `daq_cache_t` to `pgn_transmission_extra_t`:

```cpp
case PGN_TRANSMISSION_EXTRA:
{
    pgn_transmission_extra_t msg{};
    msg.trans_pressure   = (int16_t)cache.transPressure; // [0..1]
    msg.trans_clutch_temp = 0;                            // [2..3]
    sender.send(msg);
    break;
}
```

---

### Step 4: Register Callback in Receiver (`small_gauge.cpp`)
1. Extend receiver state struct in `small_gauge.cpp`:

```cpp
typedef struct {
    // ... existing state ...
    int32_t trans_pressure;
} gauge_state_t;
```

2. Register the callback in `small_gauge_init()`:

```cpp
vcan::Receiver::instance().registerCallback(PGN_TRANSMISSION_EXTRA, [](uint16_t pgn, const uint8_t *payload, uint8_t len) {
    if (!payload || len < sizeof(pgn_transmission_extra_t)) return;
    const auto *msg = reinterpret_cast<const pgn_transmission_extra_t*>(payload);

    if (xSemaphoreTake(s_mutex, 0) == pdTRUE) {
        s_state.trans_pressure = static_cast<int32_t>(msg->trans_pressure);
        s_state.last_update_ms = now_ms();
        xSemaphoreGive(s_mutex);
    }
});
```
### Step 3: Register, Overload, and Pack in Sender

1. **Add `send()` Overload in `vcan_sender.h`:**
   ```cpp
   esp_err_t send(const pgn_transmission_extra_t& msg) {
       return sendRawPayload(PGN_TRANSMISSION_EXTRA, 0x01, CAN_PRIORITY_MED, &msg, sizeof(msg));
   }
   ```
   *(Or alternatively, call `sender.sendRawPayload(PGN_TRANSMISSION_EXTRA, ...)` directly inside `sender_sched.cpp`)*

2. **Register the PGN schedule in `sender_sched_init()`:**
   ```cpp
   sender_sched_register(PGN_TRANSMISSION_EXTRA, CAN_PRIORITY_MED, 100); // 100ms interval
   ```

3. **Add `switch` case in `send_pgn_now()` (`sender_sched.cpp`):**
   ```cpp
   case PGN_TRANSMISSION_EXTRA:
   {
       pgn_transmission_extra_t msg{};
       msg.trans_pressure   = (int16_t)cache.transPressure; // [0..1]
       msg.trans_clutch_temp = 0;                            // [2..3]
       sender.send(msg); // Works now that overload exists!
       break;
   }
   ```

4. Smooth and draw in `small_gauge_draw()`:

```cpp
// Inside small_gauge_draw():
s_trans_press_lerp = lerp_f(s_trans_press_lerp, static_cast<float>(trans_press_raw), LERP_ALPHA);
update_trans_press_meter(static_cast<int32_t>(s_trans_press_lerp));
```

---

### Step 5: Implement LVGL Draw Routine (`updateUI.c`)
Implement integer scaling, hysteresis checks, and LVGL object updates:

```c
static int32_t cached_trans_press = -999;

void update_trans_press_meter(int32_t new_val)
{
    // Apply proper rounding: (val + (INT_SCALING / 2)) / INT_SCALING
    int32_t display_val = (new_val + (INT_SCALING / 2)) / INT_SCALING;

    if (abs(display_val - cached_trans_press) > UPDATE_THRESHOLD)
    {
        ESP_LOGI("updateUI", "Trans Press LVGL Update: raw=%" PRId32 " -> display=%" PRId32, new_val, display_val);
        lv_meter_set_indicator_value(objects.trans_press, screen_main_state.trans_press, display_val);
        cached_trans_press = display_val;
    }
}
```

---

## Troubleshooting Checklist

* **Missing Log Messages:** Verify `CONFIG_LOG_MAXIMUM_LEVEL` in `sdkconfig` is set to `VERBOSE` and that `esp_log_level_set("*", ESP_LOG_NONE)` is not called at runtime.
* **Callback Never Fires:** Verify `len < sizeof(pgn_struct_t)` is passing and that the transmitter sending `pgn` matches the ID registered in `vcan_protocol.h`.
* **Needle Jerkiness or Freeze:** Check if `UPDATE_THRESHOLD` is too high relative to `INT_SCALING` or if `s_mutex` lock acquisitions are timing out.