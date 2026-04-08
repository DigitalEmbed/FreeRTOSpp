#pragma once

#include "shared_data.hpp"
#include "binary.hpp"

namespace freertos {
    namespace abstract{

        /**
         * @brief Extension of `shared_data` with ISR-safe read/write operations.
         *
         * Uses a **binary semaphore** (instead of a mutex) as the internal lock so
         * that it can be safely released from an interrupt service routine.
         * Inherits all task-context methods (`set`, `get`, `use`) from
         * `abstract::shared_data`.
         *
         * @tparam DATA_TYPE The type of the protected value.
         *
         * @note Mutexes cannot be used in ISR context in FreeRTOS, which is why
         *       concrete subclasses (`heap::shared_data_with_isr`,
         *       `stack::shared_data_with_isr`) embed a binary semaphore instead.
         */
        template <typename DATA_TYPE>
        class shared_data_with_isr : public shared_data<DATA_TYPE> {
            protected:
                shared_data_with_isr(const DATA_TYPE& data) : shared_data<DATA_TYPE>(data) {}
                shared_data_with_isr(void) : shared_data<DATA_TYPE>() {}
            public:
                /**
                 * @brief Writes a new value under the binary semaphore (ISR context).
                 * @param data New value to store.
                 */
                void set_from_isr(const DATA_TYPE& data){

                    struct param_t {
                        const DATA_TYPE& data;
                        DATA_TYPE& target;
                    } param = {
                        .data = data, 
                        .target = this->data
                    };

                    lock_guard_from_isr<param_t> guard(*this->semaphore, param, [](param_t& param){
                        param.target = param.data;
                    });
                }

                /**
                 * @brief Returns a copy of the current value (ISR context).
                 * @return Copy of the protected data.
                 */
                DATA_TYPE get_from_isr(void){
                    DATA_TYPE target;

                    struct param_t {
                        DATA_TYPE& data;
                        DATA_TYPE& target;
                    } param = {
                        .data = this->data, 
                        .target = target
                    };

                    lock_guard_from_isr<param_t> guard(*this->semaphore, param, [](param_t& param){
                        param.target = param.data;
                    });

                    return target;
                }

                /**
                 * @brief Executes a callback with the protected data (ISR context).
                 *
                 * The binary semaphore is held for the duration of the callback.
                 * The callback must be short and must not call any blocking FreeRTOS API.
                 *
                 * @param callback Function receiving a `DATA_TYPE&` reference.
                 */
                void use_from_isr(void (*callback)(DATA_TYPE&)){
                    lock_guard_from_isr<DATA_TYPE> guard(*this->semaphore, this->data, callback);
                }

                template <typename ARGUMENT_TYPE>
                void use_from_isr(ARGUMENT_TYPE arguments, void (*callback)(DATA_TYPE&, ARGUMENT_TYPE)){
                    struct param_t {
                        DATA_TYPE& data;
                        ARGUMENT_TYPE arguments;
                        void (*callback)(DATA_TYPE&, ARGUMENT_TYPE);
                    } param = {this->data, arguments, callback};
                    
                    lock_guard_from_isr<param_t> guard(*this->semaphore, param, [](param_t& param){
                        param.callback(param.data, param.arguments);
                    });
                }

                using shared_data<DATA_TYPE>::set;
                using shared_data<DATA_TYPE>::get;
                using shared_data<DATA_TYPE>::use;
        };
    }

    namespace heap {
        template <typename DATA_TYPE>
        class shared_data_with_isr : public abstract::shared_data_with_isr<DATA_TYPE> {
            private:
                heap::binary locker;
            public:
                shared_data_with_isr(void) : abstract::shared_data_with_isr<DATA_TYPE>(), locker() {
                    this->semaphore = &this->locker;
                }

                shared_data_with_isr(const DATA_TYPE& data) : abstract::shared_data_with_isr<DATA_TYPE>(data), locker() {
                    this->semaphore = &this->locker;
                }

                ~shared_data_with_isr(void){
                    this->semaphore = nullptr;
                }

                using abstract::shared_data_with_isr<DATA_TYPE>::set;
                using abstract::shared_data_with_isr<DATA_TYPE>::set_from_isr;
                using abstract::shared_data_with_isr<DATA_TYPE>::get;
                using abstract::shared_data_with_isr<DATA_TYPE>::get_from_isr;
        };
    }

    namespace stack {
        template <typename DATA_TYPE>
        class shared_data_with_isr : public abstract::shared_data_with_isr<DATA_TYPE> {
            private:
                stack::binary locker;
            public:
                shared_data_with_isr(void) : abstract::shared_data_with_isr<DATA_TYPE>(), locker() {
                    this->semaphore = &this->locker;
                }

                shared_data_with_isr(const DATA_TYPE& data) : abstract::shared_data_with_isr<DATA_TYPE>(data), locker() {
                    this->semaphore = &this->locker;
                }

                ~shared_data_with_isr(void){
                    this->semaphore = nullptr;
                }
        };
    }
}