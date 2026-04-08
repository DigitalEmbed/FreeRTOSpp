#pragma once

#include "semaphore.hpp"

namespace freertos {

    namespace heap {
        /**
         * @brief Heap-allocated binary semaphore.
         *
         * A binary semaphore has two states: available (1) and unavailable (0).
         * It is useful for signalling between tasks or between an ISR and a task.
         *
         * The underlying FreeRTOS object is allocated on the heap via
         * `xSemaphoreCreateBinary()`.
         */
        class binary : public abstract::semaphore {
        public:
            /** @brief Creates the binary semaphore. The initial state is unavailable (0). */
            binary(void);

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
         * @brief Statically-allocated binary semaphore.
         *
         * Identical in behaviour to `heap::binary` but the FreeRTOS control
         * block is stored in a `semaphore_struct` member, so no heap allocation
         * takes place.  Preferred in memory-constrained or deterministic systems.
         */
        class binary : public abstract::semaphore {
        private:
            semaphore_struct buffer; ///< Static FreeRTOS control block.
        public:
            /** @brief Creates the binary semaphore using static allocation. */
            binary(void);

            using abstract::semaphore::give;
            using abstract::semaphore::take;
            using abstract::semaphore::give_from_isr;
            using abstract::semaphore::take_from_isr;
            using abstract::semaphore::is_valid;
            using abstract::semaphore::get_handle;
        };
    }
}