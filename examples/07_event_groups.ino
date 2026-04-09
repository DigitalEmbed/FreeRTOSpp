/**
 * @file 07_event_groups.ino
 * @brief Event groups — independent flag bits with a concise proxy API.
 *
 * This example demonstrates:
 *  - stack::event_group — static allocation, up to constants::max_event_group_bits bits.
 *  - operator[]  — proxy-based read and write via operator=.
 *  - operator=   — set/clear a bit  (events[i] = true) and copy bits (events[i] = events[j]).
 *  - Implicit conversions — bool, int8_t, uint8_t, int.
 *  - set() / get() — explicit methods that return false on out-of-bounds.
 *  - set_from_isr() / get_from_isr() — ISR-safe variants.
 *  - Indexed for loop — iterate all max_event_group_bits bits with index available.
 *  - Runtime enforcement — configASSERT + no-op on index >= max_event_group_bits.
 *
 * Target: ESP32 / STM32 with Arduino framework.
 */

#include <Arduino.h>
#include "freertos.hpp"

using namespace freertos;

// ---------------------------------------------------------------------------
// Shared event group — statically allocated.
// Supports up to constants::max_event_group_bits independent flags
// (24 on STM32/ESP32, 8 on 8-bit AVR with configUSE_16_BIT_TICKS=1).
// Each bit represents one independent system flag.
// ---------------------------------------------------------------------------
static stack::event_group events;

// Bit index aliases — give each flag a meaningful name.
static constexpr uint32_t BIT_SENSOR_READY   = 0;
static constexpr uint32_t BIT_WIFI_CONNECTED = 1;
static constexpr uint32_t BIT_DATA_PENDING   = 2;
static constexpr uint32_t BIT_ALARM          = 3;

// ---------------------------------------------------------------------------
// 1. Basic read and write via operator[]
// ---------------------------------------------------------------------------
static void demo_read_write() {
    Serial.println("\n--- 1. Basic read / write ---");

    // Write via operator= on the proxy — same convention as std::bitset::reference.
    events[BIT_SENSOR_READY]   = true;
    events[BIT_WIFI_CONNECTED] = true;
    events[BIT_DATA_PENDING]   = false;
    events[BIT_ALARM]          = false;

    // Read as bool — implicit conversion from proxy.
    bool sensor  = events[BIT_SENSOR_READY];
    bool wifi    = events[BIT_WIFI_CONNECTED];
    bool pending = events[BIT_DATA_PENDING];
    bool alarm   = events[BIT_ALARM];

    Serial.printf("  sensor_ready=%d  wifi=%d  data_pending=%d  alarm=%d\n",
                  sensor, wifi, pending, alarm);

    // Read as int8_t — also via implicit conversion.
    int8_t sensor_int = events[BIT_SENSOR_READY];
    Serial.printf("  sensor_ready as int8_t = %d\n", sensor_int);

    // Copy one bit into another — operator=(const event_bit_ref&).
    events[BIT_ALARM] = events[BIT_SENSOR_READY];
    bool alarm_copy = events[BIT_ALARM];
    Serial.printf("  alarm after copy from sensor_ready = %d\n", alarm_copy);

    // NOTE: The following would be compile errors — bits are not addressable:
    //   bool&   ref = events[0];   // ERROR: cannot bind ref to rvalue proxy
    //   bool*   ptr = events[0];   // ERROR: cannot convert proxy to bool*
    //   int8_t& ri  = events[0];   // ERROR: same reason
}

// ---------------------------------------------------------------------------
// 2. set() / get() — explicit methods with bounds-check return value
// ---------------------------------------------------------------------------
static void demo_set_get() {
    Serial.println("\n--- 2. set() / get() ---");

    // set() returns true on success, false on out-of-bounds or invalid handle.
    bool ok = events.set(BIT_ALARM, true);
    Serial.printf("  set(BIT_ALARM, true)  -> ok=%d\n", ok);

    bool val = false;
    ok = events.get(BIT_ALARM, val);
    Serial.printf("  get(BIT_ALARM)        -> ok=%d  value=%d\n", ok, val);

    // Out-of-bounds: configASSERT fires in debug builds; in release, returns false safely.
    ok = events.set(30, true);
    Serial.printf("  set(30, true)         -> ok=%d  (out-of-bounds)\n", ok);

    ok = events.get(30, val);
    Serial.printf("  get(30)               -> ok=%d  (out-of-bounds)\n", ok);

    // operator[] out-of-bounds: also hits configASSERT + returns no-op proxy.
    // events[30] = true;  // configASSERT + silently ignored in release
}

// ---------------------------------------------------------------------------
// 3. Indexed for loop — iterate bits with index available
// ---------------------------------------------------------------------------
static void demo_iteration() {
    Serial.println("\n--- 3. Indexed iteration ---");

    // Set a known pattern before iterating.
    events[0] = true;
    events[1] = false;
    events[2] = true;
    events[3] = true;
    events[4] = false;
    events[5] = false;
    events[6] = true;
    events[7] = false;

    // Index is always available — no ambiguity about which bit is being read.
    Serial.print("  bits 0-7: ");
    for (uint32_t i = 0; i < 8; i++) {
        bool value = events[i];
        Serial.printf("[%lu]=%d ", i, value);
    }
    Serial.println();

    // Clear all used bits.
    Serial.println("  clearing bits 0-7...");
    for (uint32_t i = 0; i < 8; i++) {
        events[i] = false;
    }

    Serial.print("  bits 0-7 after clear: ");
    for (uint32_t i = 0; i < 8; i++) {
        Serial.printf("[%lu]=%d ", i, static_cast<bool>(events[i]));
    }
    Serial.println();
}

// ---------------------------------------------------------------------------
// 4. Inter-task communication — producer sets bits, consumer reacts
//
// Sensor task sets BIT_SENSOR_READY every 800 ms.
// Network task sets BIT_WIFI_CONNECTED after a simulated connect delay.
// Monitor task polls the group and reacts when all conditions are met.
// ---------------------------------------------------------------------------
static void demo_tasks() {
    Serial.println("\n--- 4. Inter-task communication ---");

    // Reset flags used in this demo.
    events[BIT_SENSOR_READY]   = false;
    events[BIT_WIFI_CONNECTED] = false;

    // Sensor task — signals BIT_SENSOR_READY every 800 ms.
    static stack::task<1024> sensor_task("sensor", 1, []() {
        while (true) {
            this_task::delay(800);
            events[BIT_SENSOR_READY] = true;
            Serial.println("  [sensor] ready");
        }
    });

    // Network task — simulates a slow connect, then signals BIT_WIFI_CONNECTED.
    static stack::task<1024> net_task("network", 1, []() {
        this_task::delay(2000);         // simulate connection delay
        events[BIT_WIFI_CONNECTED] = true;
        Serial.println("  [network] connected");

        // After connecting, toggle DATA_PENDING every 1.5 s.
        while (true) {
            this_task::delay(1500);
            bool current = events[BIT_DATA_PENDING];
            events[BIT_DATA_PENDING] = !current;
            Serial.printf("  [network] data_pending toggled -> %d\n", !current);
        }
    });

    // Monitor task — checks the group every 300 ms and reacts.
    static stack::task<1536> monitor_task("monitor", 1, []() {
        while (true) {
            bool sensor  = events[BIT_SENSOR_READY];
            bool wifi    = events[BIT_WIFI_CONNECTED];
            bool pending = events[BIT_DATA_PENDING];
            bool alarm   = events[BIT_ALARM];

            Serial.printf("  [monitor] sensor=%d  wifi=%d  pending=%d  alarm=%d\n",
                          sensor, wifi, pending, alarm);

            // React when all conditions are met.
            if (sensor && wifi && pending) {
                Serial.println("  [monitor] ** all conditions met — processing data **");
                events[BIT_DATA_PENDING] = false;  // consume the flag
                events[BIT_SENSOR_READY] = false;  // reset for next cycle
            }

            this_task::delay(300);
        }
    });

    sensor_task.resume();
    net_task.resume();
    monitor_task.resume();
}

// ---------------------------------------------------------------------------
// 5. ISR variants — set/get from a timer callback (ISR context)
// ---------------------------------------------------------------------------
static void demo_isr() {
    Serial.println("\n--- 5. ISR variants ---");

    static heap::timer<> isr_timer(
        "isr_evt",
        []() {
            // Toggle BIT_ALARM from ISR context every 2 s.
            bool current = false;
            events.get_from_isr(BIT_ALARM, current);
            events.set_from_isr(BIT_ALARM, !current);
        },
        /*period_ms=*/2000,
        /*auto_reload=*/true,
        /*auto_start=*/true);

    static stack::task<1024> isr_reader("isr_reader", 1, []() {
        while (true) {
            bool alarm = events[BIT_ALARM];
            Serial.printf("  [isr_reader] alarm bit = %d\n", alarm);
            this_task::delay(500);
        }
    });

    isr_reader.resume();
}

// ---------------------------------------------------------------------------
// 6. Runtime limit reference
//
// The maximum usable index is constants::max_event_group_bits - 1.
// Accessing beyond that fires configASSERT in debug builds, and returns
// a no-op proxy / false in release builds.
//
// The limit is computed at compile time from sizeof(EventBits_t)*8 - 8:
//   24 on 32-bit platforms (configUSE_16_BIT_TICKS=0, default on STM32/ESP32)
//    8 on 16-bit platforms (configUSE_16_BIT_TICKS=1, typical on 8-bit AVR)
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// setup()
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(500);

    demo_read_write();
    demo_set_get();
    demo_iteration();
    demo_isr();
    demo_tasks();      // starts background tasks — output continues in loop()
}

void loop() {
    freertos::this_task::suspend();
}
