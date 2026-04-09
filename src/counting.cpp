#include "counting.hpp"

using namespace freertos;

// =============================================================================
// heap::counting
// =============================================================================

heap::counting::counting(uint32_t max_count, uint32_t initial_count) : abstract::semaphore(semaphore::type::counting) {
    if (this->handle != nullptr){
        return;
    }

    this->handle = xSemaphoreCreateCounting(max_count, initial_count);
}

// =============================================================================
// stack::counting
// =============================================================================

stack::counting::counting(uint32_t max_count, uint32_t initial_count) : abstract::semaphore(semaphore::type::counting) {
    if (this->handle != nullptr){
        return;
    }

    this->handle = xSemaphoreCreateCountingStatic(max_count, initial_count, &this->buffer);
}
