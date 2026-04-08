#pragma once

#include "semaphore.hpp"

namespace freertos {

    /**
     * @brief RAII lock guard for any `abstract::semaphore` in task context.
     *
     * Acquires the semaphore on construction and automatically releases it when
     * the guard goes out of scope, ensuring the lock is always freed even if an
     * early return or exception occurs.
     *
     * @code
     * freertos::stack::mutex mtx;
     *
     * void my_task() {
     *     freertos::lock_guard guard(mtx);   // locks here
     *     if (!guard.is_locked()) return;    // optional: check for timeout
     *     // ... critical section ...
     * }                                      // unlocks here automatically
     * @endcode
     *
     * @note Do **not** use this class from an ISR context; use
     *       `lock_guard_from_isr` instead.
     */
    class lock_guard {
        private:
            abstract::semaphore& semaphore;
            bool locked {false};
        public:
            /**
             * @brief Acquires the semaphore.
             * @param semaphore  The semaphore (or mutex) to lock.
             * @param timeout_ms Maximum time to wait in milliseconds.
             *                   Defaults to `constants::max_delay_ms` (wait forever).
             */
            explicit lock_guard(abstract::semaphore& semaphore, uint32_t timeout_ms = constants::max_delay_ms);

            /** @brief Releases the semaphore. */
            ~lock_guard(void);

            /**
             * @brief Returns whether the semaphore was successfully acquired.
             * @return true if locked, false if the timeout expired.
             */
            bool is_locked() const;
    };

}
