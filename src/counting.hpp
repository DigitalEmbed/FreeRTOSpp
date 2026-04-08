#pragma once

#include "semaphore.hpp"

namespace freertos {

    namespace heap {
        /**
         * @brief Heap-allocated counting semaphore.
         *
         * A counting semaphore maintains an integer count in the range
         * [0, max_count].  Typical uses include tracking a pool of resources
         * (one `take` per acquisition, one `give` per release) or counting
         * events produced by an ISR.
         *
         * Allocated on the heap via `xSemaphoreCreateCounting()`.
         *
         * @param max_count     Upper bound of the counter.
         * @param initial_count Starting value of the counter (default 0).
         */
        class counting : public abstract::semaphore {
        public:
            /**
             * @brief Creates the counting semaphore.
             * @param max_count     Maximum value the counter may reach.
             * @param initial_count Initial counter value. Defaults to 0.
             */
            counting(uint32_t max_count, uint32_t initial_count = 0);

            using abstract::semaphore::give;
            using abstract::semaphore::take;
            using abstract::semaphore::give_from_isr;
            using abstract::semaphore::take_from_isr;
            using abstract::semaphore::is_valid;
            using abstract::semaphore::get_handle;
        };
    }

    namespace stack {
        /**
         * @brief Statically-allocated counting semaphore.
         *
         * Identical in behaviour to `heap::counting` but uses a `semaphore_struct`
         * member for static allocation — no heap is required.
         */
        class counting : public abstract::semaphore {
        private:
            semaphore_struct buffer; ///< Static FreeRTOS control block.
        public:
            /**
             * @brief Creates the counting semaphore using static allocation.
             * @param max_count     Maximum value the counter may reach.
             * @param initial_count Initial counter value. Defaults to 0.
             */
            counting(uint32_t max_count, uint32_t initial_count = 0);

            using abstract::semaphore::give;
            using abstract::semaphore::take;
            using abstract::semaphore::give_from_isr;
            using abstract::semaphore::take_from_isr;
            using abstract::semaphore::is_valid;
            using abstract::semaphore::get_handle;
        };
    }
}