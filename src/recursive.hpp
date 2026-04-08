#pragma once

#include "semaphore.hpp"

namespace freertos {

    namespace heap {
        /**
         * @brief Heap-allocated recursive mutex.
         *
         * A recursive mutex can be locked multiple times by the **same** task
         * without causing a deadlock.  Each successful `take()` increments an
         * internal counter; the mutex is only fully released when `give()` has
         * been called the same number of times, in the reverse order.
         *
         * ISR variants are **not** supported and will return false.
         *
         * Allocated on the heap via `xSemaphoreCreateRecursiveMutex()`.
         */
        class recursive : public abstract::semaphore {
        public:
            /** @brief Creates the recursive mutex. */
            recursive(void);

            using abstract::semaphore::give;
            using abstract::semaphore::take;
            using abstract::semaphore::is_valid;
            using abstract::semaphore::get_handle;
        };
    }

    namespace stack {
        /**
         * @brief Statically-allocated recursive mutex.
         *
         * Identical in behaviour to `heap::recursive` but uses a `semaphore_struct`
         * member for static allocation — no heap is required.
         */
        class recursive : public abstract::semaphore {
        private:
            semaphore_struct buffer; ///< Static FreeRTOS control block.
        public:
            /** @brief Creates the recursive mutex using static allocation. */
            recursive(void);

            using abstract::semaphore::give;
            using abstract::semaphore::take;
            using abstract::semaphore::is_valid;
            using abstract::semaphore::get_handle;
        };
    }
}