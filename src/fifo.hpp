#pragma once

#include "collection.hpp"

namespace freertos {

    namespace heap {
        /**
         * @brief Heap-allocated first-in, first-out queue.
         *
         * Items are inserted at the back and removed from the front, preserving
         * arrival order (FIFO).  The underlying FreeRTOS queue is allocated on
         * the heap via `xQueueCreate()`.
         *
         * @tparam DATA_TYPE Type of the elements.  Must be trivially copyable.
         */
        template <typename DATA_TYPE>
        class fifo : public abstract::collection<DATA_TYPE> {
        private:
        public:
            /**
             * @brief Creates a heap-allocated FIFO queue.
             * @param storage_size    Maximum number of items the queue can hold.
             * @param overwrite_if_full When true, the oldest item is discarded on
             *                          overflow instead of blocking.  Default false.
             */
            fifo(uint32_t storage_size, bool overwrite_if_full = false) :
            abstract::collection<DATA_TYPE>(abstract::collection<DATA_TYPE>::send_mode::to_back, overwrite_if_full, storage_size)
            {
                if (this->handle != nullptr) {
                    return;
                }

                this->handle = xQueueCreate(storage_size, sizeof(DATA_TYPE));
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

    namespace stack {
        /**
         * @brief Statically-allocated first-in, first-out queue.
         *
         * Identical in behaviour to `heap::fifo` but the queue control block
         * and storage area are kept as member variables, so no heap allocation
         * occurs.  The capacity is a compile-time template parameter.
         *
         * @tparam STORAGE_SIZE Maximum number of items (compile-time constant).
         * @tparam DATA_TYPE    Type of the elements.  Must be trivially copyable.
         */
        template <uint32_t STORAGE_SIZE, typename DATA_TYPE>
        class fifo : public abstract::collection<DATA_TYPE> {
        private:
            queue_struct buffer;
            uint8_t storage_area[STORAGE_SIZE * sizeof(DATA_TYPE)];
        public:
            /**
             * @brief Creates the statically-allocated FIFO queue.
             * @param overwrite_if_full When true, the oldest item is discarded on
             *                          overflow instead of blocking.  Default false.
             */
            fifo(bool overwrite_if_full = false) :
            abstract::collection<DATA_TYPE>(abstract::collection<DATA_TYPE>::send_mode::to_back, overwrite_if_full, STORAGE_SIZE) 
            {
                if (this->handle != nullptr) {
                    return;
                }

                this->handle = xQueueCreateStatic(STORAGE_SIZE, sizeof(DATA_TYPE), this->storage_area, &this->buffer);
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