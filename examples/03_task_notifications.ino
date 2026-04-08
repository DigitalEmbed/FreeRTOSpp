/**
 * @file 03_task_notifications.cpp
 * @brief Task-to-task communication using the FreeRTOS++ notification API.
 *
 * This example demonstrates:
 *  - Waiting for a task notification with a timeout (get_notification).
 *  - Sending a value from one task to another with send_value().
 *  - Monitoring free stack memory at runtime with get_info().
 *  - Using the `freertos` namespace directly to reduce verbosity.
 *
 * Target: ESP32 / STM32 with Arduino framework or ESP-IDF.
 */

#include <Arduino.h>
#include "freertos.hpp"
#include "esp_log.h"

using namespace freertos;

static const char* TAG_RX = "receiver";
static const char* TAG_TX = "sender";

void setup() {
    // -----------------------------------------------------------------------
    // Receiver task
    //
    // - Logs free stack memory every iteration (useful for sizing tasks).
    // - Waits up to 100 ms for a notification.
    // - If a notification arrives, logs its value, then suspends itself.
    //   The sender is responsible for resuming it for the next round.
    // -----------------------------------------------------------------------
    static stack::task<2048> receiver("receiver", 1, []() {
        while (true) {
            ESP_LOGI(TAG_RX, "free stack: %lu bytes",
                     this_task::get_info().get_free_stack_memory());

            uint32_t value = 0;
            if (this_task::get_notification(value, /*index=*/0, /*timeout_ms=*/100)) {
                ESP_LOGI(TAG_RX, "received notification: %lu", value);
            }

            // Suspend until the sender calls resume().
            this_task::suspend();
        }
    });
    receiver.resume();

    // -----------------------------------------------------------------------
    // Sender task
    //
    // - Sends an incrementing value to the receiver every second.
    // - Uses send_value() which only succeeds if the previous notification
    //   has already been consumed — preventing message loss.
    // - Resumes the receiver after a successful send so it can process the
    //   notification.
    // -----------------------------------------------------------------------
    static stack::task<2048, abstract::task&> sender("sender", 1, receiver,
        [](abstract::task& rx) {
            static uint32_t seq = 0;

            while (true) {
                ESP_LOGI(TAG_TX, "free stack: %lu bytes",
                         this_task::get_info().get_free_stack_memory());

                // send_value() returns false if the receiver has not yet read
                // the previous notification — safe, no data overwritten.
                if (rx.get_notifier().send_value(seq, /*index=*/0, /*resume=*/true)) {
                    ESP_LOGI(TAG_TX, "sent: %lu", seq);
                    seq++;
                } else {
                    ESP_LOGW(TAG_TX, "notification pending — skipping send");
                }

                this_task::delay(1000);
            }
        });
    sender.resume();
}

void loop() {
    this_task::suspend();
}
