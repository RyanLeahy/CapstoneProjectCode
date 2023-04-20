#ifndef MAIN_H
#define MAIN_H

#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h" // data types
#include "freertos/task.h"     // vTaskDelay
#include "esp_log.h"
#include "esp_pm.h"

#include "bno055.h"
#include "nmea_parser.h"
#include "led.h"
#include "photoresist.h"

#include "parameters.h"

static const char* TAG = "main";

typedef enum stateMachine {
    initial_state = 1,
    threshold_angle_and_speed = 2,
} out_of_level_t;

int raw_ADC_to_LED_val(int);
bool is_out_of_level(bno055_vec3_t*, float*);

#endif //MAIN_H