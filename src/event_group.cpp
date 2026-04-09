#include "event_group.hpp"

using namespace freertos;
using namespace abstract;
using namespace constants;

// =============================================================================
// event_bit_ref
// =============================================================================

event_bit_ref::event_bit_ref(EventGroupHandle_t* handle, uint32_t index) :
handle(handle),
index(index)
{}

event_bit_ref& event_bit_ref::operator=(bool value) {
    if (handle == nullptr || *handle == nullptr) {
        return *this;
    }

    const EventBits_t mask = EventBits_t(1) << index;

    if (value) {
        xEventGroupSetBits(*handle, mask);
    } else {
        xEventGroupClearBits(*handle, mask);
    }

    return *this;
}

event_bit_ref& event_bit_ref::operator=(const event_bit_ref& other) {
    return *this = static_cast<bool>(other);
}

event_bit_ref::operator bool() const {
    if (handle == nullptr || *handle == nullptr) {
        return false;
    }

    return ((xEventGroupGetBits(*handle) >> index) & 1u) != 0u;
}

event_bit_ref::operator int8_t() const {
    return static_cast<int8_t>(static_cast<bool>(*this));
}

event_bit_ref::operator uint8_t() const {
    return static_cast<uint8_t>(static_cast<bool>(*this));
}

event_bit_ref::operator int() const {
    return static_cast<int>(static_cast<bool>(*this));
}

// =============================================================================
// abstract::event_group
// =============================================================================

event_group::~event_group(void) {
    if (handle != nullptr) {
        vEventGroupDelete(handle);
        handle = nullptr;
    }
}

event_bit_ref event_group::operator[](uint32_t index) {
    configASSERT(index < max_event_group_bits);

    if (index >= max_event_group_bits) {
        return event_bit_ref(nullptr, 0);
    }

    return event_bit_ref(&handle, index);
}

bool event_group::set(uint32_t index, bool value) {
    configASSERT(index < max_event_group_bits);

    if (index >= max_event_group_bits || handle == nullptr) {
        return false;
    }

    const EventBits_t mask = EventBits_t(1) << index;

    if (value) {
        xEventGroupSetBits(handle, mask);
    } else {
        xEventGroupClearBits(handle, mask);
    }

    return true;
}

bool event_group::get(uint32_t index, bool& out) const {
    configASSERT(index < max_event_group_bits);

    if (index >= max_event_group_bits || handle == nullptr) {
        return false;
    }

    out = ((xEventGroupGetBits(handle) >> index) & 1u) != 0u;
    return true;
}

bool event_group::set_from_isr(uint32_t index, bool value) {
    configASSERT(index < max_event_group_bits);

    if (index >= max_event_group_bits || handle == nullptr) {
        return false;
    }

    const EventBits_t mask = EventBits_t(1) << index;

    if (value) {
        BaseType_t woken = pdFALSE;
        return xEventGroupSetBitsFromISR(handle, mask, &woken) == pdPASS;
    }

    xEventGroupClearBitsFromISR(handle, mask);
    return true;
}

bool event_group::get_from_isr(uint32_t index, bool& out) const {
    configASSERT(index < max_event_group_bits);

    if (index >= max_event_group_bits || handle == nullptr) {
        return false;
    }

    out = ((xEventGroupGetBitsFromISR(handle) >> index) & 1u) != 0u;
    return true;
}

bool event_group::is_valid(void) const {
    return handle != nullptr;
}

// =============================================================================
// abstract::event_group::iterator
// =============================================================================

event_group::iterator::iterator(event_group* parent, uint32_t index) :
parent(parent),
index(index)
{}

event_bit_ref event_group::iterator::operator*(void) {
    return event_bit_ref(&parent->handle, index);
}

event_group::iterator& event_group::iterator::operator++(void) {
    ++index;
    return *this;
}

bool event_group::iterator::operator!=(const iterator& other) const {
    return index != other.index;
}

event_group::iterator event_group::begin(void) {
    return iterator(this, 0);
}

event_group::iterator event_group::end(void) {
    return iterator(this, max_event_group_bits);
}

// =============================================================================
// stack::event_group
// =============================================================================

freertos::stack::event_group::event_group(void) {
    this->handle = xEventGroupCreateStatic(&buffer);
}

// =============================================================================
// heap::event_group
// =============================================================================

freertos::heap::event_group::event_group(void) {
    this->handle = xEventGroupCreate();
}
