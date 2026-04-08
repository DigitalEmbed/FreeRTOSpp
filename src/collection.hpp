#pragma once

#include "typedefs.hpp"
#include "constants.hpp"

namespace freertos {

    using namespace typedefs;
    using namespace constants;

    namespace abstract {

        /**
         * @brief Abstract base class for all queue-based collection types.
         *
         * Wraps the FreeRTOS queue API and provides `push`/`pop` operations with
         * optional timeout, ISR-safe variants, and capacity queries.
         *
         * Concrete subclasses (`fifo`, `lifo`, `singleton`) inherit from this
         * template and choose a `send_mode` to define the ordering policy.
         *
         * @tparam DATA_TYPE Type of the elements stored in the collection.
         *                   Must be trivially copyable (copied by value into the queue).
         */
        template <typename DATA_TYPE>
        class collection {
        protected:
            /**
             * @brief Controls where new items are inserted in the underlying queue.
             *
             * - `to_back`  — insert at the back  → FIFO ordering.
             * - `to_front` — insert at the front → LIFO ordering.
             * - `single`   — overwrite the single slot (always latest value).
             */
            enum class send_mode : int8_t {
                none     = -1,
                to_back  =  0,
                to_front =  1,
                single   =  2
            };
            send_mode mode {send_mode::none};
            queue_handle handle {nullptr};
            bool overwrite_if_full {false};
            const uint32_t storage_size {0};

            explicit collection(send_mode mode, bool overwrite_if_full, uint32_t storage_size):
            mode(mode),
            overwrite_if_full(overwrite_if_full),
            storage_size(storage_size)
            {}
        public:
            /** @brief Destroys the collection and frees the underlying FreeRTOS queue. */
            ~collection(void){
                if (this->handle != nullptr) {
                    vQueueDelete(this->handle);
                    this->handle = nullptr;
                }
            }

            /**
             * @brief Removes and returns the front item from the collection (task context).
             *
             * Blocks for up to @p timeout_ms milliseconds waiting for an item to
             * become available.
             *
             * @param[out] data       Receives the dequeued item.
             * @param      timeout_ms Maximum wait time in milliseconds.
             *                        Use `constants::max_delay_ms` to wait forever.
             * @return true if an item was successfully retrieved, false on timeout.
             */
            bool pop(DATA_TYPE& data, uint32_t timeout_ms = max_delay_ms) {
                if (this->handle == nullptr || this->mode == send_mode::none) {
                    return false;
                }

                uint32_t ticks = 1;
                if (timeout_ms != max_delay_ms){
                    ticks = pdMS_TO_TICKS(timeout_ms);
                }

                return xQueueReceive(this->handle, (void*) &data, ticks) == pdPASS;
            }

            /**
             * @brief Inserts an item into the collection (task context).
             *
             * If `overwrite_if_full` is true and the queue is full, the oldest
             * item is discarded to make room.  Otherwise the call blocks for up
             * to @p timeout_ms milliseconds.
             *
             * @param data       Item to insert (copied by value).
             * @param timeout_ms Maximum wait time in milliseconds.
             * @return true on success, false if the queue is full and the timeout expired.
             */
            bool push(DATA_TYPE data, uint32_t timeout_ms = max_delay_ms) {
                if (this->handle == nullptr || this->mode == send_mode::none) {
                    return false;
                }

                uint32_t ticks = 1;
                if (this->mode == send_mode::single){
                    ticks = 0;
                }
                else if (timeout_ms != max_delay_ms){
                    ticks = pdMS_TO_TICKS(timeout_ms);
                }

                if (this->overwrite_if_full && this->mode != send_mode::single && this->is_full()){
                    DATA_TYPE buffer;
                    if (this->pop(buffer, timeout_ms) == false){
                        return false;
                    }
                }

                return xQueueGenericSend(this->handle, static_cast<void*>(&data), ticks, static_cast<UBaseType_t>(this->mode)) == pdPASS;
            }

            /**
             * @brief Removes and returns the front item (ISR context, non-blocking).
             * @param[out] data Receives the dequeued item.
             * @return true if an item was available and retrieved.
             */
            bool pop_from_isr(DATA_TYPE& data) {
                if (this->handle == nullptr || this->mode == send_mode::none) {
                    return false;
                }

                return xQueueReceiveFromISR(this->handle, (void*) &data, nullptr) == pdPASS;
            }

            /**
             * @brief Inserts an item into the collection (ISR context, non-blocking).
             * @param data Item to insert (copied by value).
             * @return true on success, false if the queue was full.
             */
            bool push_from_isr(DATA_TYPE data) {
                if (this->handle == nullptr || this->mode == send_mode::none) {
                    return false;
                }

                if (this->overwrite_if_full && this->mode != send_mode::single && this->is_full_from_isr()){
                    DATA_TYPE buffer;
                    if (this->pop_from_isr(buffer) == false){
                        return false;
                    }
                }

                return xQueueGenericSendFromISR(this->handle, static_cast<const void*>(&data), nullptr, static_cast<UBaseType_t>(this->mode)) == pdPASS;
            }

            /** @brief Returns the number of items currently in the collection. */
            uint32_t available(void) const {
                return uxQueueMessagesWaiting(this->handle);
            }

            /** @brief Returns the number of free slots remaining in the collection. */
            uint32_t get_free_space(void) const {
                return uxQueueSpacesAvailable(this->handle);
            }

            /** @brief Returns true if the collection contains no items (task context). */
            bool is_empty(void) const {
                return (this->get_free_space() == this->storage_size);
            }

            /** @brief Returns true if the collection has no free slots (task context). */
            bool is_full(void) const {
                return (this->get_free_space() == 0);
            }

            /** @brief Returns true if the collection contains no items (ISR context). */
            bool is_empty_from_isr(void) const {
                return xQueueIsQueueEmptyFromISR(this->handle);
            }

            /** @brief Returns true if the collection has no free slots (ISR context). */
            bool is_full_from_isr(void) const {
                return xQueueIsQueueFullFromISR(this->handle);
            }

            /** @brief Removes all items from the collection. @return true on success. */
            bool clear(void) {
                return xQueueReset(this->handle) == pdPASS;
            }

            /** @brief Returns true if the underlying FreeRTOS queue was successfully created. */
            bool is_valid(void) const {
                return this->handle != nullptr;
            }

            /** @brief Returns the size in bytes of one element stored in the collection. */
            uint16_t get_item_size(void) const{
                return sizeof(DATA_TYPE);
            }

            /** @brief Returns the maximum number of items the collection can hold. */
            uint32_t get_storage_size(void){
                return this->storage_size;
            }

            /**
             * @brief Returns a reference to the raw FreeRTOS queue handle.
             *
             * Use with caution; direct manipulation of the handle bypasses
             * the class invariants.
             */
            queue_handle& get_handle(void) {
                return this->handle;
            }
        };
    }
}