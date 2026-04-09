#include "lock_guard.hpp"

using namespace freertos;

#include "esp_log.h"

// =============================================================================
// freertos::lock_guard
// =============================================================================

lock_guard::lock_guard(abstract::semaphore& semaphore, uint32_t timeout_ms) :
semaphore(semaphore)
{
    this->locked = this->semaphore.take(timeout_ms) == true;
}

lock_guard::~lock_guard(void) {
    this->semaphore.give();
    this->locked = false;
}

bool lock_guard::is_locked() const {
    return this->locked;
}
