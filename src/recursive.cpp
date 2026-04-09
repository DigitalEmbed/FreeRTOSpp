#include "recursive.hpp"

using namespace freertos;

// =============================================================================
// heap::recursive
// =============================================================================

heap::recursive::recursive(void) : abstract::semaphore(semaphore::type::recursive){
    if (this->handle != nullptr) {
        return;
    }

    this->handle = xSemaphoreCreateRecursiveMutex();
}

// =============================================================================
// stack::recursive
// =============================================================================

stack::recursive::recursive(void) : abstract::semaphore(semaphore::type::recursive){
    if (this->handle != nullptr) {
        return;
    }

    this->handle = xSemaphoreCreateRecursiveMutexStatic(&this->buffer);
}
