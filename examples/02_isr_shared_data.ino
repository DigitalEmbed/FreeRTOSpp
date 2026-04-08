/**
 * @file 02_isr_shared_data.cpp
 * @brief Multiple application objects sharing ISR-safe data with a hardware timer.
 *
 * This example demonstrates:
 *  - Two independent application classes (my_app, my_app_2) each following
 *    the abstract::app / singleton pattern.
 *  - shared_data_with_isr<int> — a binary-semaphore-protected container that
 *    can be safely read/written from both task and ISR context.
 *  - A FreeRTOS-independent hardware timer (esp_timer) that increments the
 *    shared counter from an ISR via use_from_isr().
 *
 * Target: ESP32 (requires esp_timer.h from ESP-IDF / Arduino-ESP32).
 */

#include <Arduino.h>
#include "freertos.hpp"
#include "esp_timer.h"

// ---------------------------------------------------------------------------
// Application 1 — reads the shared counter and prints it every second.
// ---------------------------------------------------------------------------
class my_app : public freertos::abstract::app {
private:
    static constexpr const char* NAME       = "my_app";
    static constexpr uint32_t    STACK_SIZE = 4 * 1024;
    static constexpr uint16_t    PRIORITY   = 1;

    freertos::stack::task<STACK_SIZE, my_app&> task;

    static void run(my_app& self) {
        while (true) {
            // use() holds the mutex for the duration of the lambda.
            self.counter->use([](int& v) {
                Serial.printf("[my_app] counter = %d\n", v);
            });
            freertos::this_app::delay(1000);
        }
    }

    my_app()
        : freertos::abstract::app(task),
          task(NAME, PRIORITY, *this, my_app::run)
    {}

public:
    // Pointer set from setup() after the shared object is created.
    freertos::stack::shared_data_with_isr<int>* counter {nullptr};

    static my_app& instance() {
        static my_app inst;
        return inst;
    }
};

// ---------------------------------------------------------------------------
// Application 2 — reads the same counter at a faster rate (100 ms).
// ---------------------------------------------------------------------------
class my_app_2 : public freertos::abstract::app {
private:
    static constexpr const char* NAME       = "my_app_2";
    static constexpr uint32_t    STACK_SIZE = 4 * 1024;
    static constexpr uint16_t    PRIORITY   = 1;

    freertos::stack::task<STACK_SIZE, my_app_2&> task;

    static void run(my_app_2& self) {
        while (true) {
            self.counter->use([](int& v) {
                Serial.printf("[my_app_2] counter = %d\n", v);
            });
            freertos::this_app::delay(100);
        }
    }

    my_app_2()
        : freertos::abstract::app(task),
          task(NAME, PRIORITY, *this, my_app_2::run)
    {}

public:
    freertos::stack::shared_data_with_isr<int>* counter {nullptr};

    static my_app_2& instance() {
        static my_app_2 inst;
        return inst;
    }
};

// ---------------------------------------------------------------------------
// Hardware timer ISR — increments the shared counter every 100 ms.
//
// IRAM_ATTR ensures the function is placed in IRAM so it can execute even
// when the flash cache is busy.
// ---------------------------------------------------------------------------
void IRAM_ATTR on_timer(void* param) {
    auto* cnt = static_cast<freertos::stack::shared_data_with_isr<int>*>(param);
    if (cnt == nullptr) return;

    // use_from_isr() acquires the binary semaphore non-blocking, runs the
    // callback, then releases the semaphore — all in ISR context.
    cnt->use_from_isr([](int& v) {
        v++;
    });
}

// ---------------------------------------------------------------------------
// Helper — configure the ESP32 hardware timer.
// ---------------------------------------------------------------------------
static void setup_hardware_timer(freertos::stack::shared_data_with_isr<int>* cnt) {
    esp_timer_create_args_t cfg = {
        .callback        = &on_timer,
        .arg             = cnt,
        .dispatch_method = ESP_TIMER_ISR,   // run callback directly in ISR
        .name            = "hw_timer",
        .skip_unhandled_events = false
    };

    esp_timer_handle_t handle;
    ESP_ERROR_CHECK(esp_timer_create(&cfg, &handle));
    ESP_ERROR_CHECK(esp_timer_start_periodic(handle, 100'000)); // 100 ms in µs
}

// ---------------------------------------------------------------------------
// setup()
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);

    // One shared counter — protected by a binary semaphore so that both
    // tasks and the ISR can access it safely.
    static freertos::stack::shared_data_with_isr<int> counter;

    // Wire the shared counter into both application singletons.
    my_app::instance().counter   = &counter;
    my_app_2::instance().counter = &counter;

    my_app::instance().start();
    my_app_2::instance().start();

    setup_hardware_timer(&counter);
}

void loop() {
    freertos::this_task::suspend();
}
