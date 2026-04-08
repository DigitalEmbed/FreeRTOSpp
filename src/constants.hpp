#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/FreeRTOSConfig.h"
#include "freertos/task.h"

namespace freertos {

    /**
     * @brief Compile-time constants derived from FreeRTOS configuration macros.
     *
     * These constants provide a type-safe C++ interface to the values defined in
     * FreeRTOSConfig.h, avoiding direct use of C macros in application code.
     */
    namespace constants {

        /** @brief Maximum timeout value. Passing this to any blocking call means "wait forever". */
        constexpr uint32_t max_delay_ms = portMAX_DELAY;

        /** @brief Maximum number of characters in a task name, as configured by configMAX_TASK_NAME_LEN. */
        constexpr uint8_t task_name_length = configMAX_TASK_NAME_LEN;

        /** @brief Highest valid task priority (configMAX_PRIORITIES - 1). */
        constexpr uint8_t max_priority = configMAX_PRIORITIES - 1;

        /**
         * @brief Maximum valid notification index.
         *
         * Equals configTASK_NOTIFICATION_ARRAY_ENTRIES - 1 when the FreeRTOS
         * notification array is available (kernel >= 10.4), or 0 for single-
         * notification kernels.
         */
        #if defined(configTASK_NOTIFICATION_ARRAY_ENTRIES)
            constexpr uint8_t max_notifications = configTASK_NOTIFICATION_ARRAY_ENTRIES - 1;
        #else
            constexpr uint8_t max_notifications = 0;
        #endif

        /** @brief Minimum allowed task stack size in words, as configured by configMINIMAL_STACK_SIZE. */
        constexpr uint32_t min_stack_size = configMINIMAL_STACK_SIZE;
    }

}