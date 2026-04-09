# FreeRTOS++

A modern C++ wrapper library for FreeRTOS targeting embedded systems (STM32, ESP32 and any FreeRTOS-compatible platform). It replaces raw C FreeRTOS calls with type-safe, RAII-based abstractions using object-oriented design patterns and C++11/14/17 features.

---

## Features

- **Type-safe API** — strong typedefs, templated collections and tasks eliminate common casting errors.
- **RAII resource management** — semaphores, queues and tasks are automatically released when their objects go out of scope.
- **Dual allocation strategy** — every primitive comes in a `stack::` (static allocation, no heap) and a `heap::` (dynamic allocation) variant.
- **ISR-safe variants** — all blocking primitives expose `_from_isr` methods for safe use in interrupt handlers.
- **Task notification API** — lightweight signalling without queues or semaphores.
- **Zero-cost abstractions** — templates are resolved at compile time; no virtual dispatch overhead in the hot path.
- **Backward compatible** — preprocessor guards cover FreeRTOS kernel versions 8 through 10+.

---

## Getting Started

1. Copy (or symlink) the `src/` directory into your project.
2. Add `src/` to your include path.
3. Include the single aggregator header:

```cpp
#include "freertos.hpp"
```

Everything lives in the `freertos::` namespace.

---

## Namespace Overview

| Namespace | Purpose |
|---|---|
| `freertos::abstract::` | Base classes — not instantiated directly |
| `freertos::stack::` | Statically-allocated variants (no heap) |
| `freertos::heap::` | Dynamically-allocated variants |
| `freertos::constants::` | FreeRTOS config constants (`max_delay_ms`, etc.) |
| `freertos::typedefs::` | C++ aliases for FreeRTOS C types |
| `freertos::this_task` | Utilities for the currently running task |
| `freertos::this_app` | Alias for `this_task` inside `app` subclasses |

---

## Core Concepts

### Namespace Design

The three top-level namespaces reflect **allocation strategy** and **usage intent**:

| Namespace | Allocation | Purpose |
|---|---|---|
| `freertos::stack::` | **Static** — lives in BSS, zero heap usage | Production firmware; deterministic, no fragmentation risk |
| `freertos::heap::` | **Dynamic** — allocated via `pvPortMalloc` | When size or lifetime is not known at compile time |
| `freertos::abstract::` | N/A — base classes only | Polymorphic handles; lets code accept any variant without caring about allocation |

The `abstract::` classes are never instantiated directly. Their value is that you can write functions that receive an `abstract::semaphore&` and work with both a `stack::mutex` and a `heap::binary` — the allocation detail is hidden behind the base interface.

```cpp
// Works with stack::mutex, heap::mutex, stack::binary, heap::binary — any semaphore
void enter_critical(freertos::abstract::semaphore& sem) {
    sem.take();
}

freertos::stack::mutex  static_mutex;   // allocated in BSS — no heap
freertos::heap::mutex   dynamic_mutex;  // allocated via pvPortMalloc

enter_critical(static_mutex);
enter_critical(dynamic_mutex);
```

### Tasks

Tasks are created suspended; call `resume()` to start execution. When the callback returns the task suspends itself automatically and can be restarted.

```cpp
// Stack task — STACK_SIZE is in words, not bytes
static freertos::stack::task<1024> blink_task("blink", 1, []() {
    while (true) {
        toggle_led();
        freertos::this_task::delay(500);
    }
});
blink_task.resume();
```

Pass a typed argument to the task callback:

```cpp
static int counter = 0;
static freertos::stack::task<1024, int&> counter_task("counter", 1, counter,
    [](int& c) {
        while (true) {
            c++;
            freertos::this_task::delay(100);
        }
    });
counter_task.resume();
```

#### `heap::task<void>` — lambdas with captures

`heap::task<void>` is the only variant that stores the callback as a
`std::function<void()>`, which means lambdas that **capture** variables work
out of the box:

```cpp
static int sensor_value = 0;

static freertos::heap::task<> reader("reader", 1, 2048, [&]() {
    while (true) {
        sensor_value = read_adc();
        freertos::this_task::delay(50);
    }
});
reader.resume();
```

This is an intentional design choice: `heap::task` already performs a heap
allocation for the task stack, so the small extra allocation that `std::function`
may make for the closure is not an architectural change.

**`stack::task` uses plain function pointers** — no hidden allocations, but no
captures either. Pass context through the `ARGUMENT_TYPE` template parameter
instead.

**Caveats when using `heap::task` with captures:**

| Risk | Detail |
|---|---|
| Extra heap allocation | `std::function` may allocate a separate heap block for closures that exceed its internal SBO buffer (~16–32 bytes). On systems with very limited heap this can matter. |
| Indirect call overhead | Calling through `std::function` adds one extra level of indirection compared to a plain function pointer. Negligible for task callbacks, but non-zero. |
| Dangling references | A lambda that captures a variable **by reference** (`[&]`) will cause undefined behaviour if the captured object is destroyed before the task is deleted. Always capture `static` or heap-allocated objects. |

#### `this_task` utilities

| Method | Description |
|---|---|
| `this_task::delay(ms)` | Sleep for `ms` milliseconds |
| `this_task::suspend()` | Suspend the current task indefinitely |
| `this_task::yield()` | Yield to another ready task |
| `this_task::get_handle()` | Get the current task's FreeRTOS handle |
| `this_task::get_info()` | Get priority, stack usage and state |
| `this_task::get_notification(val, idx, timeout)` | Wait for a task notification |
| `this_task::delay_for(arg, cond, poll_ms, timeout)` | Poll a predicate, sleeping between checks |
| `this_task::suspend_for(arg, cond)` | Suspend while a predicate holds |

### Synchronisation Primitives

All primitives inherit from `abstract::semaphore` and share the same interface:

| Method | Description |
|---|---|
| `take(timeout_ms)` | Acquire (blocks up to timeout) |
| `give()` | Release |
| `take_from_isr()` | Non-blocking acquire from ISR |
| `give_from_isr()` | Release from ISR |
| `is_valid()` | Check if creation succeeded |

| Type | Class | Notes |
|---|---|---|
| Binary semaphore | `stack::binary` / `heap::binary` | Two states: available / unavailable. Supports ISR. |
| Counting semaphore | `stack::counting` / `heap::counting` | Integer counter [0, max]. Supports ISR. |
| Mutex | `stack::mutex` / `heap::mutex` | Priority inheritance. **No ISR support.** |
| Recursive mutex | `stack::recursive` / `heap::recursive` | Re-entrant mutex. **No ISR support.** |

### Lock Guards (RAII)

**Task context:**

```cpp
freertos::stack::mutex mtx;

void critical_section() {
    freertos::lock_guard guard(mtx);        // locks here
    if (!guard.is_locked()) return;         // check for timeout
    // ... protected code ...
}                                           // unlocks automatically
```

**ISR context:**

```cpp
// The callback runs inside the critical section
freertos::lock_guard_from_isr<int> guard(binary_sem, shared_value,
    [](int& v) { v++; });
```

### Collections (Queues)

All collections inherit from `abstract::collection<DATA_TYPE>` and expose:

| Method | Description |
|---|---|
| `push(item, timeout_ms)` | Insert an item |
| `pop(item, timeout_ms)` | Remove and return an item |
| `push_from_isr(item)` | ISR-safe insert |
| `pop_from_isr(item)` | ISR-safe remove |
| `available()` | Number of items currently stored |
| `get_free_space()` | Number of empty slots |
| `is_empty()` / `is_full()` | Capacity checks |
| `clear()` | Flush all items |

| Type | Ordering | Class |
|---|---|---|
| FIFO queue | First-in, first-out | `stack::fifo<N, T>` / `heap::fifo<T>` |
| LIFO stack | Last-in, first-out | `stack::lifo<N, T>` / `heap::lifo<T>` |
| Singleton | Single slot (always latest) | `stack::singleton<T>` |

```cpp
// Stack-allocated FIFO of capacity 8, holding uint32_t
freertos::stack::fifo<8, uint32_t> event_queue;
event_queue.push(42);

uint32_t val;
if (event_queue.pop(val, 100)) {   // wait up to 100 ms
    process(val);
}
```

### Shared Data

Thread-safe wrappers that combine a value with an internal lock.

```cpp
// Accessible from tasks only
freertos::stack::shared_data<int> counter(0);
counter.set(10);
int val = counter.get();
counter.use([](int& v) { v += 5; });    // atomic read-modify-write

// Also accessible from ISRs (uses binary semaphore internally)
freertos::stack::shared_data_with_isr<int> isr_counter;
isr_counter.set_from_isr(99);
isr_counter.use_from_isr([](int& v) { v++; });
```

### Timers

```cpp
// Periodic stack timer with argument
static int ticks = 0;
static freertos::stack::timer<int&> periodic("tick", ticks,
    [](int& t) { t++; },
    /*period_ms=*/1000, /*auto_reload=*/true);
periodic.start();

// One-shot heap timer, no argument
static freertos::heap::timer<> once("once",
    []() { do_something(); },
    /*period_ms=*/500, /*auto_reload=*/false, /*auto_start=*/true);
```

| Method | Description |
|---|---|
| `start(delay_ms)` | Start the timer |
| `stop(delay_ms)` | Stop the timer |
| `reset(delay_ms)` | Restart the countdown |
| `set_period(ms)` | Change the period at runtime |
| `get_period_ms()` | Read the current period |
| `is_running()` | Check if actively counting |

### Application Framework

`abstract::app` wraps a task into a self-contained application object with a clean `start` / `stop` lifecycle.  Use the **singleton pattern** to ensure the task storage outlives the scheduler.

```cpp
class my_app : public freertos::abstract::app {
    static constexpr uint32_t STACK = 4096;
    freertos::stack::task<STACK, my_app&> task;

    static void run(my_app& self);
    my_app() : freertos::abstract::app(task),
               task("my_app", 1, *this, my_app::run) {}
public:
    static my_app& instance() { static my_app i; return i; }
};

void my_app::run(my_app& self) {
    while (true) {
        // application logic
        freertos::this_app::delay(100);
    }
}

// In setup():
my_app::instance().start();
```

### Task Notifications

A lightweight way to send a 32-bit value between tasks, avoiding semaphore overhead.

```cpp
// Receiver task
static freertos::stack::task<1024> receiver("rx", 1, []() {
    while (true) {
        uint32_t value = 0;
        if (freertos::this_task::get_notification(value, 0, 1000)) {
            handle(value);
        }
    }
});

// Sender task — obtain notifier from the receiver task object
receiver.get_notifier().send_value(42);
```

| Notifier method | Behaviour |
|---|---|
| `signal()` | Increment notification value by 1 |
| `set_bits(mask)` | OR bitmask into notification value |
| `increment(n)` | Add `n` to notification value |
| `overwrite_value(v)` | Unconditionally set value |
| `send_value(v)` | Set value only if no pending notification |
| `clear()` | Clear pending notification |

All methods have `_from_isr` variants.

### System Utilities

```cpp
freertos::system::get_tick_count();         // raw tick counter
freertos::system::get_milliseconds();       // ticks → milliseconds
freertos::system::get_amount_of_tasks();    // number of active tasks
```

---

## Examples

See the [`examples/`](examples/) directory for fully annotated code:

| Example | Covers |
|---|---|
| [`01_basic_tasks`](examples/01_basic_tasks.ino) | `stack::task`, `heap::task` (lambda capture), `stack::lifo`, `stack::shared_data`, `stack/heap::timer`, `abstract::app` |
| [`02_isr_shared_data`](examples/02_isr_shared_data.ino) | Multiple `abstract::app` singletons, `shared_data_with_isr`, hardware timer ISR |
| [`03_task_notifications`](examples/03_task_notifications.ino) | `send_value`, `get_notification`, `get_info` / stack monitoring |
| [`04_semaphores_and_locks`](examples/04_semaphores_and_locks.ino) | `stack::binary`, `stack::counting`, `stack::mutex` + `lock_guard`, `stack::recursive`, `abstract::semaphore&` polymorphism, `heap::mutex` |
| [`05_queues_and_collections`](examples/05_queues_and_collections.ino) | `stack::fifo`, `heap::fifo`, `stack::lifo`, `heap::lifo`, `stack::singleton`, `push/pop_from_isr` |
| [`06_advanced_tasks`](examples/06_advanced_tasks.ino) | `delay_for`, `suspend_for`, `yield`, `join`, `system::*`, `heap::shared_data`, all notifier modes (`signal`, `set_bits`, `increment`, `overwrite_value`, `send_value`, `clear`), notifier ISR variants |
| [`07_event_groups`](examples/07_event_groups.ino) | `stack::event_group`, `operator[]`, `<<`/`>>` proxy operators, `set()`/`get()`, `set_from_isr()`/`get_from_isr()`, range-based for, bounds enforcement |

---

## Roadmap

- **Stream Buffers** — single-producer / single-consumer byte stream with variable-length reads and writes (`stack::stream_buffer`, `heap::stream_buffer`).
- **Message Buffers** — built on top of stream buffers; preserves discrete message boundaries (`stack::message_buffer`, `heap::message_buffer`).

---

## Compatibility

| Platform | Status |
|---|---|
| ESP32 (Arduino / ESP-IDF) | Fully supported; core-pinning constructors available |
| STM32 (FreeRTOS via CubeMX) | Fully supported |
| Any FreeRTOS >= 8 | Supported (some features require kernel >= 10) |

---

## License

See [LICENSE](LICENSE).
