#pragma once

#include "constants.hpp"
#include "typedefs.hpp"
#include <functional>

namespace freertos {
    using namespace constants;
    using namespace typedefs;

    namespace abstract {

        /**
         * @brief Abstract base class that represents a FreeRTOS task.
         *
         * Provides the core task lifecycle API (`resume`, `suspend`, `join`) and
         * gives access to nested helper classes:
         *
         * - `task::info`     — read-only task diagnostics (priority, stack, state, name).
         * - `task::notifier` — type-safe task-notification API.
         * - `task::self`     — static utilities for the **currently running** task.
         *
         * Concrete tasks are created via `stack::task` (static allocation) or
         * `heap::task` (dynamic allocation); do not instantiate this class directly.
         */
        class task {
            public:

                /**
                 * @brief Provides read-only diagnostic information about a task.
                 *
                 * Obtain an instance via `abstract::task::get_info()` or
                 * `task::self::get_info()`.
                 */
                class info {
                    public:
                        /** @brief Possible states a FreeRTOS task can be in. */
                        enum class state {
                            ready     = eReady,     ///< Ready to run.
                            running   = eRunning,   ///< Currently executing.
                            blocked   = eBlocked,   ///< Waiting for an event or timeout.
                            suspended = eSuspended, ///< Explicitly suspended.
                            deleted   = eDeleted,   ///< Deleted, awaiting cleanup.

                            #if (tskKERNEL_VERSION_MAJOR > 8)
                                invalid = eInvalid  ///< Handle is invalid (kernel >= 9).
                            #else
                                invalid = eDeleted
                            #endif
                        };
                    private:
                        task_handle handle {nullptr};

                        friend class task;
                        friend class self;

                    public:
                        /** @brief Constructs an info object for the given handle. */
                        info(task_handle handle);

                        /** @brief Returns the task's current priority level. */
                        uint16_t get_priority(void) const;

                        /** @brief Returns the minimum free stack space ever recorded, in bytes. */
                        uint32_t get_free_stack_memory(void) const;

                        /** @brief Returns the current scheduler state of the task. */
                        state get_state(void) const;

                        /** @brief Returns the null-terminated name assigned at creation. */
                        const char* get_name(void) const;
                };

                /**
                 * @brief Sends notifications to a specific task.
                 *
                 * FreeRTOS task notifications are a lightweight alternative to
                 * semaphores/queues for simple signalling between tasks or from ISRs.
                 * Each method operates on a specific notification index (default 0)
                 * and optionally resumes the target task after notifying.
                 *
                 * Obtain an instance via `abstract::task::get_notifier()`.
                 */
                class notifier {

                    private:
                        task_handle handle {nullptr};
                        uint32_t last_value {0};

                        friend class task;
                    public:
                        /** @brief Constructs a notifier bound to the given task handle. */
                        notifier(task_handle handle);

                        /**
                         * @brief Sends a plain signal (increments the notification value by 1).
                         * @param index  Notification index (default 0).
                         * @param resume If true, resumes the target task after notifying.
                         * @return true on success.
                         */
                        bool signal(u_base_type index = 0, bool resume = true);

                        /** @brief ISR-safe variant of `signal()`. */
                        bool signal_from_isr(u_base_type index = 0, bool resume = true);

                        /**
                         * @brief ORs the given bitmask into the notification value.
                         * @param bits   Bitmask to set.
                         * @param index  Notification index.
                         * @param resume Resume target task after notifying.
                         * @return true on success.
                         */
                        bool set_bits(uint32_t bits, u_base_type index = 0, bool resume = true);

                        /** @brief ISR-safe variant of `set_bits()`. */
                        bool set_bits_from_isr(uint32_t bits, u_base_type index = 0, bool resume = true);

                        /**
                         * @brief Adds @p value to the current notification value.
                         * @param value  Amount to add.
                         * @param index  Notification index.
                         * @param resume Resume target task after notifying.
                         * @return true on success.
                         */
                        bool increment(uint32_t value, u_base_type index = 0, bool resume = true);

                        /** @brief ISR-safe variant of `increment()`. */
                        bool increment_from_isr(uint32_t value, u_base_type index = 0, bool resume = true);

                        /**
                         * @brief Overwrites the notification value unconditionally.
                         * @param value  New value.
                         * @param index  Notification index.
                         * @param resume Resume target task after notifying.
                         * @return true on success.
                         */
                        bool overwrite_value(uint32_t value, u_base_type index = 0, bool resume = true);

                        /** @brief ISR-safe variant of `overwrite_value()`. */
                        bool overwrite_value_from_isr(uint32_t value, u_base_type index = 0, bool resume = true);

                        /**
                         * @brief Sends @p value only if the previous notification has been consumed.
                         *
                         * If the target task has not yet read its previous notification, the
                         * send fails and returns false, preventing data loss.
                         *
                         * @param value  Value to send.
                         * @param index  Notification index.
                         * @param resume Resume target task after notifying.
                         * @return true if sent, false if a notification was already pending.
                         */
                        bool send_value(uint32_t value, u_base_type index = 0, bool resume = true);

                        /** @brief ISR-safe variant of `send_value()`. */
                        bool send_value_from_isr(uint32_t value, u_base_type index = 0, bool resume = true);

                        /**
                         * @brief Clears the pending notification.
                         * @param index  Notification index.
                         * @param resume Resume target task after clearing.
                         * @return true on success.
                         */
                        bool clear(u_base_type index = 0, bool resume = true);

                        /** @brief ISR-safe variant of `clear()`. */
                        bool clear_from_isr(u_base_type index = 0, bool resume = true);

                        /**
                         * @brief Returns the last value sent via any `send_value*` call.
                         * @return Cached last notification value.
                         */
                        uint32_t get_last_value(void) const;
                };

                /**
                 * @brief Static utilities operating on the **currently running** task.
                 *
                 * All methods are static; they always refer to the task that calls them.
                 * Aliased as `freertos::this_task` for convenience.
                 */
                class self {
                    public:
                        /** @brief Returns the handle of the currently running task. */
                        static task_handle get_handle(void);

                        /** @brief Suspends the currently running task indefinitely. */
                        static void suspend(void);

                        /** @brief Yields execution to another ready task of equal or higher priority. */
                        static void yield(void);

                        /**
                         * @brief Delays the current task for @p time_ms milliseconds.
                         * @param time_ms Sleep duration in milliseconds.
                         */
                        static void delay(uint32_t time_ms);

                        /**
                         * @brief Waits for a task notification and retrieves its value.
                         *
                         * Blocks for up to @p timeout_ms milliseconds.  On success the
                         * notification value is stored in @p notification_value and the
                         * pending flag is cleared.
                         *
                         * @param[out] notification_value  Receives the notification value.
                         * @param      index               Notification array index (default 0).
                         * @param      timeout_ms          Maximum wait time.
                         * @return true if a notification was received before the timeout.
                         */
                        static bool get_notification(uint32_t& notification_value, u_base_type index = 0, uint32_t timeout_ms = max_delay_ms);

                        /** @brief Returns diagnostic information about the current task. */
                        static info get_info(void);

                        /**
                         * @brief Delays the current task until a condition becomes true.
                         *
                         * Polls `condition(arguments)` every @p observer_delay_ms milliseconds.
                         * Returns early with false if @p timeout_ms elapses before the condition
                         * is satisfied.
                         *
                         * @tparam ARGUMENT_TYPE       Type of the argument passed to @p condition.
                         * @param  arguments           Argument forwarded to each `condition` call.
                         * @param  condition           Predicate; polling stops when this returns true.
                         * @param  observer_delay_ms   Polling interval in milliseconds (default 100).
                         * @param  timeout_ms          Maximum wait time (default `max_delay_ms`).
                         * @return true if the condition became true, false on timeout.
                         */
                        template <typename ARGUMENT_TYPE>
                        static bool delay_for(ARGUMENT_TYPE arguments, bool (*condition)(ARGUMENT_TYPE), uint32_t observer_delay_ms = 100, uint32_t timeout_ms = max_delay_ms){
                            if (condition == nullptr) {
                                return false;
                            }
                            
                            if (observer_delay_ms > timeout_ms || observer_delay_ms == 0) {
                                observer_delay_ms = 100;
                            }

                            uint32_t start_time = xTaskGetTickCount();
                            uint32_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);

                            while (!condition(arguments)) {
                                uint32_t current_time = xTaskGetTickCount();
                                uint32_t elapsed_ticks;
                                
                                if (current_time >= start_time) {
                                    elapsed_ticks = current_time - start_time;
                                } else {
                                    elapsed_ticks = (UINT32_MAX - start_time) + current_time + 1;
                                }
                                
                                if (timeout_ms != max_delay_ms && elapsed_ticks >= timeout_ticks) {
                                    return false;
                                }
                                vTaskDelay(pdMS_TO_TICKS(observer_delay_ms));
                            }

                            return true;
                        }

                        /**
                         * @brief Suspends the current task repeatedly while a condition holds.
                         *
                         * The task is suspended (via `vTaskSuspend`) on each iteration where
                         * `condition(arguments)` returns true.  Another task or ISR must resume
                         * it to re-evaluate the condition.
                         *
                         * @tparam ARGUMENT_TYPE Type of the argument passed to @p condition.
                         * @param  arguments     Argument forwarded to each `condition` call.
                         * @param  condition     Predicate; the task resumes when this returns false.
                         */
                        template <typename ARGUMENT_TYPE>
                        static void suspend_for(ARGUMENT_TYPE arguments, bool (*condition)(ARGUMENT_TYPE)){
                            while (condition != nullptr && condition(arguments)) {
                                vTaskSuspend(nullptr);
                            }
                        }

                };
            protected:
                task_handle handle {nullptr};
                task() = default;
            public:
                /** @brief Constructs from an existing FreeRTOS handle. */
                task(task_handle handle);

                /** @brief Deletes the underlying FreeRTOS task when the object is destroyed. */
                ~task();

                /**
                 * @brief Resumes a suspended task (task context).
                 * @return true if the task was successfully resumed.
                 */
                bool resume(void);

                /**
                 * @brief Resumes a suspended task (ISR context).
                 * @return true if the task was successfully resumed.
                 */
                bool resume_from_isr(void);

                /**
                 * @brief Suspends the task (task context).
                 * @return true if the task was successfully suspended.
                 */
                bool suspend(void);

                /**
                 * @brief Suspends the task (ISR context).
                 * @return true if the task was successfully suspended.
                 */
                bool suspend_from_isr(void);

                /**
                 * @brief Returns true if the task is currently running (not suspended or deleted).
                 */
                bool is_running(void);

                /**
                 * @brief Blocks the caller until this task has fully stopped.
                 *
                 * Polls the task state every @p observer_delay_ms milliseconds until it
                 * is no longer in the running/ready/blocked state, or until @p timeout_ms
                 * elapses.
                 *
                 * @param observer_delay_ms Polling interval in milliseconds (default 100).
                 * @param timeout_ms        Maximum wait time (default `max_delay_ms`).
                 * @return true if the task stopped within the timeout, false otherwise.
                 */
                bool join(uint32_t observer_delay_ms = 100, uint32_t timeout_ms = max_delay_ms);

                /**
                 * @brief Returns true if the task handle is valid (task was created successfully).
                 */
                bool is_valid(void);

                /**
                 * @brief Returns a `notifier` object bound to this task.
                 *
                 * Use the returned notifier to send notifications from other tasks or ISRs.
                 */
                notifier get_notifier(void);

                /** @brief Returns diagnostic information about this task. */
                info get_info(void);

                /**
                 * @brief Returns a reference to the raw FreeRTOS task handle.
                 *
                 * Use with caution; direct manipulation bypasses the class invariants.
                 */
                task_handle& get_handle(void);
        };
    }

    namespace stack {

        /**
         * @brief Statically-allocated FreeRTOS task with a typed argument.
         *
         * The task stack and control block are stored as member variables so that
         * no heap allocation is required.  The stack size is a compile-time
         * template parameter (in words, not bytes).
         *
         * After construction the task is created but **suspended**; call `resume()`
         * to start execution.  When the callback returns the task suspends itself
         * automatically and can be re-started with another `resume()`.
         *
         * @tparam STACK_SIZE    Stack depth in words (e.g. 1024 = 4 KiB on 32-bit).
         * @tparam ARGUMENT_TYPE Type of the single argument passed to the callback.
         *                       Specialised for `void` when no argument is needed.
         *
         * @code
         * static freertos::stack::task<1024, int&> my_task(
         *     "my_task", 1, my_value,
         *     [](int& v){ while(1){ v++; freertos::this_task::delay(100); } });
         * my_task.resume();
         * @endcode
         */
        template <uint32_t STACK_SIZE, typename ARGUMENT_TYPE = void>
        class task;

        template <uint32_t STACK_SIZE, typename ARGUMENT_TYPE>
        class task : public abstract::task {
            private:
                task_stack stack[STACK_SIZE];
                task_struct buffer;
                ARGUMENT_TYPE arguments;
                void (*callback)(ARGUMENT_TYPE);

                static void wrapper(void* arguments) {
                    if (arguments == nullptr) {
                        return;
                    }

                    task& self = *static_cast<task*>(arguments);

                    while (1) {
                        if (self.callback != nullptr) {
                            self.callback(self.arguments);
                        }

                        vTaskSuspend(nullptr);
                    }
                }

            public:
                /**
                 * @brief Creates the task with a typed argument.
                 * @param name     Null-terminated task name (up to `task_name_length` chars).
                 * @param priority Task priority (0 = lowest, `max_priority` = highest).
                 * @param param    Argument forwarded to every invocation of @p callback.
                 * @param callback Function executed by the task; receives @p param by value.
                 */
                task(const char* name, uint16_t priority, ARGUMENT_TYPE param, void (*callback)(ARGUMENT_TYPE)) :
                abstract::task(),
                arguments(param),
                callback(callback)
                {
                    this->handle = xTaskCreateStatic(wrapper, name, STACK_SIZE, this, priority, stack, &buffer);
                }

                #if defined(ESP32)
                /**
                 * @brief ESP32-only: creates the task pinned to a specific core.
                 * @param core_id CPU core to pin the task to (0 or 1).
                 */
                task(const char* name, uint16_t priority, ARGUMENT_TYPE param, void (*callback)(ARGUMENT_TYPE), uint8_t core_id) :
                abstract::task(),
                arguments(param),
                callback(callback)
                {
                    this->handle = xTaskCreateStaticPinnedToCore(wrapper, name, STACK_SIZE, this, priority, stack, &buffer, core_id);
                }
                #endif

                using abstract::task::resume;
                using abstract::task::suspend;
                using abstract::task::is_running;
                using abstract::task::join;
                using abstract::task::is_valid;
                using abstract::task::get_notifier;
                using abstract::task::get_handle;
                using abstract::task::get_info;
        };

        /** @brief `void`-argument specialisation of `stack::task` (no argument passed to callback). */
        template <uint32_t STACK_SIZE>
        class task<STACK_SIZE, void> : public abstract::task {
            private:
                task_stack stack[STACK_SIZE];
                task_struct buffer;

                void (*callback)(void);

                static void wrapper(void* arguments) {
                    if (arguments == nullptr) {
                        return;
                    }

                    task& self = *static_cast<task*>(arguments);

                    while (1) {
                        if (self.callback != nullptr) {
                            self.callback();
                        }
                        
                        vTaskSuspend(nullptr);
                    }
                }

            public:
                /**
                 * @brief Creates the task with no argument.
                 * @param name     Null-terminated task name.
                 * @param priority Task priority.
                 * @param callback Function executed by the task; takes no parameters.
                 */
                task(const char* name, uint16_t priority, void (*callback)(void)) :
                abstract::task(),
                callback(callback)
                {
                    this->handle = xTaskCreateStatic(wrapper, name, STACK_SIZE, this, priority, stack, &buffer);
                }

                #if defined(ESP32)
                /**
                 * @brief ESP32-only: creates the task pinned to a specific core.
                 * @param core_id CPU core to pin the task to (0 or 1).
                 */
                task(const char* name, uint16_t priority, void (*callback)(void), uint8_t core_id) :
                abstract::task(),
                callback(callback)
                {
                    this->handle = xTaskCreateStaticPinnedToCore(wrapper, name, STACK_SIZE, this, priority, stack, &buffer, core_id);
                }
                #endif
                
                using abstract::task::resume;
                using abstract::task::suspend;
                using abstract::task::is_running;
                using abstract::task::join;
                using abstract::task::is_valid;
                using abstract::task::get_notifier;
                using abstract::task::get_handle;
                using abstract::task::get_info;
        };
    }

    namespace heap {

        /**
         * @brief Heap-allocated FreeRTOS task with a typed argument.
         *
         * The task stack is allocated on the heap at construction time using
         * `xTaskCreate()`.  Use this when the stack size is not known at compile
         * time, or when static allocation is not required.
         *
         * For the `void` specialisation the callback is stored as a
         * `std::function<void()>`, enabling lambdas that capture variables by
         * reference or value — a natural fit since heap allocation is already
         * taking place.
         *
         * @tparam ARGUMENT_TYPE Type of the single argument passed to the callback.
         *                       Defaults to `void` (lambda-friendly, supports captures).
         */
        template <typename ARGUMENT_TYPE = void>
        class task;

        template <typename ARGUMENT_TYPE>
        class task : public abstract::task {
            private:
                ARGUMENT_TYPE arguments;
                void (*callback)(ARGUMENT_TYPE);

                static void wrapper(void* arguments) {
                    if (arguments == nullptr) {
                        return;
                    }

                    task& self = *static_cast<task*>(arguments);

                    while (1) {
                        if (self.callback != nullptr) {
                            self.callback(self.arguments);
                        }

                        vTaskSuspend(nullptr);
                    }
                }

            public:
                task(const char* name, uint16_t priority, uint32_t stack_size, ARGUMENT_TYPE param, void (*callback)(ARGUMENT_TYPE)) :
                abstract::task(),
                arguments(param), 
                callback(callback) 
                {
                    xTaskCreate(wrapper, name, stack_size, this, priority, &this->handle);
                }

                #if defined(ESP32)
                    task(const char* name, uint16_t priority, uint32_t stack_size, ARGUMENT_TYPE param, void (*callback)(ARGUMENT_TYPE), uint8_t core_id) :
                    abstract::task(),
                    arguments(param),
                    callback(callback)
                    {
                        xTaskCreatePinnedToCore(wrapper, name, stack_size, this, priority, &this->handle, core_id);
                    }
                #endif

                using abstract::task::resume;
                using abstract::task::suspend;
                using abstract::task::is_running;
                using abstract::task::join;
                using abstract::task::is_valid;
                using abstract::task::get_notifier;
                using abstract::task::get_handle;
                using abstract::task::get_info;
        };

        /**
         * @brief `void`-argument specialisation of `heap::task`.
         *
         * Unlike `stack::task`, this specialisation stores the callback as a
         * `std::function<void()>` instead of a plain function pointer.  This is
         * an intentional design choice that leverages the fact that a heap
         * allocation is **already** required for the task stack — so the extra
         * allocation made by `std::function` to store a closure is not an
         * architectural change, just an additional (small) heap block.
         *
         * **Benefit:** lambdas that capture local or static variables by reference
         * or by value work without any boilerplate:
         * @code
         * int shared = 0;
         * static freertos::heap::task<> t("t", 1, 2048, [&]() {
         *     while (true) { use(shared); freertos::this_task::delay(100); }
         * });
         * @endcode
         *
         * **Trade-offs and caveats:**
         *
         * 1. **Extra heap allocation.** `std::function` performs a small heap
         *    allocation when storing a closure that exceeds its internal SBO
         *    (Small Buffer Optimisation) threshold — typically ~16–32 bytes
         *    depending on the standard library.  On systems with very limited heap
         *    this can matter.
         *
         * 2. **Indirect call overhead.** Invoking a `std::function` goes through
         *    an extra level of indirection compared to a plain function pointer.
         *    For a task callback called once per wake-up cycle the cost is
         *    negligible, but it is non-zero.
         *
         * 3. **Dangling references.** If the lambda captures a variable **by
         *    reference**, the captured reference must remain valid for the entire
         *    lifetime of the task.  Capturing a local variable from `setup()` or
         *    from a function that returns before the task is deleted will cause
         *    undefined behaviour.  Prefer `static` storage or heap-allocated
         *    objects for captured variables.
         *
         * 4. **Not available for `stack::task`.** Stack tasks use plain function
         *    pointers precisely to avoid any hidden heap allocation.  If you need
         *    context, pass it as the typed `ARGUMENT_TYPE` parameter instead.
         */
        template <>
        class task<void> : public abstract::task {
            private:
                std::function<void()> callback;

                static void wrapper(void* arguments) {
                    if (arguments == nullptr) {
                        return;
                    }

                    task& self = *static_cast<task*>(arguments);

                    while (1) {
                        if (self.callback != nullptr) {
                            self.callback();
                        }

                        vTaskSuspend(nullptr);
                    }
                }

            public:
                /**
                 * @brief Creates the heap-allocated task with no argument.
                 * @param name       Null-terminated task name.
                 * @param priority   Task priority.
                 * @param stack_size Stack depth in words.
                 * @param callback   Callable (function pointer, lambda with captures, etc.).
                 */
                task(const char* name, uint16_t priority, uint32_t stack_size, std::function<void()> callback) :
                abstract::task(),
                callback(callback)
                {
                    xTaskCreate(wrapper, name, stack_size, this, priority, &this->handle);
                }

                #if defined(ESP32)
                /**
                 * @brief ESP32-only: creates the heap-allocated task pinned to a specific core.
                 * @param core_id CPU core to pin the task to (0 or 1).
                 */
                task(const char* name, uint16_t priority, uint32_t stack_size, std::function<void()> callback, uint8_t core_id) :
                abstract::task(),
                callback(callback)
                {
                    xTaskCreatePinnedToCore(wrapper, name, stack_size, this, priority, &this->handle, core_id);
                }
                #endif

                using abstract::task::resume;
                using abstract::task::suspend;
                using abstract::task::is_running;
                using abstract::task::join;
                using abstract::task::is_valid;
                using abstract::task::get_notifier;
                using abstract::task::get_handle;
                using abstract::task::get_info;
        };
    }
    
    using this_task = abstract::task::self;
}

