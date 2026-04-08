/**
 * @file 05_queues_and_collections.cpp
 * @brief All collection types: FIFO, LIFO, singleton — static and dynamic.
 *
 * This example demonstrates:
 *  - stack::fifo<N,T>  — fixed-capacity FIFO queue between two tasks.
 *  - heap::fifo<T>     — dynamically-allocated FIFO queue.
 *  - stack::lifo<N,T>  — fixed-capacity LIFO stack (last-in, first-out).
 *  - heap::lifo<T>     — dynamically-allocated LIFO stack.
 *  - stack::singleton<T> — single-slot queue that always holds the latest value.
 *  - push_from_isr / pop_from_isr — ISR-safe collection access.
 *
 * Target: ESP32 / STM32 with Arduino framework.
 */

#include <Arduino.h>
#include "freertos.hpp"

using namespace freertos;

// ---------------------------------------------------------------------------
// 1. stack::fifo — sensor reading pipeline
//
// A producer samples a "sensor" every 200 ms and pushes readings into a
// fixed-capacity FIFO.  A consumer pops and processes them at its own pace.
// The queue holds up to 8 uint16_t readings.
// ---------------------------------------------------------------------------
static void demo_fifo_stack() {
    static stack::fifo<8, uint16_t> sensor_queue;

    static stack::task<1024> producer("fifo_prod", 1, []() {
        static uint16_t reading = 0;
        while (true) {
            reading = (reading + 10) % 1024;    // simulated ADC value

            if (!sensor_queue.push(reading, /*timeout_ms=*/50)) {
                Serial.println("[fifo/stack] queue full — reading dropped");
            } else {
                Serial.printf("[fifo/stack] pushed %u  (queued: %lu)\n",
                              reading, sensor_queue.available());
            }
            this_task::delay(200);
        }
    });

    static stack::task<1024> consumer("fifo_cons", 1, []() {
        while (true) {
            uint16_t val = 0;
            if (sensor_queue.pop(val, /*timeout_ms=*/500)) {
                Serial.printf("[fifo/stack] popped %u\n", val);
            }
            this_task::delay(350);
        }
    });

    producer.resume();
    consumer.resume();
}

// ---------------------------------------------------------------------------
// 2. heap::fifo — dynamically-allocated queue
//
// Identical logic to the stack variant, but the queue is allocated on the heap
// and its capacity is specified at runtime.
// ---------------------------------------------------------------------------
static void demo_fifo_heap() {
    // heap::fifo<DATA_TYPE> — capacity passed to constructor at runtime.
    static heap::fifo<uint16_t> heap_queue(/*capacity=*/4);

    static stack::task<1024> prod("heap_fifo_prod", 1, []() {
        static uint16_t val = 0;
        while (true) {
            val++;
            if (heap_queue.push(val, /*timeout_ms=*/50)) {
                Serial.printf("[fifo/heap] pushed %u  free=%lu\n",
                              val, heap_queue.get_free_space());
            }
            this_task::delay(300);
        }
    });

    static stack::task<1024> cons("heap_fifo_cons", 1, []() {
        while (true) {
            uint16_t v = 0;
            if (heap_queue.pop(v, 600)) {
                Serial.printf("[fifo/heap] popped %u\n", v);
            }
            this_task::delay(600);
        }
    });

    prod.resume();
    cons.resume();
}

// ---------------------------------------------------------------------------
// 3. stack::lifo — task command buffer
//
// Commands are pushed onto a LIFO stack.  The processor always handles the
// most recently pushed command first (useful for undo stacks, call stacks, etc.)
// overwrite_if_full=true so that new commands always fit — oldest discarded.
// ---------------------------------------------------------------------------
static void demo_lifo() {
    static stack::lifo<4, uint8_t> cmd_stack(/*overwrite_if_full=*/true);

    static stack::task<1024> push_task("lifo_push", 1, []() {
        static uint8_t cmd = 0;
        while (true) {
            cmd++;
            cmd_stack.push(cmd);
            Serial.printf("[lifo] pushed cmd=%u  depth=%lu\n",
                          cmd, cmd_stack.available());
            this_task::delay(400);
        }
    });

    static stack::task<1024> pop_task("lifo_pop", 1, []() {
        while (true) {
            uint8_t c = 0;
            if (cmd_stack.pop(c, 800)) {
                Serial.printf("[lifo] processed cmd=%u\n", c);
            }
            this_task::delay(800);
        }
    });

    push_task.resume();
    pop_task.resume();
}

// ---------------------------------------------------------------------------
// 4. heap::lifo — dynamically-allocated LIFO
// ---------------------------------------------------------------------------
static void demo_lifo_heap() {
    static heap::lifo<const char*> event_stack(/*capacity=*/3,
                                               /*overwrite_if_full=*/false);

    // Pre-populate the stack before the scheduler has other tasks running.
    event_stack.push("event_A");
    event_stack.push("event_B");
    event_stack.push("event_C");

    static stack::task<1024> drain("lifo_heap_drain", 1, []() {
        while (event_stack.available() > 0) {
            const char* ev = nullptr;
            if (event_stack.pop(ev, 0)) {
                Serial.printf("[lifo/heap] drained: %s\n", ev);
            }
        }
        Serial.println("[lifo/heap] stack empty");
        this_task::suspend();
    });
    drain.resume();
}

// ---------------------------------------------------------------------------
// 5. stack::singleton — "latest value" mailbox
//
// A singleton is a single-slot queue that silently discards the previous item
// when a new one is pushed.  The reader always gets the most recent state,
// not a historical queue of values.  Great for sharing sensor state, flags, etc.
// ---------------------------------------------------------------------------

struct sensor_state {
    float    temperature;
    uint16_t humidity;
    bool     alarm;
};

static void demo_singleton() {
    static stack::singleton<sensor_state> latest_state;

    // Sensor task — overwrites the mailbox every 300 ms.
    static stack::task<1024> sensor("sensor_writer", 1, []() {
        static float t = 20.0f;
        while (true) {
            t += 0.5f;
            sensor_state s = { t, static_cast<uint16_t>(60 + (int)t % 20), t > 30.0f };
            latest_state.push(s);       // always succeeds — no timeout needed
            this_task::delay(300);
        }
    });

    // UI task — reads the latest state every second; never sees stale history.
    static stack::task<1024> ui("ui_reader", 1, []() {
        while (true) {
            sensor_state s{};
            if (latest_state.pop(s, /*timeout_ms=*/1000)) {
                Serial.printf("[singleton] temp=%.1f°C  hum=%u%%  alarm=%d\n",
                              s.temperature, s.humidity, s.alarm);
            }
            this_task::delay(1000);
        }
    });

    sensor.resume();
    ui.resume();
}

// ---------------------------------------------------------------------------
// 6. push_from_isr / pop_from_isr
//
// Illustrates the ISR-safe API on a stack::fifo.  A FreeRTOS timer callback
// runs in a software interrupt context and pushes values via push_from_isr().
// The consumer task pops normally.
// ---------------------------------------------------------------------------
static void demo_isr_queue() {
    static stack::fifo<8, uint32_t> isr_queue;
    static uint32_t isr_counter = 0;

    // heap::timer with a callback that feeds the queue from ISR context.
    static heap::timer<> isr_feeder(
        "isr_feed",
        []() {
            isr_counter++;
            isr_queue.push_from_isr(isr_counter);   // non-blocking, ISR-safe
        },
        /*period_ms=*/250,
        /*auto_reload=*/true,
        /*auto_start=*/true);

    // Consumer drains the queue every 100 ms.
    static stack::task<1024> drainer("isr_drain", 1, []() {
        while (true) {
            uint32_t v = 0;
            while (isr_queue.pop(v, 0)) {           // drain all available items
                Serial.printf("[isr_queue] received from ISR: %lu\n", v);
            }
            this_task::delay(100);
        }
    });

    drainer.resume();
}

// ---------------------------------------------------------------------------
// setup()
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);

    demo_fifo_stack();
    demo_fifo_heap();
    demo_lifo();
    demo_lifo_heap();
    demo_singleton();
    demo_isr_queue();
}

void loop() {
    freertos::this_task::suspend();
}
