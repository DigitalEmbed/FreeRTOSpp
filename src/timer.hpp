#pragma once

#include "constants.hpp"
#include "typedefs.hpp"

namespace freertos {
    using namespace typedefs;
    using namespace constants;

    namespace abstract {

        /**
         * @brief Abstract base class for FreeRTOS software timers.
         *
         * A software timer calls a user-supplied callback function after a
         * configurable period.  Timers can be one-shot (fires once) or
         * auto-reloading (periodic).
         *
         * Concrete timers are created via `stack::timer` or `heap::timer`.
         * The destructor automatically deletes the underlying FreeRTOS timer.
         *
         * @note Software timer callbacks run in the context of the FreeRTOS
         *       timer daemon task.  Keep callbacks short and non-blocking.
         */
        class timer {
            protected:
                timer_handle handle;
                timer() = default;
            public:
                /** @brief Constructs from an existing FreeRTOS timer handle. */
                timer(timer_handle handle);

                /** @brief Destroys the timer and frees the FreeRTOS resource. */
                ~timer();

                /**
                 * @brief Starts the timer.
                 * @param delay_ms Optional command queue delay in milliseconds (default 0).
                 * @return true if the start command was posted successfully.
                 */
                bool start(uint32_t delay_ms = 0);

                /** @brief Starts the timer from an ISR context. */
                bool start_from_isr(void);

                /**
                 * @brief Stops the timer.
                 * @param delay_ms Optional command queue delay in milliseconds (default 0).
                 * @return true if the stop command was posted successfully.
                 */
                bool stop(uint32_t delay_ms = 0);

                /** @brief Stops the timer from an ISR context. */
                bool stop_from_isr(void);

                /**
                 * @brief Resets (restarts) the timer from its current period.
                 * @param delay_ms Optional command queue delay in milliseconds (default 0).
                 * @return true if the reset command was posted successfully.
                 */
                bool reset(uint32_t delay_ms = 0);

                /** @brief Resets the timer from an ISR context. */
                bool reset_from_isr(void);

                /**
                 * @brief Changes the timer period.
                 * @param period_ms New period in milliseconds.
                 * @return true if the command was posted successfully.
                 */
                bool set_period(uint32_t period_ms);

                /** @brief Changes the timer period from an ISR context. */
                bool set_period_from_isr(uint32_t period_ms);

                /** @brief Returns the current timer period in milliseconds. */
                uint32_t get_period_ms(void);

                /** @brief Returns the absolute expiration time in milliseconds. */
                uint32_t get_expiration_time_ms(void);

                /** @brief Returns true if the timer is currently active (counting). */
                bool is_running(void);

                /** @brief Returns true if the underlying FreeRTOS timer was created successfully. */
                bool is_valid(void);

                /**
                 * @brief Returns a reference to the raw FreeRTOS timer handle.
                 *
                 * Use with caution; direct manipulation bypasses class invariants.
                 */
                timer_handle& get_handle(void);

                #if (tskKERNEL_VERSION_MAJOR > 8)
                    /** @brief Returns true if the timer is set to auto-reload (periodic). */
                    bool get_auto_reload(void);

                    /**
                     * @brief Sets the auto-reload mode.
                     * @param auto_reload true for periodic, false for one-shot.
                     * @return true on success.
                     */
                    bool set_auto_reload(bool auto_reload);
                #endif
        };
    }

    namespace heap {
        /**
         * @brief Heap-allocated software timer with a typed callback argument.
         *
         * The underlying FreeRTOS timer object is allocated on the heap via
         * `xTimerCreate()`.
         *
         * @tparam ARGUMENT_TYPE Type of the argument forwarded to the callback.
         *                       Defaults to `void` (no argument).
         */
        template <typename ARGUMENT_TYPE = void>
        class timer;

        template <typename ARGUMENT_TYPE>
        class timer : public abstract::timer {
            private:
                void (*callback)(ARGUMENT_TYPE);
                ARGUMENT_TYPE argument;

                static void callback_wrapper(TimerHandle_t handle){
                    timer& self = *static_cast<timer*>(pvTimerGetTimerID(handle));
                    if (self.callback != nullptr) {
                        self.callback(self->argument);
                    }
                }

            public:
                timer(const char* name, ARGUMENT_TYPE argument, void (*callback)(ARGUMENT_TYPE), uint32_t period_ms = 0, bool auto_reload = false, bool auto_start = false):
                abstract::timer(),
                callback(callback),
                argument(argument)
                {
                    this->handle = xTimerCreate("", pdMS_TO_TICKS(period_ms), auto_reload, this, &timer::callback_wrapper);
                    if (auto_start) {
                        this->start();
                    }
                }

                using abstract::timer::start;
                using abstract::timer::start_from_isr;
                using abstract::timer::stop;
                using abstract::timer::stop_from_isr;
                using abstract::timer::reset;
                using abstract::timer::reset_from_isr;
                using abstract::timer::set_period;
                using abstract::timer::set_period_from_isr;
                using abstract::timer::get_period_ms;
                using abstract::timer::get_expiration_time_ms;
                using abstract::timer::is_running;
                using abstract::timer::is_valid;
                
                #if (tskKERNEL_VERSION_MAJOR > 8)
                    using abstract::timer::get_auto_reload;
                    using abstract::timer::set_auto_reload;
                #endif
        };

        template <>
        class timer<void> : public abstract::timer {
            private:
                void (*callback)(void);

                static void callback_wrapper(TimerHandle_t handle){
                    timer& self = *static_cast<timer*>(pvTimerGetTimerID(handle));
                    if (self.callback != nullptr) {
                        self.callback();
                    }
                }
            public:
                timer(const char* name, void (*callback)(), uint32_t period_ms = 0, bool auto_reload = false, bool auto_start = false):
                abstract::timer(),
                callback(callback)
                {
                    this->handle = xTimerCreate(name, pdMS_TO_TICKS(period_ms), auto_reload, this, &timer::callback_wrapper);
                    if (auto_start) {
                        this->start();
                    }
                }

                using abstract::timer::start;
                using abstract::timer::start_from_isr;
                using abstract::timer::stop;
                using abstract::timer::stop_from_isr;
                using abstract::timer::reset;
                using abstract::timer::reset_from_isr;
                using abstract::timer::set_period;
                using abstract::timer::set_period_from_isr;
                using abstract::timer::get_period_ms;
                using abstract::timer::get_expiration_time_ms;
                using abstract::timer::is_running;
                using abstract::timer::is_valid;

                #if (tskKERNEL_VERSION_MAJOR > 8)
                    using abstract::timer::get_auto_reload;
                    using abstract::timer::set_auto_reload;
                #endif
        };
    }

    namespace stack {
        /**
         * @brief Statically-allocated software timer with a typed callback argument.
         *
         * Identical in behaviour to `heap::timer` but the FreeRTOS control block
         * is stored in a `timer_struct` member, so no heap allocation takes place.
         *
         * @tparam ARGUMENT_TYPE Type of the argument forwarded to the callback.
         *                       Defaults to `void` (no argument).
         */
        template <typename ARGUMENT_TYPE = void>
        class timer;

        template <typename ARGUMENT_TYPE>
        class timer : public abstract::timer {
            private:
                void (*callback)(ARGUMENT_TYPE);
                ARGUMENT_TYPE argument;
                timer_struct buffer;

                static void callback_wrapper(TimerHandle_t handle){
                    timer& self = *static_cast<timer*>(pvTimerGetTimerID(handle));
                    if (self.callback != nullptr) {
                        self.callback(self.argument);
                    }
                }

            public:
                timer(const char* name, ARGUMENT_TYPE argument, void (*callback)(ARGUMENT_TYPE), uint32_t period_ms = 0, bool auto_reload = false, bool auto_start = false):
                abstract::timer(),
                callback(callback),
                argument(argument)
                {
                    this->handle = xTimerCreateStatic(name, pdMS_TO_TICKS(period_ms), auto_reload, this, &timer::callback_wrapper, &this->buffer);
                    if (auto_start) {
                        this->start();
                    }
                }

                using abstract::timer::start;
                using abstract::timer::start_from_isr;
                using abstract::timer::stop;
                using abstract::timer::stop_from_isr;
                using abstract::timer::reset;
                using abstract::timer::reset_from_isr;
                using abstract::timer::set_period;
                using abstract::timer::set_period_from_isr;
                using abstract::timer::get_period_ms;
                using abstract::timer::get_expiration_time_ms;
                using abstract::timer::is_running;
                using abstract::timer::is_valid;

                #if (tskKERNEL_VERSION_MAJOR > 8)
                    using abstract::timer::get_auto_reload;
                    using abstract::timer::set_auto_reload;
                #endif
        };

        template <>
        class timer<void> : public abstract::timer {
            private:
                void (*callback)(void);
                timer_struct buffer;

                static void callback_wrapper(TimerHandle_t handle){
                    timer& self = *static_cast<timer*>(pvTimerGetTimerID(handle));
                    if (self.callback != nullptr) {
                        self.callback();
                    }
                }

            public:
                timer(const char* name, void (*callback)(), uint32_t period_ms = 0, bool auto_reload = false, bool auto_start = false):
                abstract::timer(),
                callback(callback)
                {
                    this->handle = xTimerCreateStatic(name, pdMS_TO_TICKS(period_ms), auto_reload, this, &timer::callback_wrapper, &this->buffer);
                    if (auto_start) {
                        this->start();
                    }
                }

                using abstract::timer::start;
                using abstract::timer::start_from_isr;
                using abstract::timer::stop;
                using abstract::timer::stop_from_isr;
                using abstract::timer::reset;
                using abstract::timer::reset_from_isr;
                using abstract::timer::set_period;
                using abstract::timer::set_period_from_isr;
                using abstract::timer::get_period_ms;
                using abstract::timer::get_expiration_time_ms;
                using abstract::timer::is_running;
                using abstract::timer::is_valid;
                #if (tskKERNEL_VERSION_MAJOR > 8)
                    using abstract::timer::get_auto_reload;
                    using abstract::timer::set_auto_reload;
                #endif
        };
    }
}