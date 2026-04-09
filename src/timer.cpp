#include "timer.hpp"

using namespace freertos;
using namespace abstract;
using namespace constants;
using namespace typedefs;

// =============================================================================
// abstract::timer
// =============================================================================

timer::timer(timer_handle handle):
handle(handle)
{}

timer::~timer(){
    if (this->handle != nullptr) {
        xTimerDelete(handle, 0);
        this->handle = nullptr;
    }
}

bool timer::start(uint32_t delay_ms){
    if (this->handle == nullptr) {
        return false;
    }

    uint32_t ticks = pdMS_TO_TICKS(delay_ms);
    return xTimerStart(this->handle, ticks) == pdPASS;
}

bool timer::start_from_isr(void){
    if (this->handle == nullptr) {
        return false;
    }

    return xTimerStartFromISR(this->handle, 0) == pdPASS;
}

bool timer::stop(uint32_t delay_ms){
    if (this->handle == nullptr) {
        return false;
    }

    uint32_t ticks = pdMS_TO_TICKS(delay_ms);
    return xTimerStop(this->handle, ticks) == pdPASS;
}

bool timer::stop_from_isr(void){
    if (this->handle == nullptr) {
        return false;
    }

    return xTimerStopFromISR(this->handle, 0) == pdPASS;
}

bool timer::reset(uint32_t delay_ms){
    if (this->handle == nullptr) {
        return false;
    }

    uint32_t ticks = pdMS_TO_TICKS(delay_ms);
    return xTimerReset(this->handle, ticks) == pdPASS;
}

bool timer::reset_from_isr(void){
    if (this->handle == nullptr) {
        return false;
    }

    return xTimerResetFromISR(this->handle, 0) == pdPASS;
}

bool timer::is_running(void){
    if (this->handle == nullptr) {
        return false;
    }

    return xTimerIsTimerActive(this->handle) == pdTRUE;
}

bool timer::is_valid(void){
    return this->handle != nullptr;
}

bool timer::set_period(uint32_t period_ms){
    if (this->handle == nullptr) {
        return false;
    }

    uint32_t ticks = pdMS_TO_TICKS(period_ms);
    return xTimerChangePeriod(this->handle, ticks, 0) == pdPASS;
}

bool timer::set_period_from_isr(uint32_t period_ms){
    if (this->handle == nullptr) {
        return false;
    }

    uint32_t ticks = pdMS_TO_TICKS(period_ms);
    return xTimerChangePeriodFromISR(this->handle, ticks, 0) == pdPASS;
}

uint32_t timer::get_period_ms(void){
    if (this->handle == nullptr) {
        return 0;
    }

    return ((TickType_t) (uint64_t) xTimerGetPeriod(this->handle) * 1000 / configTICK_RATE_HZ);
}

uint32_t timer::get_expiration_time_ms(void){
    if (this->handle == nullptr) {
        return 0;
    }

    return ((TickType_t) (uint64_t) xTimerGetExpiryTime(this->handle) * 1000 / configTICK_RATE_HZ);
}

timer_handle& timer::get_handle(void){
    return this->handle;
}

#if (tskKERNEL_VERSION_MAJOR > 8)
    bool timer::get_auto_reload(void){
        if (this->handle == nullptr) {
            return false;
        }

        return xTimerIsTimerActive(this->handle) == pdTRUE;
    }

    bool timer::set_auto_reload(bool auto_reload){
        if (this->handle == nullptr) {
            return false;
        }

        vTimerSetReloadMode(this->handle, static_cast<UBaseType_t>(auto_reload));
        return true;
    }
#endif
