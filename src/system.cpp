#include "system.hpp"

#include "freertos/task.h"

using namespace freertos;
using namespace typedefs;

// =============================================================================
// freertos::system
// =============================================================================

tick_type system::get_tick_count(void) {
    return xTaskGetTickCount();
}

tick_type system::get_tick_count_from_isr(void){
    return xTaskGetTickCountFromISR();
}

uint64_t system::get_milliseconds(void){
    return ((TickType_t) (uint64_t) xTaskGetTickCount() * 1000 / configTICK_RATE_HZ);
}

uint64_t system::get_milliseconds_from_isr(void){
    return ((TickType_t) (uint64_t) xTaskGetTickCountFromISR() * 1000 / configTICK_RATE_HZ);
}

u_base_type system::get_amount_of_tasks(void){
    return uxTaskGetNumberOfTasks();
}
