#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "constants.hpp"

namespace freertos {

    using namespace constants;

    // =========================================================================
    // event_bit_ref
    //
    // Lightweight proxy returned by event_group::operator[].
    // Supports reading and writing a single bit without exposing the underlying
    // EventBits_t word.
    //
    // Design notes:
    //  - Stores a pointer (not reference) to the handle so that an out-of-bounds
    //    operator[] can return a nullptr-handle proxy that silently does nothing.
    //  - Implicit conversions to value types (bool, int8_t, uint8_t, int) work.
    //  - Binding to a reference (bool&) or pointer (bool*) is a compile error —
    //    there is no addressable bit in memory, which is the correct behaviour.
    //  - Must be a standalone (non-nested) class so that implicit conversions
    //    and operator= work without knowing any template parameter of the parent.
    // =========================================================================

    /**
     * @brief Proxy object representing one bit inside an `event_group`.
     *
     * Returned by `event_group::operator[]`.  Supports read and write via
     * implicit conversions and the `<<` / `>>` operators.
     *
     * @note Never store an `event_bit_ref` across statements.  It holds a
     *       pointer into the parent `event_group`; if the group is destroyed
     *       the proxy becomes invalid.
     *
     * @note References and pointers to this proxy (`bool& r = events[i]`,
     *       `bool* p = events[i]`) are **compile errors** — this is intentional.
     *       Bits are not addressable objects; use value copies instead.
     */
    class event_bit_ref {
    private:
        EventGroupHandle_t* handle;   ///< Pointer to the parent's handle (nullptr = no-op proxy).
        uint32_t            index;    ///< Bit position within the event group.

    public:
        /**
         * @brief Constructs a proxy for a specific bit.
         * @param handle Pointer to the underlying FreeRTOS event group handle.
         *               Pass nullptr to create a no-op proxy (used for out-of-bounds safety).
         * @param index  Bit position (0 … max_event_group_bits-1).
         */
        event_bit_ref(EventGroupHandle_t* handle, uint32_t index);

        // ---------------------------------------------------------------------
        // Write
        // ---------------------------------------------------------------------

        /**
         * @brief Sets or clears the bit.
         *
         * Follows the same convention as `std::bitset<N>::reference::operator=`.
         *
         * @code
         * events[0] = true;    // set bit 0
         * events[1] = false;   // clear bit 1
         * events[0] = events[1]; // copy bit 1 into bit 0
         * @endcode
         *
         * @param value true to set the bit, false to clear it.
         * @return *this, allowing chained assignments.
         */
        event_bit_ref& operator=(bool value);

        /**
         * @brief Copies the value of another bit.
         *
         * Allows `events[i] = events[j]` to copy one bit into another.
         * Equivalent to `events[i] = static_cast<bool>(events[j])`.
         *
         * @param other  Source proxy.
         * @return *this.
         */
        event_bit_ref& operator=(const event_bit_ref& other);

        // ---------------------------------------------------------------------
        // Read — implicit conversions (value types only)
        //
        // bool&, bool*, int8_t&, int8_t* etc. are intentionally NOT provided —
        // the compiler will reject those usages with a clear error about binding
        // a reference / pointer to an rvalue, which is the correct behaviour.
        // ---------------------------------------------------------------------

        /** @brief Converts to bool — `bool b = events[i]`. */
        operator bool() const;

        /** @brief Converts to int8_t — `int8_t v = events[i]` (0 or 1). */
        operator int8_t() const;

        /** @brief Converts to uint8_t — `uint8_t v = events[i]` (0 or 1). */
        operator uint8_t() const;

        /** @brief Converts to int — `int v = events[i]` (0 or 1). */
        operator int() const;
    };

    // =========================================================================
    // abstract::event_group
    // =========================================================================

    namespace abstract {

        /**
         * @brief Abstract base for statically- and heap-allocated event groups.
         *
         * An event group holds up to `constants::max_event_group_bits` independent
         * boolean flags (bits).  Each bit can be individually set, cleared, and
         * tested via a concise proxy API.  All operations that modify the group
         * are thread-safe — FreeRTOS manages the internal lock.
         *
         * **Usable bits:** FreeRTOS always reserves the top 8 bits of `EventBits_t`
         * for internal control flags.  The actual limit depends on the platform:
         *  - 32-bit ticks (`configUSE_16_BIT_TICKS = 0`, STM32/ESP32): **24 bits**
         *  - 16-bit ticks (`configUSE_16_BIT_TICKS = 1`, 8-bit AVR):   **8 bits**
         *
         * The constant `constants::max_event_group_bits` always reflects the correct
         * ceiling for the current toolchain without any template parameter needed.
         *
         * **Bit addressing:** bits are numbered 0 … max_event_group_bits-1.
         * Accessing an index >= max_event_group_bits fires `configASSERT` in debug
         * builds.  In release builds (asserts disabled) `operator[]` returns a no-op
         * proxy and `set`/`get` return false, so errors are always detectable.
         *
         * @code
         * freertos::stack::event_group events;
         *
         * events[0] << true;          // set bit 0
         * events[1] << false;         // clear bit 1
         *
         * bool b    = events[2];      // read bit 2 as bool
         * int8_t v  = events[3];      // read bit 3 as int8_t (0 or 1)
         *
         * events[4] >> b;             // read bit 4 into b
         * b         << events[5];     // read bit 5 into b (alternative)
         *
         * if (!events.set(30, true))  { // out-of-bounds: returns false }
         * // events[30] << true;      // out-of-bounds: configASSERT + no-op
         *
         * for (const auto& bit : events) {
         *     bool value = bit;       // iterate all max_event_group_bits bits
         * }
         * @endcode
         */
        class event_group {
        protected:
            EventGroupHandle_t handle {nullptr}; ///< Underlying FreeRTOS event group handle.

        public:
            /** @brief Deletes the underlying FreeRTOS event group on destruction. */
            ~event_group(void);

            // -----------------------------------------------------------------
            // operator[]
            // -----------------------------------------------------------------

            /**
             * @brief Returns a read/write proxy for bit @p index.
             *
             * Fires `configASSERT` if @p index >= max_event_group_bits.  In release
             * builds with asserts disabled, returns a no-op proxy (reads yield false;
             * writes are silently discarded).
             *
             * @param index  Bit position (0 … max_event_group_bits-1).
             * @return An `event_bit_ref` proxy bound to the requested bit.
             */
            event_bit_ref operator[](uint32_t index);

            // -----------------------------------------------------------------
            // set / get
            // -----------------------------------------------------------------

            /**
             * @brief Sets or clears bit @p index.
             *
             * @param index  Bit position (0 … max_event_group_bits-1).
             * @param value  true to set, false to clear.
             * @return true on success; false if @p index >= max_event_group_bits or the group is invalid.
             */
            bool set(uint32_t index, bool value);

            /**
             * @brief Reads bit @p index.
             *
             * @param  index  Bit position (0 … max_event_group_bits-1).
             * @param[out] out  Receives the current bit value.
             * @return true on success; false if @p index >= max_event_group_bits or the group is invalid.
             */
            bool get(uint32_t index, bool& out) const;

            // -----------------------------------------------------------------
            // ISR variants
            // -----------------------------------------------------------------

            /**
             * @brief Sets or clears bit @p index from ISR context.
             *
             * @note `xEventGroupSetBitsFromISR` may defer the set to the FreeRTOS
             *       daemon task, so the bit is not guaranteed to be set by the time
             *       the ISR returns.  `xEventGroupClearBitsFromISR` behaves the same way.
             *
             * @param index  Bit position (0 … max_event_group_bits-1).
             * @param value  true to set, false to clear.
             * @return true on success (or if enqueued successfully).
             */
            bool set_from_isr(uint32_t index, bool value);

            /**
             * @brief Reads bit @p index from ISR context (non-blocking).
             *
             * @param  index  Bit position (0 … max_event_group_bits-1).
             * @param[out] out  Receives the current bit value.
             * @return true on success.
             */
            bool get_from_isr(uint32_t index, bool& out) const;

            // -----------------------------------------------------------------
            // Utility
            // -----------------------------------------------------------------

            /** @brief Returns true if the event group was created successfully. */
            bool is_valid(void) const;

            // -----------------------------------------------------------------
            // Range-based for
            // -----------------------------------------------------------------

            /**
             * @brief Forward iterator over all max_event_group_bits bits.
             *
             * Dereferences to an `event_bit_ref` proxy, supporting both read and write:
             *
             * @code
             * for (const auto& bit : events) {
             *     bool value = bit;   // read
             * }
             * @endcode
             */
            class iterator {
            private:
                event_group* parent;
                uint32_t     index;

            public:
                /** @brief Constructs an iterator at @p index. */
                iterator(event_group* parent, uint32_t index);

                /** @brief Dereferences to a proxy for the current bit. */
                event_bit_ref operator*(void);

                /** @brief Advances to the next bit. */
                iterator& operator++(void);

                /** @brief Returns true while the loop has not reached the end. */
                bool operator!=(const iterator& other) const;
            };

            /** @brief Returns an iterator to bit 0. */
            iterator begin(void);

            /** @brief Returns an iterator past the last bit. */
            iterator end(void);
        };

    } // namespace abstract

    // =========================================================================
    // stack::event_group
    // =========================================================================

    namespace stack {

        /**
         * @brief Statically-allocated event group.
         *
         * The FreeRTOS control block is stored as a member variable; no heap
         * allocation is performed.  Prefer this variant in production firmware
         * where deterministic memory usage is required.
         *
         * Supports up to `constants::max_event_group_bits` independent flag bits
         * (24 on 32-bit platforms, 8 on 16-bit platforms).
         */
        class event_group : public abstract::event_group {
        private:
            StaticEventGroup_t buffer; ///< Static FreeRTOS event group control block.

        public:
            /** @brief Creates the event group with static allocation. */
            event_group(void);

            using abstract::event_group::operator[];
            using abstract::event_group::set;
            using abstract::event_group::get;
            using abstract::event_group::set_from_isr;
            using abstract::event_group::get_from_isr;
            using abstract::event_group::is_valid;
            using abstract::event_group::begin;
            using abstract::event_group::end;
        };

    } // namespace stack

    // =========================================================================
    // heap::event_group
    // =========================================================================

    namespace heap {

        /**
         * @brief Heap-allocated event group.
         *
         * The FreeRTOS control block is allocated via `pvPortMalloc` at
         * construction time and freed automatically on destruction.
         *
         * Supports up to `constants::max_event_group_bits` independent flag bits
         * (24 on 32-bit platforms, 8 on 16-bit platforms).
         */
        class event_group : public abstract::event_group {
        private:
        public:
            /** @brief Creates the event group with dynamic allocation. */
            event_group(void);

            using abstract::event_group::operator[];
            using abstract::event_group::set;
            using abstract::event_group::get;
            using abstract::event_group::set_from_isr;
            using abstract::event_group::get_from_isr;
            using abstract::event_group::is_valid;
            using abstract::event_group::begin;
            using abstract::event_group::end;
        };

    } // namespace heap

} // namespace freertos
