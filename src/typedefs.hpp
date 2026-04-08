#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"

namespace freertos {

    /**
     * @brief C++ type aliases for the underlying FreeRTOS C types.
     *
     * Importing these aliases avoids sprinkling raw FreeRTOS typedefs throughout
     * application code and keeps the API consistent with the rest of the library.
     */
    namespace typedefs {
        /** @brief Handle to a FreeRTOS task (TaskHandle_t). */
        using task_handle = TaskHandle_t;

        /** @brief Handle to a FreeRTOS queue (QueueHandle_t). */
        using queue_handle = QueueHandle_t;

        /** @brief Handle to a FreeRTOS semaphore or mutex (SemaphoreHandle_t). */
        using semaphore_handle = SemaphoreHandle_t;

        /** @brief Static storage buffer for a task created without heap allocation (StaticTask_t). */
        using task_struct = StaticTask_t;

        /** @brief Static storage buffer for a queue created without heap allocation (StaticQueue_t). */
        using queue_struct = StaticQueue_t;

        /** @brief Static storage buffer for a semaphore created without heap allocation (StaticSemaphore_t). */
        using semaphore_struct = StaticSemaphore_t;

        /** @brief Type used for each word of a task stack (StackType_t). */
        using task_stack = StackType_t;

        /** @brief Handle to a FreeRTOS software timer (TimerHandle_t). */
        using timer_handle = TimerHandle_t;

        /** @brief Static storage buffer for a timer created without heap allocation (StaticTimer_t). */
        using timer_struct = StaticTimer_t;

        /** @brief Unsigned base integer type used by FreeRTOS (UBaseType_t). */
        using u_base_type = UBaseType_t;

        /** @brief Signed base integer type used by FreeRTOS (BaseType_t). */
        using base_type = BaseType_t;

        /** @brief Tick count type; one tick equals the period configured by configTICK_RATE_HZ (TickType_t). */
        using tick_type = TickType_t;
    }
}