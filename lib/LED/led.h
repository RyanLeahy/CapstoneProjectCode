#ifndef LED_H
#define LED_H

#include "esp_types.h"
#include "esp_event.h"
#include "esp_err.h"
#include "driver/gptimer.h"
#include "driver/ledc.h"

static const char* LED_TAG = "LED";

typedef struct {
    bool *is_led_on;
    bool *led_on;
    int *led_on_val;
} timer_event_handler_args_t;

esp_err_t led_init(gptimer_handle_t *, timer_event_handler_args_t *);
esp_err_t led_deinit(gptimer_handle_t );

#endif