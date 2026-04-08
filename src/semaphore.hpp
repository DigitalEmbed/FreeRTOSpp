#pragma once

#include "constants.hpp"
#include "typedefs.hpp"

namespace freertos {

    using namespace typedefs;
    using namespace constants;

    namespace abstract {

        /**
         * @brief Abstract base class for all FreeRTOS synchronization primitives.
         *
         * Provides a unified interface over binary semaphores, counting semaphores,
         * mutexes, and recursive mutexes.  Concrete types are created by the
         * subclasses in the `freertos::heap` and `freertos::stack` namespaces.
         *
         * The destructor automatically deletes the underlying FreeRTOS object.
         *
         * @note `give_from_isr` and `take_from_isr` are not supported for mutex or
         *       recursive-mutex types and will return false if called on them.
         */
        class semaphore {
        protected:
            /** @brief Underlying FreeRTOS semaphore/mutex handle. */
            semaphore_handle handle {nullptr};

            /** @brief Discriminates the concrete semaphore kind at runtime. */
            enum class type {
                none      = -1, ///< Uninitialized / invalid.
                binary    =  0, ///< Binary semaphore.
                counting  =  1, ///< Counting semaphore.
                mutex     =  2, ///< Non-recursive mutex.
                recursive =  3, ///< Recursive mutex.
            };

            type semaphore_type {type::none};

            /**
             * @brief Protected constructor; called by concrete subclasses.
             * @param semaphore_type The kind of semaphore being constructed.
             */
            semaphore(type semaphore_type);

        public:
            /** @brief Destroys the semaphore and frees the associated FreeRTOS resource. */
            ~semaphore(void);

            /**
             * @brief Checks whether the semaphore was successfully created.
             * @return true if the handle is valid, false otherwise.
             */
            bool is_valid(void) const;

            /**
             * @brief Returns a reference to the raw FreeRTOS handle.
             * @return Reference to the internal semaphore_handle.
             */
            semaphore_handle& get_handle(void);

            /**
             * @brief Returns whether this semaphore can be used from an ISR context.
             *
             * Only binary and counting semaphores support ISR-safe give/take.
             * Mutexes and recursive mutexes do **not** — calling `give_from_isr`
             * or `take_from_isr` on them always returns false.
             *
             * @return true for binary and counting semaphores, false for mutexes.
             */
            bool supports_isr(void) const;

            /**
             * @brief Releases the semaphore from a task context.
             *
             * For mutexes this corresponds to unlocking; for semaphores it
             * increments the count (or sets the binary flag).
             *
             * @return true on success, false if the operation failed.
             */
            virtual bool give(void);

            /**
             * @brief Acquires the semaphore from a task context.
             *
             * Blocks for up to @p timeout_ms milliseconds waiting for the
             * semaphore to become available.
             *
             * @param timeout_ms Maximum time to wait in milliseconds.
             *                   Use `constants::max_delay_ms` to wait forever.
             * @return true if acquired, false on timeout.
             */
            virtual bool take(uint32_t timeout_ms = max_delay_ms);

            /**
             * @brief Releases the semaphore from an ISR context.
             *
             * Must only be called from an interrupt service routine.
             * Not supported for mutex or recursive mutex types.
             *
             * @return true on success, false if not supported or failed.
             */
            virtual bool give_from_isr(void);

            /**
             * @brief Acquires the semaphore from an ISR context (non-blocking).
             *
             * Must only be called from an interrupt service routine.
             * Not supported for mutex or recursive mutex types.
             *
             * @return true if acquired, false if not available or not supported.
             */
            virtual bool take_from_isr(void);
        };
    }
}