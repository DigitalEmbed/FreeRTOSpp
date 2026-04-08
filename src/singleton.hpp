#pragma once

#include "collection.hpp"

namespace freertos {
    namespace stack {
        /**
         * @brief Statically-allocated single-slot queue that always holds the latest value.
         *
         * A `singleton` is a queue with a capacity of exactly one item.  Every call
         * to `push()` overwrites any previously stored value without blocking, so
         * the reader always retrieves the most recently written item.
         *
         * Typical use: sharing the current state of a sensor or flag between a
         * producer task and one or more consumer tasks.
         *
         * @tparam DATA_TYPE Type of the stored element.  Must be trivially copyable.
         */
        template <typename DATA_TYPE>
        class singleton : public abstract::collection<DATA_TYPE> {
        private:
            queue_struct buffer;                        ///< Static FreeRTOS queue control block.
            uint8_t storage_area[sizeof(DATA_TYPE)];    ///< Raw byte storage for the single item.
        public:
            /** @brief Creates the singleton queue with static allocation. */
            singleton(void) :
            abstract::collection<DATA_TYPE>(abstract::collection<DATA_TYPE>::send_mode::single, true, 1)
            {
                if (this->handle != nullptr) {
                    return;
                }

                this->handle = xQueueCreateStatic(1, sizeof(DATA_TYPE), this->storage_area, &this->buffer);
            }

            using abstract::collection<DATA_TYPE>::pop;
            using abstract::collection<DATA_TYPE>::push;
            using abstract::collection<DATA_TYPE>::pop_from_isr;
            using abstract::collection<DATA_TYPE>::push_from_isr;
            using abstract::collection<DATA_TYPE>::available;
            using abstract::collection<DATA_TYPE>::get_free_space;
            using abstract::collection<DATA_TYPE>::clear;
            using abstract::collection<DATA_TYPE>::is_valid;
            using abstract::collection<DATA_TYPE>::is_empty;
            using abstract::collection<DATA_TYPE>::is_full;
            using abstract::collection<DATA_TYPE>::is_empty_from_isr;
            using abstract::collection<DATA_TYPE>::is_full_from_isr;
            using abstract::collection<DATA_TYPE>::get_item_size;
            using abstract::collection<DATA_TYPE>::get_storage_size;
            using abstract::collection<DATA_TYPE>::get_handle;
        };
    }
}
