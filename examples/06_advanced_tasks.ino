/**
 * @file 06_advanced_tasks.cpp
 * @brief Advanced task features, system utilities, heap shared data and all notifier methods.
 *
 * This example demonstrates:
 *  - this_task::delay_for()   — poll a predicate, sleeping between checks.
 *  - this_task::suspend_for() — suspend while a condition holds.
 *  - this_task::yield()       — cooperative scheduling between equal-priority tasks.
 *  - task::join()             — block until another task finishes.
 *  - system::get_tick_count() / get_milliseconds() / get_amount_of_tasks().
 *  - heap::shared_data<T>     — dynamically-allocated thread-safe wrapper.
 *  - task::notifier — all send modes: signal(), set_bits(), increment(),
 *                     overwrite_value(), send_value(), clear().
 *  - Notifier ISR variants via a software timer callback.
 *
 * Target: ESP32 / STM32 with Arduino framework.
 */

#include <Arduino.h>
#include "freertos.hpp"

using namespace freertos;

// ---------------------------------------------------------------------------
// 1. this_task::delay_for() — wait until a flag is set
//
// A "work" task waits for a flag to become true before proceeding.
// Another task sets the flag after a short delay.
// delay_for() polls the condition every 100 ms without busy-waiting.
// ---------------------------------------------------------------------------
static void demo_delay_for() {
    static bool flag_ready = false;

    // Flag setter — raises the flag after 1.5 s.
    static stack::task<1024> setter("flag_setter", 1, []() {
        this_task::delay(1500);
        flag_ready = true;
        Serial.println("[delay_for] flag set");
        this_task::suspend();
    });

    // Waiter — blocks with delay_for() until the flag is true.
    static stack::task<1024> waiter("flag_waiter", 1, []() {
        Serial.println("[delay_for] waiting for flag...");

        bool ok = this_task::delay_for(
            &flag_ready,                        // argument passed to condition
            [](bool* f) { return *f; },         // condition: returns true when flag is set
            /*observer_delay_ms=*/100,
            /*timeout_ms=*/5000);

        if (ok) {
            Serial.println("[delay_for] flag detected — proceeding");
        } else {
            Serial.println("[delay_for] timeout waiting for flag");
        }
        this_task::suspend();
    });

    setter.resume();
    waiter.resume();
}

// ---------------------------------------------------------------------------
// 2. this_task::suspend_for() — stay suspended while a condition holds
//
// A task that should only run when a "door" is open.  It calls suspend_for()
// to stay suspended as long as the door is closed.  An external task opens
// and closes the door periodically — resuming or allowing re-entry.
// ---------------------------------------------------------------------------
static void demo_suspend_for() {
    static bool door_closed = true;

    // Door controller — toggles the door every 2 s.
    static stack::task<1024> door_ctrl("door_ctrl", 1, []() {
        while (true) {
            this_task::delay(2000);
            door_closed = false;
            Serial.println("[suspend_for] door opened");
            this_task::delay(500);
            door_closed = true;
            Serial.println("[suspend_for] door closed");
        }
    });

    // Worker — must not run while door is closed.
    // suspend_for() suspends the task on each iteration where the condition
    // holds; another task or ISR must call resume() to re-evaluate.
    static stack::task<1024>* worker_ptr = nullptr;

    static stack::task<1024> worker("door_worker", 1, []() {
        while (true) {
            // Suspend while door is closed (requires resume() from outside).
            this_task::suspend_for(
                &door_closed,
                [](bool* closed) { return *closed; });

            Serial.println("[suspend_for] worker: door is open — doing work");
            this_task::delay(200);
        }
    });
    worker_ptr = &worker;

    // A helper task that periodically resumes the worker so it can
    // re-evaluate the condition after the door opens.
    static stack::task<1024, abstract::task*> resumer("door_resumer", 1, worker_ptr,
        [](abstract::task* w) {
            while (true) {
                this_task::delay(300);
                w->resume();    // wake the worker to re-check the condition
            }
        });

    door_ctrl.resume();
    worker.resume();
    resumer.resume();
}

// ---------------------------------------------------------------------------
// 3. this_task::yield() — cooperative scheduling
//
// Two equal-priority tasks share the CPU voluntarily.  Each prints its name
// and immediately yields to give the other a chance to run.
// Without yield() one task could starve the other on a non-preemptive tick.
// ---------------------------------------------------------------------------
static void demo_yield() {
    static stack::task<1024> t1("yield_t1", 1, []() {
        for (int i = 0; i < 5; i++) {
            Serial.printf("[yield] task_1 iteration %d\n", i);
            this_task::yield();     // hand control to task_2
        }
        this_task::suspend();
    });

    static stack::task<1024> t2("yield_t2", 1, []() {
        for (int i = 0; i < 5; i++) {
            Serial.printf("[yield] task_2 iteration %d\n", i);
            this_task::yield();     // hand control back to task_1
        }
        this_task::suspend();
    });

    t1.resume();
    t2.resume();
}

// ---------------------------------------------------------------------------
// 4. task::join() — wait for a worker task to finish
//
// A short-lived worker does some computation and returns.  The monitor task
// calls join() to block until the worker completes, then reads the result.
// ---------------------------------------------------------------------------
static void demo_join() {
    static int worker_result = 0;

    static stack::task<1024> worker("join_worker", 1, []() {
        Serial.println("[join] worker: starting computation");
        this_task::delay(1000);         // simulate work
        worker_result = 42;
        Serial.println("[join] worker: done");
        // Returning suspends the task — join() detects this.
    });

    static stack::task<1024, abstract::task*> monitor("join_monitor", 1, &worker,
        [](abstract::task* w) {
            w->resume();                // kick off the worker

            Serial.println("[join] monitor: waiting for worker...");
            bool finished = w->join(/*poll_ms=*/50, /*timeout_ms=*/3000);

            if (finished) {
                Serial.printf("[join] monitor: worker finished, result = %d\n", worker_result);
            } else {
                Serial.println("[join] monitor: timeout waiting for worker");
            }
            this_task::suspend();
        });

    monitor.resume();
}

// ---------------------------------------------------------------------------
// 5. system utilities
//
// get_tick_count(), get_milliseconds(), get_amount_of_tasks() — useful for
// timestamps, profiling and runtime diagnostics.
// ---------------------------------------------------------------------------
static void demo_system() {
    static stack::task<1024> sys_task("sys_info", 1, []() {
        while (true) {
            uint32_t ticks = system::get_tick_count();
            uint32_t ms    = system::get_milliseconds();
            uint32_t tasks = system::get_amount_of_tasks();

            Serial.printf("[system] ticks=%lu  ms=%lu  active_tasks=%lu\n",
                          ticks, ms, tasks);
            this_task::delay(2000);
        }
    });
    sys_task.resume();
}

// ---------------------------------------------------------------------------
// 6. heap::shared_data — dynamically-allocated thread-safe wrapper
//
// heap::shared_data<T> works identically to stack::shared_data<T> but the
// internal mutex is heap-allocated.  Useful when shared objects are created
// after startup or inside classes that cannot hold static storage.
// ---------------------------------------------------------------------------
static void demo_heap_shared_data() {
    // Dynamic shared data — no static storage needed.
    static heap::shared_data<float> temperature(25.0f);

    static stack::task<1024> writer("hsd_writer", 1, []() {
        static float t = 25.0f;
        while (true) {
            t += 0.1f;
            temperature.set(t);
            this_task::delay(300);
        }
    });

    static stack::task<1024> reader("hsd_reader", 1, []() {
        while (true) {
            float val = temperature.get();
            Serial.printf("[heap_sd] temperature = %.1f°C\n", val);

            // use() for atomic read-modify-write without external lock.
            temperature.use([](float& v) { v = v < 30.0f ? v : 25.0f; });

            this_task::delay(1000);
        }
    });

    writer.resume();
    reader.resume();
}

// ---------------------------------------------------------------------------
// 7. All notifier send modes
//
// Each method encodes a different value update strategy:
//
//  signal()         — increment notification value by 1 (counting signal)
//  set_bits()       — bitwise OR a mask (event flags)
//  increment()      — add N to the notification value
//  overwrite_value()— unconditionally set value (may lose previous notification)
//  send_value()     — set value only if previous one was consumed (safe send)
//  clear()          — cancel a pending notification
//
// A single receiver task demonstrates all of them via different notification
// indexes so that the modes do not interfere.
// ---------------------------------------------------------------------------

// Event bits used with set_bits().
static constexpr uint32_t EVT_A = (1 << 0);
static constexpr uint32_t EVT_B = (1 << 1);
static constexpr uint32_t EVT_C = (1 << 2);

static void demo_notifier() {
    // Receiver waits on index 0 (generic value) and index 1 (event bits).
    static stack::task<2048> receiver("notif_rx", 2, []() {
        while (true) {
            // --- index 0: generic accumulated value ---
            uint32_t val = 0;
            if (this_task::get_notification(val, /*index=*/0, /*timeout_ms=*/200)) {
                Serial.printf("[notifier] idx0 value = %lu\n", val);
            }

            // --- index 1: event bits ---
            uint32_t bits = 0;
            if (this_task::get_notification(bits, /*index=*/1, /*timeout_ms=*/200)) {
                Serial.printf("[notifier] idx1 events: A=%d B=%d C=%d\n",
                    !!(bits & EVT_A), !!(bits & EVT_B), !!(bits & EVT_C));
            }
        }
    });
    receiver.resume();

    // Senders — each demonstrates one notifier method.

    // signal() — increments the value; multiple calls accumulate.
    static stack::task<1024, abstract::task*> sig_task("notif_signal", 1, &receiver,
        [](abstract::task* rx) {
            while (true) {
                rx->get_notifier().signal(/*index=*/0, /*resume=*/true);
                rx->get_notifier().signal(/*index=*/0, /*resume=*/true); // now value = 2
                this_task::delay(1000);
            }
        });

    // set_bits() — OR bits into index 1; receiver decodes individual events.
    static stack::task<1024, abstract::task*> bits_task("notif_bits", 1, &receiver,
        [](abstract::task* rx) {
            while (true) {
                rx->get_notifier().set_bits(EVT_A | EVT_C, /*index=*/1, /*resume=*/true);
                this_task::delay(1500);
                rx->get_notifier().set_bits(EVT_B, /*index=*/1, /*resume=*/true);
                this_task::delay(1500);
            }
        });

    // increment() — adds a specific amount to the notification value.
    static stack::task<1024, abstract::task*> inc_task("notif_inc", 1, &receiver,
        [](abstract::task* rx) {
            while (true) {
                rx->get_notifier().increment(10, /*index=*/0, /*resume=*/true);
                this_task::delay(2000);
            }
        });

    // overwrite_value() — forces a specific value regardless of pending state.
    static stack::task<1024, abstract::task*> ow_task("notif_overwrite", 1, &receiver,
        [](abstract::task* rx) {
            while (true) {
                this_task::delay(3000);
                rx->get_notifier().overwrite_value(0xDEAD, /*index=*/0, /*resume=*/true);
            }
        });

    // send_value() — only sends if the previous notification was consumed.
    // clear() — cancels a pending notification.
    static stack::task<1024, abstract::task*> sv_task("notif_send", 1, &receiver,
        [](abstract::task* rx) {
            while (true) {
                bool sent = rx->get_notifier().send_value(99, /*index=*/0, /*resume=*/true);
                if (!sent) {
                    Serial.println("[notifier] send_value: previous not yet read — clearing");
                    rx->get_notifier().clear(/*index=*/0);
                }
                this_task::delay(500);
            }
        });

    sig_task.resume();
    bits_task.resume();
    inc_task.resume();
    ow_task.resume();
    sv_task.resume();
}

// ---------------------------------------------------------------------------
// 8. Notifier ISR variants — signal from a timer callback
//
// A heap::timer fires every 400 ms in a software-ISR context and uses
// signal_from_isr() to increment the receiver's notification value.
// increment_from_isr() and set_bits_from_isr() follow the same pattern.
// ---------------------------------------------------------------------------
static void demo_notifier_from_isr() {
    static stack::task<1024> isr_rx("isr_notif_rx", 1, []() {
        while (true) {
            uint32_t val = 0;
            if (this_task::get_notification(val, 0, 600)) {
                Serial.printf("[notifier_isr] received from ISR: %lu\n", val);
            }
        }
    });
    isr_rx.resume();

    // Obtain the notifier before the timer is created — safe because the
    // notifier is a lightweight value object bound to the task handle.
    static abstract::task::notifier notif = isr_rx.get_notifier();

    static heap::timer<> isr_timer(
        "isr_notif_timer",
        []() {
            // signal_from_isr() is safe to call from any ISR/timer callback.
            notif.signal_from_isr(/*index=*/0, /*resume=*/true);
        },
        /*period_ms=*/400,
        /*auto_reload=*/true,
        /*auto_start=*/true);
}

// ---------------------------------------------------------------------------
// setup()
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);

    demo_delay_for();
    demo_suspend_for();
    demo_yield();
    demo_join();
    demo_system();
    demo_heap_shared_data();
    demo_notifier();
    demo_notifier_from_isr();
}

void loop() {
    freertos::this_task::suspend();
}
