#pragma once

#include "semaphore.hpp"

namespace freertos {

    namespace heap {
        /**
         * @brief Heap-allocated non-recursive mutex.
         *
         * A mutex provides mutual exclusion with priority-inheritance support.
         * Only the task that successfully called `take()` may call `give()`.
         * Attempting to lock a mutex twice from the same task without releasing
         * it first will cause a deadlock — use `recursive` if re-entrancy is needed.
         *
         * ISR variants (`give_from_isr` / `take_from_isr`) are **not** supported
         * by FreeRTOS for mutexes and will return false.
         *
         * Allocated on the heap via `xSemaphoreCreateMutex()`.
         */
        class mutex : public abstract::semaphore {
        public:
            /** @brief Creates the mutex. */
            mutex(void);

            using abstract::semaphore::give;
            using abstract::semaphore::take;
            using abstract::semaphore::is_valid;
            using abstract::semaphore::get_handle;
        };
    }

    namespace stack {
        /**
         * @brief Statically-allocated non-recursive mutex.
         *
         * Identical in behaviour to `heap::mutex` but the FreeRTOS control
         * block is stored in a `semaphore_struct` member — no heap allocation.
         */
        class mutex : public abstract::semaphore {
        private:
            semaphore_struct buffer; ///< Static FreeRTOS control block.
        public:
            /** @brief Creates the mutex using static allocation. */
            mutex(void);

            using abstract::semaphore::give;
            using abstract::semaphore::take;
            using abstract::semaphore::is_valid;
            using abstract::semaphore::get_handle;
        };
    }
}