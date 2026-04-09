#include "mutex.hpp"

using namespace freertos;

// =============================================================================
// heap::mutex
// =============================================================================

heap::mutex::mutex(void) : abstract::semaphore(semaphore::type::mutex) {
    if (this->handle != nullptr){
        return;
    }

    this->handle = xSemaphoreCreateMutex();
}

// =============================================================================
// stack::mutex
// =============================================================================

stack::mutex::mutex(void) : abstract::semaphore(semaphore::type::mutex) {
    if (this->handle != nullptr){
        return;
    }

    this->handle = xSemaphoreCreateMutexStatic(&this->buffer);
}
