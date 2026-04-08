#pragma once

#include "task.hpp"

namespace freertos {
    namespace abstract {

        /**
         * @brief Base class for application objects that wrap a FreeRTOS task.
         *
         * Derive from `app` to create a self-contained application unit with a
         * clean `start` / `stop` lifecycle.  The constructor requires a reference
         * to an `abstract::task` (typically a `stack::task` member) that will be
         * resumed on `start()` and suspended on `stop()`.
         *
         * The singleton pattern is recommended for concrete subclasses so that
         * the owning task's static storage is guaranteed to outlive the scheduler.
         *
         * @code
         * class my_app : public freertos::abstract::app {
         *     freertos::stack::task<2048, my_app&> task;
         *     static void run(my_app& self);
         *     my_app() : freertos::abstract::app(task),
         *                task("my_app", 1, *this, my_app::run) {}
         * public:
         *     static my_app& instance() { static my_app i; return i; }
         * };
         * @endcode
         */
        class app {
            private:
                freertos::abstract::task& task;
            protected:
                /**
                 * @brief Constructs the application and binds it to a task.
                 * @param task Reference to the task that drives this application.
                 */
                app(freertos::abstract::task& task);
            public:
                /**
                 * @brief Starts (resumes) the application task.
                 * @param is_from_isr Set to true when calling from an ISR context.
                 * @return true if the task was successfully resumed.
                 */
                bool start(bool is_from_isr = false);

                /**
                 * @brief Stops (suspends) the application task and waits for it to finish.
                 *
                 * Calls `task.suspend()` followed by `task.join()` to ensure the task
                 * has fully stopped before returning.
                 *
                 * @param is_from_isr Set to true when calling from an ISR context.
                 * @return true if the task was successfully suspended.
                 */
                bool stop(bool is_from_isr = false);

                /**
                 * @brief Returns whether the application task is currently running.
                 * @return true if the task is active, false if suspended or deleted.
                 */
                bool is_running(void);
        };
    }

    /**
     * @brief Alias for `this_task` — current-task utilities in an application context.
     *
     * Allows writing `freertos::this_app::delay(ms)` inside application task
     * callbacks for readability.
     */
    using this_app = this_task;
}