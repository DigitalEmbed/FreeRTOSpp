#pragma once

#include "semaphore.hpp"

namespace freertos {

    /**
     * @brief RAII lock guard for use inside an ISR, with an inline callback pattern.
     *
     * Unlike `lock_guard`, this class is designed for **interrupt service routines**.
     * It attempts a non-blocking `take_from_isr()`; if successful, it immediately
     * executes a user-supplied callback with a reference to the shared data, then
     * releases the semaphore in the destructor.
     *
     * The callback-based design keeps the critical section as short as possible and
     * avoids any need to manually pair take/give calls in ISR code.
     *
     * @tparam DATA_TYPE Type of the shared data passed to the callback.
     *                   Specialised for `void` when no data argument is needed.
     *
     * @code
     * // ISR example – increment a shared counter
     * void IRAM_ATTR on_timer_isr() {
     *     freertos::lock_guard_from_isr<int> guard(my_binary_sem, shared_counter,
     *         [](int& v){ v++; });
     * }
     * @endcode
     *
     * @note Only binary semaphores are supported in ISR context.
     *       Mutexes and recursive mutexes will always return false on take.
     */
    template <typename DATA_TYPE = void>
    class lock_guard_from_isr;

    /** @brief Generic specialisation — passes a `DATA_TYPE&` to the callback. */
    template <typename DATA_TYPE>
    class lock_guard_from_isr {
    public:
        /**
         * @brief Acquires the semaphore (non-blocking) and runs the callback if successful.
         *
         * @pre The semaphore must be a binary or counting semaphore.
         *      Passing a mutex or recursive mutex triggers a `configASSERT` failure
         *      because FreeRTOS does not support mutex operations from ISR context.
         *
         * @param semaphore The semaphore to acquire.
         * @param data      Reference to the shared data forwarded to the callback.
         * @param callback  Function executed inside the critical section.
         */
        explicit lock_guard_from_isr(abstract::semaphore& semaphore, DATA_TYPE& data, void (*callback)(DATA_TYPE&)):
        semaphore(semaphore)
        {
            configASSERT(semaphore.supports_isr());
            if (semaphore.take_from_isr()){
                this->locked = true;
                callback(data);
            }
        }

        /** @brief Releases the semaphore. */
        ~lock_guard_from_isr(void){
            this->semaphore.give_from_isr();
            this->locked = false;
        }

        /**
         * @brief Returns whether the semaphore was acquired and the callback ran.
         * @return true if the critical section executed, false otherwise.
         */
        bool is_locked() const {
            return this->locked;
        }
    private:
        abstract::semaphore& semaphore;
        bool locked {false};
    };

    /** @brief Void specialisation — no data argument is passed to the callback. */
    template <>
    class lock_guard_from_isr<void> {
    public:
        /**
         * @brief Acquires the semaphore (non-blocking) and runs the callback if successful.
         *
         * @pre The semaphore must be a binary or counting semaphore.
         *      Passing a mutex or recursive mutex triggers a `configASSERT` failure.
         *
         * @param semaphore The semaphore to acquire.
         * @param callback  Function executed inside the critical section.
         */
        explicit lock_guard_from_isr(abstract::semaphore& semaphore, void (*callback)(void)) :
        semaphore(semaphore)
        {
            configASSERT(semaphore.supports_isr());
            if (semaphore.take_from_isr()){
                this->locked = true;
                callback();
            }
        }

        /** @brief Releases the semaphore. */
        ~lock_guard_from_isr(void){
            this->semaphore.give_from_isr();
            this->locked = false;
        }

        /**
         * @brief Returns whether the semaphore was acquired and the callback ran.
         * @return true if the critical section executed, false otherwise.
         */
        bool is_locked() const {
            return this->locked;
        }
    private:
        abstract::semaphore& semaphore;
        bool locked {false};
    };
}
