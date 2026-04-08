#pragma once

#include "mutex.hpp"

namespace freertos {
    namespace abstract{

        /**
         * @brief Abstract base class for mutex-protected shared data.
         *
         * Wraps a value of type `DATA_TYPE` together with a pointer to an
         * `abstract::semaphore` (supplied by concrete subclasses) so that all
         * reads and writes are automatically serialised with a `lock_guard`.
         *
         * Prefer the concrete types `stack::shared_data` or `heap::shared_data`
         * rather than using this base class directly.
         *
         * @tparam DATA_TYPE The type of the protected value.
         */
        template <typename DATA_TYPE>
        class shared_data {
        protected:
            DATA_TYPE data;                         ///< The protected value.
            abstract::semaphore* semaphore {nullptr}; ///< Lock used by set/get/use.

            shared_data(const DATA_TYPE& data) : data(data), semaphore(nullptr) {}
            shared_data(void) : data(), semaphore(nullptr) {}
        public:
            /**
             * @brief Writes a new value under the mutex.
             * @param data New value to store (copied by value).
             */
            void set(const DATA_TYPE& data){
                lock_guard guard(*this->semaphore);
                this->data = data;
            }

            /**
             * @brief Returns a copy of the current value under the mutex.
             * @return Copy of the protected data.
             */
            DATA_TYPE get(void){
                lock_guard guard(*this->semaphore);
                return this->data;
            }

            /**
             * @brief Executes a callback with a reference to the protected data.
             *
             * The mutex is held for the entire duration of the callback.
             * Use this when you need to perform multiple operations on the data
             * atomically without an extra copy.
             *
             * @param callback Function receiving a `DATA_TYPE&` reference.
             */
            void use(void (*callback)(DATA_TYPE&)){
                lock_guard guard(*this->semaphore);
                callback(this->data);
            }

            /**
             * @brief Executes a callback with the protected data and an extra argument.
             *
             * Useful when the callback needs additional context beyond the shared data.
             *
             * @tparam ARGUMENT_TYPE Type of the extra argument.
             * @param arguments      Extra argument forwarded to the callback.
             * @param callback       Function receiving `DATA_TYPE&` and `ARGUMENT_TYPE`.
             */
            template <typename ARGUMENT_TYPE>
            void use(ARGUMENT_TYPE arguments, void (*callback)(DATA_TYPE&, ARGUMENT_TYPE)){
                lock_guard guard(*this->semaphore);
                callback(this->data, arguments);
            }
        };
    }

    namespace heap {
        /**
         * @brief Heap-allocated mutex-protected shared data container.
         *
         * Combines a heap-allocated mutex with an inline value so that any
         * task can safely read or write the data concurrently.
         *
         * @tparam DATA_TYPE The type of the protected value.
         */
        template <typename DATA_TYPE>
        class shared_data : public abstract::shared_data<DATA_TYPE> {
            private:
                heap::mutex locker;
            public:
                /** @brief Default-constructs the protected value and creates the mutex. */
                shared_data(void) : abstract::shared_data<DATA_TYPE>(), locker() {
                    this->semaphore = &this->locker;
                }

                /**
                 * @brief Constructs with an initial value and creates the mutex.
                 * @param data Initial value of the protected data.
                 */
                shared_data(const DATA_TYPE& data) : abstract::shared_data<DATA_TYPE>(data), locker() {
                    this->semaphore = &this->locker;
                }

                ~shared_data(void){
                    this->semaphore = nullptr;
                }
        };
    }

    namespace stack {
        /**
         * @brief Statically-allocated mutex-protected shared data container.
         *
         * Identical in behaviour to `heap::shared_data` but the internal mutex
         * uses static allocation — no heap is required.
         *
         * @tparam DATA_TYPE The type of the protected value.
         */
        template <typename DATA_TYPE>
        class shared_data : public abstract::shared_data<DATA_TYPE> {
            private:
                stack::mutex locker;
            public:
                /** @brief Default-constructs the protected value and creates the mutex. */
                shared_data(void) : abstract::shared_data<DATA_TYPE>(), locker() {
                    this->semaphore = &this->locker;
                }

                /**
                 * @brief Constructs with an initial value and creates the mutex.
                 * @param data Initial value of the protected data.
                 */
                shared_data(const DATA_TYPE& data) : abstract::shared_data<DATA_TYPE>(data), locker() {
                    this->semaphore = &this->locker;
                }

                ~shared_data(void){
                    this->semaphore = nullptr;
                }
        };
    }
}