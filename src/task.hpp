#pragma once

#include "constants.hpp"
#include "typedefs.hpp"
#include <functional>

namespace freertos {
    using namespace constants;
    using namespace typedefs;

    namespace abstract {
        class task {
            public:
                
                    class info {
                        public:
                            enum class state {
                                ready = eReady,
                                running = eRunning,
                                blocked = eBlocked,
                                suspended = eSuspended,
                                deleted = eDeleted,

                                #if (tskKERNEL_VERSION_MAJOR > 8)
                                    invalid = eInvalid
                                #else 
                                    invalid = eDeleted
                                #endif
                            };
                        private:
                            task_handle handle {nullptr};
                            
                            friend class task;
                            friend class self;

                        public:
                            info(task_handle handle);
                            
                            uint16_t get_priority(void) const;
                            uint32_t get_free_stack_memory(void) const;
                            state get_state(void) const;
                            const char* get_name(void) const;
                    };

                class notifier {
                    
                    private:
                        task_handle handle {nullptr};
                        uint32_t last_value {0};

                        friend class task;
                    public:
                        notifier(task_handle handle);
                        
                        bool signal(u_base_type index = 0, bool resume = true);
                        bool signal_from_isr(u_base_type index = 0, bool resume = true);

                        bool set_bits(uint32_t bits, u_base_type index = 0, bool resume = true);
                        bool set_bits_from_isr(uint32_t bits, u_base_type index = 0, bool resume = true);

                        bool increment(uint32_t value, u_base_type index = 0, bool resume = true);
                        bool increment_from_isr(uint32_t value, u_base_type index = 0, bool resume = true);

                        bool overwrite_value(uint32_t value, u_base_type index = 0, bool resume = true);
                        bool overwrite_value_from_isr(uint32_t value, u_base_type index = 0, bool resume = true);

                        bool send_value(uint32_t value, u_base_type index = 0, bool resume = true);
                        bool send_value_from_isr(uint32_t value, u_base_type index = 0, bool resume = true);

                        bool clear(u_base_type index = 0, bool resume = true);
                        bool clear_from_isr(u_base_type index = 0, bool resume = true);

                        uint32_t get_last_value(void) const;
                };               
                class self {
                    public:
                        static task_handle get_handle(void);
                        static void suspend(void);
                        static void yield(void);
                        static void delay(uint32_t time_ms);
                        static bool get_notification(uint32_t& notification_value, u_base_type index = 0, uint32_t timeout_ms = max_delay_ms);
                        static info get_info(void);

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
                task(task_handle handle);
                ~task();
                bool resume(void);
                bool resume_from_isr(void);
                bool suspend(void);
                bool suspend_from_isr(void);
                bool is_running(void);
                bool join(uint32_t observer_delay_ms = 100, uint32_t timeout_ms = max_delay_ms);
                bool is_valid(void);
                notifier get_notifier(void);
                info get_info(void);

                task_handle& get_handle(void);
        };
    }

    namespace stack {
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
                task(const char* name, uint16_t priority, ARGUMENT_TYPE param, void (*callback)(ARGUMENT_TYPE)) :
                abstract::task(),
                arguments(param), 
                callback(callback) 
                {
                    this->handle = xTaskCreateStatic(wrapper, name, STACK_SIZE, this, priority, stack, &buffer);
                }

                #if defined(ESP32)
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

        template <uint32_t STACK_SIZE>
        class task<STACK_SIZE, void> : public abstract::task {
            private:
                task_stack stack[STACK_SIZE];
                task_struct buffer;

                void (*callback)();

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
                task(const char* name, uint16_t priority, void (*callback)(void)) :
                abstract::task(),
                callback(callback)
                {
                    this->handle = xTaskCreateStatic(wrapper, name, STACK_SIZE, this, priority, stack, &buffer);
                }

                #if defined(ESP32)
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
                task(const char* name, uint16_t priority, uint32_t stack_size, std::function <void()> callback) :
                abstract::task(),
                callback(callback)
                {
                    xTaskCreate(wrapper, name, stack_size, this, priority, &this->handle);
                }
                
                #if defined(ESP32)
                    task(const char* name, uint16_t priority, uint32_t stack_size, std::function <void()> callback, uint8_t core_id) :
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

