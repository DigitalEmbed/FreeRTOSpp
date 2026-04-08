/**
 * @file 01_basic_tasks.cpp
 * @brief Basic FreeRTOS++ usage: tasks, LIFO queue, shared data and timers.
 *
 * This example demonstrates:
 *  - A stack-allocated task with a typed argument (shared_data reference).
 *  - A second task that monitors the first and restarts it when it finishes.
 *  - A heap-allocated task with a lambda that captures by reference.
 *  - A LIFO queue populated and drained at startup.
 *  - A stack timer with an argument and a heap timer with no argument.
 *  - An application class (abstract::app) with the singleton pattern.
 *
 * Target: ESP32 / STM32 with Arduino framework (adapt Serial calls as needed).
 */

#include <Arduino.h>
#include "freertos.hpp"

// ---------------------------------------------------------------------------
// Application class
// ---------------------------------------------------------------------------

/**
 * @brief Minimal application that prints "Hello World" every 200 ms.
 *
 * Inherits from freertos::abstract::app which provides start() / stop() and
 * ties the object lifetime to the internal task's lifetime.
 */
class my_app : public freertos::abstract::app {
private:
    static constexpr const char*  NAME       = "my_app";
    static constexpr uint32_t     STACK_SIZE = 1024; // words
    static constexpr uint16_t     PRIORITY   = 1;

    // The task that drives this application.
    freertos::stack::task<STACK_SIZE, freertos::abstract::app&> task;

    // Entry point for the task — runs in the FreeRTOS scheduler context.
    static void run(freertos::abstract::app& app) {
        while (true) {
            Serial.println("[my_app] Hello World");
            freertos::this_app::delay(200);
        }
    }

    // Private constructor enforces the singleton pattern.
    my_app()
        : freertos::abstract::app(task),
          task(NAME, PRIORITY, *this, my_app::run)
    {}

public:
    /** @brief Returns the singleton instance. */
    static my_app& instance() {
        static my_app inst;
        return inst;
    }
};

// ---------------------------------------------------------------------------
// setup()
// ---------------------------------------------------------------------------

void setup() {
    Serial.begin(115200);

    // ------------------------------------------------------------------
    // 1. LIFO queue — drain on startup before the scheduler starts tasks
    // ------------------------------------------------------------------
    // stack::lifo<CAPACITY, DATA_TYPE> — 2 slots, overwrite_if_full = true
    freertos::stack::lifo<2, const char*> messages(/*overwrite_if_full=*/true);

    messages.push("First");
    messages.push("Second");
    messages.push("Third");   // overwrites "First" because capacity is 2

    while (messages.available() > 0) {
        const char* msg = nullptr;
        if (messages.pop(msg, 0)) {         // 0 ms = non-blocking
            Serial.printf("[lifo] %s\n", msg);
        }
    }

    // ------------------------------------------------------------------
    // 2. Shared data + two cooperating tasks
    // ------------------------------------------------------------------

    // A mutex-protected integer shared between two tasks.
    static freertos::stack::shared_data<int> counter(0);

    // Task A: increments the counter until it exceeds 20, then returns.
    static freertos::stack::task<2048, freertos::abstract::shared_data<int>&>
        task_a("task_a", 1, counter, [](freertos::abstract::shared_data<int>& shared) {
            while (true) {
                int val = shared.get();
                if (val > 20) {
                    Serial.println("[task_a] done");
                    return;         // returning suspends the task automatically
                }
                Serial.printf("[task_a] counter = %d\n", val);
                shared.set(val + 1);
                freertos::this_task::delay(250);
            }
        });
    task_a.resume();

    // Bundle references so Task B can access both objects.
    static struct {
        freertos::stack::shared_data<int>&                               data;
        freertos::stack::task<2048, freertos::abstract::shared_data<int>&>& task;
    } ctx = { counter, task_a };

    // Task B: watches Task A; resets and restarts it when it finishes.
    static freertos::stack::task<2048, decltype(ctx)&>
        task_b("task_b", 1, ctx, [](decltype(ctx)& c) {
            while (true) {
                if (!c.task.is_running()) {
                    Serial.println("[task_b] restarting task_a");
                    c.data.set(0);
                    c.task.resume();
                }
                freertos::this_task::delay(1000);
            }
        });
    task_b.resume();

    // ------------------------------------------------------------------
    // 3. Heap task — lambda with capture (only possible with heap::task)
    // ------------------------------------------------------------------
    static int8_t heap_counter = 0;

    // heap::task<void> stores the callback as std::function, so lambdas
    // that capture local/static variables by reference are fully supported.
    static freertos::heap::task<> heap_task("heap_task", 1, 2048, [&]() {
        while (true) {
            Serial.printf("[heap_task] value = %d\n", heap_counter++);
            freertos::this_task::delay(1000);
        }
    });
    heap_task.resume();

    // ------------------------------------------------------------------
    // 4. Timers
    // ------------------------------------------------------------------

    // Stack timer — receives a typed argument (int& reference).
    static int timer_val = 0;
    static freertos::stack::timer<int&> periodic_timer(
        "periodic",
        timer_val,
        [](int& v) {
            v++;
            Serial.printf("[timer] value = %d\n", v);
        },
        /*period_ms=*/1000,
        /*auto_reload=*/true);
    periodic_timer.start();

    // Heap timer — no argument, starts automatically.
    static freertos::heap::timer<> heartbeat_timer(
        "heartbeat",
        []() { Serial.println("[heartbeat] tick"); },
        /*period_ms=*/500,
        /*auto_reload=*/true,
        /*auto_start=*/true);

    // ------------------------------------------------------------------
    // 5. Start the application
    // ------------------------------------------------------------------
    my_app::instance().start();
}

void loop() {
    // The loop task is not used — suspend it to avoid wasting stack space.
    freertos::this_task::suspend();
}
