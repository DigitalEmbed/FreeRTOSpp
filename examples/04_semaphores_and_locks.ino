/**
 * @file 04_semaphores_and_locks.cpp
 * @brief All semaphore types and RAII lock guards.
 *
 * This example demonstrates:
 *  - stack::binary   — signalling between a producer and a consumer task.
 *  - stack::counting — modelling a pool of N identical resources.
 *  - stack::mutex    — protecting shared state with a lock_guard (RAII).
 *  - stack::recursive — re-entrant locking for a recursive function.
 *  - abstract::semaphore& — polymorphic function that works with any semaphore type.
 *  - heap::mutex     — dynamic-allocation alternative to stack::mutex.
 *
 * Target: ESP32 / STM32 with Arduino framework.
 */

#include <Arduino.h>
#include "freertos.hpp"

using namespace freertos;

// ---------------------------------------------------------------------------
// 1. Binary semaphore — producer / consumer
//
// The producer fires every 500 ms and signals the consumer via give().
// The consumer blocks on take() indefinitely until the signal arrives.
// ---------------------------------------------------------------------------
static void demo_binary() {
    static stack::binary signal_sem;

    // Consumer — blocks waiting for each signal from the producer.
    static stack::task<1024> consumer("consumer", 1, []() {
        while (true) {
            if (signal_sem.take(/*timeout_ms=*/max_delay_ms)) {
                Serial.println("[binary] consumer: signal received");
            }
        }
    });

    // Producer — gives the semaphore every 500 ms.
    static stack::task<1024> producer("producer", 1, []() {
        while (true) {
            this_task::delay(500);
            Serial.println("[binary] producer: sending signal");
            signal_sem.give();
        }
    });

    consumer.resume();
    producer.resume();
}

// ---------------------------------------------------------------------------
// 2. Counting semaphore — resource pool
//
// Three identical "resources" are modelled as a counting semaphore initialised
// to 3.  Four worker tasks compete for them; at most three run concurrently.
// ---------------------------------------------------------------------------
static void demo_counting() {
    // max_count = 3 resources, initial_count = 3 (all available at start).
    static stack::counting<3, 3> pool;

    static stack::task<1024> w1("worker_1", 1, []() {
        while (true) {
            pool.take();                          // acquire one slot
            Serial.println("[counting] worker_1: using resource");
            this_task::delay(800);
            pool.give();                          // release slot
            Serial.println("[counting] worker_1: released resource");
            this_task::delay(200);
        }
    });

    static stack::task<1024> w2("worker_2", 1, []() {
        while (true) {
            pool.take();
            Serial.println("[counting] worker_2: using resource");
            this_task::delay(600);
            pool.give();
            this_task::delay(400);
        }
    });

    static stack::task<1024> w3("worker_3", 1, []() {
        while (true) {
            pool.take();
            Serial.println("[counting] worker_3: using resource");
            this_task::delay(400);
            pool.give();
            this_task::delay(600);
        }
    });

    // Worker 4 will block if all 3 resources are taken.
    static stack::task<1024> w4("worker_4", 1, []() {
        while (true) {
            if (pool.take(/*timeout_ms=*/300)) {
                Serial.println("[counting] worker_4: using resource");
                this_task::delay(200);
                pool.give();
            } else {
                Serial.println("[counting] worker_4: no resource available — retrying");
            }
            this_task::delay(300);
        }
    });

    w1.resume();
    w2.resume();
    w3.resume();
    w4.resume();
}

// ---------------------------------------------------------------------------
// 3. Mutex + lock_guard — RAII critical section
//
// Two tasks share a counter.  Each access is guarded by a lock_guard so that
// the mutex is always released even if the task returns early.
// ---------------------------------------------------------------------------
static void demo_mutex() {
    static stack::mutex mtx;
    static int shared_counter = 0;

    static stack::task<1024> inc_task("inc", 1, []() {
        while (true) {
            {
                lock_guard guard(mtx);          // locks here
                if (!guard.is_locked()) {
                    // Mutex take timed out — skip this cycle.
                    this_task::delay(50);
                    continue;
                }
                shared_counter++;
                Serial.printf("[mutex] counter = %d (incremented)\n", shared_counter);
            }                                   // mutex released here
            this_task::delay(300);
        }
    });

    static stack::task<1024> read_task("read", 1, []() {
        while (true) {
            {
                lock_guard guard(mtx);
                if (guard.is_locked()) {
                    Serial.printf("[mutex] counter = %d (read)\n", shared_counter);
                }
            }
            this_task::delay(700);
        }
    });

    inc_task.resume();
    read_task.resume();
}

// ---------------------------------------------------------------------------
// 4. Recursive mutex — re-entrant function
//
// log_message() acquires the mutex before writing.  It can call itself
// recursively (or be called from nested functions that also hold the lock)
// without deadlocking, because the same task can take it multiple times.
// ---------------------------------------------------------------------------
static stack::recursive recursive_mtx;

static void log_message(const char* msg, int depth) {
    lock_guard guard(recursive_mtx);        // re-entrant — safe to nest
    if (!guard.is_locked()) return;

    Serial.printf("[recursive] depth=%d  %s\n", depth, msg);

    if (depth > 0) {
        log_message("nested call", depth - 1);   // still holds the mutex here
    }
}

static void demo_recursive() {
    static stack::task<2048> task("recursive_demo", 1, []() {
        while (true) {
            log_message("top level", 3);    // acquires / nests 4 times total
            this_task::delay(2000);
        }
    });
    task.resume();
}

// ---------------------------------------------------------------------------
// 5. Polymorphism — function that accepts any abstract::semaphore
//
// print_sem_state() works with stack::mutex, stack::binary,
// heap::mutex, heap::binary — any subclass of abstract::semaphore.
// ---------------------------------------------------------------------------
static void print_sem_state(abstract::semaphore& sem, const char* label) {
    Serial.printf("[poly] %-20s valid=%d  supports_isr=%d\n",
                  label, sem.is_valid(), sem.supports_isr());
}

static void demo_polymorphism() {
    static stack::binary  s_bin;
    static stack::mutex   s_mtx;
    static heap::binary   h_bin;
    static heap::mutex    h_mtx;

    static stack::task<1024> task("poly_demo", 1, []() {
        print_sem_state(s_bin, "stack::binary");
        print_sem_state(s_mtx, "stack::mutex");
        print_sem_state(h_bin, "heap::binary");
        print_sem_state(h_mtx, "heap::mutex");
        this_task::suspend();       // run once, then stop
    });
    task.resume();
}

// ---------------------------------------------------------------------------
// setup()
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);

    demo_binary();
    demo_counting();
    demo_mutex();
    demo_recursive();
    demo_polymorphism();
}

void loop() {
    freertos::this_task::suspend();
}
