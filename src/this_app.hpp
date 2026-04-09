#pragma once

#include "task.hpp"

namespace freertos {

    /**
     * @brief Semantic alias for `this_task` intended for use inside `abstract::app` subclasses.
     *
     * `this_app` inherits every static utility from `this_task` (`delay`, `suspend`,
     * `yield`, `get_handle`, `get_info`, `get_notification`, `delay_for`, `suspend_for`)
     * and adds no new behaviour.  Its sole purpose is readability: inside application
     * code written against `abstract::app` it communicates intent more clearly than the
     * generic `this_task` name.
     *
     * @code
     * class my_app : public freertos::abstract::app {
     *     static void run(my_app& self) {
     *         while (true) {
     *             // Prefer this_app over this_task inside app code.
     *             freertos::this_app::delay(1000);
     *         }
     *     }
     * };
     * @endcode
     *
     * @note Both `this_app` and `this_task` refer to the **currently running task**.
     *       They are interchangeable at runtime; the distinction is purely semantic.
     */
    class this_app : public this_task {};

}