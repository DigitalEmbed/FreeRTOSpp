#pragma once

#include <stdint.h>
#include "./typedefs.hpp"

namespace freertos {
    using namespace typedefs;

    /**
     * @brief Static utility class for querying global FreeRTOS system state.
     *
     * All methods are static; there is no need to instantiate this class.
     * ISR-safe variants (suffixed `_from_isr`) must only be called from
     * interrupt context.
     */
    class system {
        public:
            /**
             * @brief Returns the current FreeRTOS tick count from a task context.
             * @return Current tick count as tick_type.
             */
            static tick_type get_tick_count(void);

            /**
             * @brief Returns the current FreeRTOS tick count from an ISR context.
             * @return Current tick count as tick_type.
             */
            static tick_type get_tick_count_from_isr(void);

            /**
             * @brief Converts the current tick count to milliseconds (task context).
             * @return Elapsed time in milliseconds since the scheduler started.
             */
            static uint64_t get_milliseconds(void);

            /**
             * @brief Converts the current tick count to milliseconds (ISR context).
             * @return Elapsed time in milliseconds since the scheduler started.
             */
            static uint64_t get_milliseconds_from_isr(void);

            /**
             * @brief Returns the number of tasks currently managed by the scheduler.
             * @return Task count as u_base_type.
             */
            static u_base_type get_amount_of_tasks(void);
    };
}