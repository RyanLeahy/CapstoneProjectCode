#include "led.h"
#include "esp_log.h"

#define ALARM_TIME (2000) //amount of time in milliseconds the timer will run before the alarm is triggered
#define PWM_FREQ (5*10e3) //the frequency at which the PWM signal operates at
#define LED_GPIO (42)

static bool led_alarm_handler(gptimer_handle_t timer_handle, const gptimer_alarm_event_data_t *event_data, void* user_args)
{
    esp_err_t err;
    static uint8_t led_toggle; //leverage the fact that static variables get initialized to 0 as a check if its ever been initialized

    //if the toggle is zero this is the first time running this handler, initialize it
    if(led_toggle == 0)
    {
        led_toggle = 1; //were going to use the first bit as an indicator that the variable has been initialized and the second bit as the actual toggle bit
    }

    //pulls the passed in data from the initializer out of a structure
    timer_event_handler_args_t *args = (timer_event_handler_args_t *)user_args;
    bool *is_led_on = args->is_led_on;
    bool *led_on = args->led_on;
    int *led_on_val = args->led_on_val;

    if(*led_on) //Toggle PWM
    {
        if(led_toggle >> 1) //if the second bit is a 1 then turn the led on. Use the percentage and multiply it by the max duty resolution value to scale it.
        {
            *is_led_on = true; //indicate back to main that the led is on
            err = ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, *led_on_val);
            ESP_LOGD(LED_TAG, "led_alarm_handler(): ledc_set_duty turning on the led returned %s", esp_err_to_name(err));
            ESP_ERROR_CHECK(err);

            err = ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
            ESP_LOGD(LED_TAG, "led_alarm_handler(): ledc_update_duty turning on the led returned %s", esp_err_to_name(err));
            ESP_ERROR_CHECK(err);
            
        }
        else //turn off led if the second bit is not 1
        {
            *is_led_on = false; //indicate back to main that the led is off
            err = ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
            ESP_LOGD(LED_TAG, "led_alarm_handler(): ledc_set_duty turning off the led returned %s", esp_err_to_name(err));
            ESP_ERROR_CHECK(err);

            err = ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
            ESP_LOGD(LED_TAG, "led_alarm_handler(): ledc_update_duty turning off the led returned %s", esp_err_to_name(err));
            ESP_ERROR_CHECK(err);
        }
        led_toggle = led_toggle ^ 0x02; //xor the second bit causing it to toggle states.
    }
    else //Disable PWM
    {
        *is_led_on = false; //indicate back to main that the led is off
        err = ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
        ESP_LOGD(LED_TAG, "led_alarm_handler(): ledc_set_duty disabling the led returned %s", esp_err_to_name(err));
        ESP_ERROR_CHECK(err);

        err = ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
        ESP_LOGD(LED_TAG, "led_alarm_handler(): ledc_update_duty disabling the led returned %s", esp_err_to_name(err));
        ESP_ERROR_CHECK(err);
    }

    return true;
}

/**
 * @name led_init
 * 
 * @brief function initializes the timer module used for flashing and the PWM module that sends the flashing
 * 
 * @param timer_handle holds the handle for the timer to be used in setting up the timer and handing it off to the alarm handler
 * @param led_on used in the alarm handler to turn on the LED or not
 * @param led_on_pct used in the alarm handler to control the brightness of the LED
 * 
 * @return err variable that lets you know if everything was successfully initialized or not
 * 
 * @authors Ryan Leahy
 * @date 02/15/2023
 * 
 * @cite https://docs.espressif.com/projects/esp-idf/en/v5.0/esp32s3/api-reference/peripherals/gptimer.html
 * @cite https://docs.espressif.com/projects/esp-idf/en/v5.0/esp32s3/api-reference/peripherals/ledc.html
*/
esp_err_t led_init(gptimer_handle_t *timer_handle, timer_event_handler_args_t *event_handler_args)
{
    esp_err_t err;

    gptimer_config_t config = {
      .clk_src = GPTIMER_CLK_SRC_DEFAULT,
      .direction = GPTIMER_COUNT_UP,
      .resolution_hz = 1 * 10e3, //1kHz, 1 tick = 1 ms  
    };

    if((err = gptimer_new_timer(&config, timer_handle)) != ESP_OK)
    {
        ESP_LOGD(LED_TAG, "led_init(): gptimer_new_timer returned %s", esp_err_to_name(err));
        return err;
    }

    gptimer_alarm_config_t alarm_config = {
        .alarm_count = ALARM_TIME, 
        .reload_count = 0,
        .flags.auto_reload_on_alarm = true,
    }; 

    if((err = gptimer_set_alarm_action(*timer_handle, &alarm_config)) != ESP_OK)
    {
        ESP_LOGD(LED_TAG, "led_init(): gptimer_set_alarm_action returned %s", esp_err_to_name(err));
        return err;
    }

    gptimer_event_callbacks_t timer_event_handler = {
        .on_alarm = led_alarm_handler,
    };

    //Registers the event handler for when the alarm goes off
    if((err = gptimer_register_event_callbacks(*timer_handle, &timer_event_handler, ((void *)event_handler_args))) != ESP_OK)
    { 
        ESP_LOGD(LED_TAG, "led_init(): gptimer_register_event_callbacks returned %s", esp_err_to_name(err));
        return err;
    }

    if((err = gptimer_enable(*timer_handle)) != ESP_OK)
    {
        ESP_LOGD(LED_TAG, "led_init(): gptimer_enable returned %s", esp_err_to_name(err));
        return err;
    }

    //Before starting the timer. Setup the LED.
    ledc_timer_config_t led_timer_config = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_10_BIT, 
        .freq_hz = PWM_FREQ,
        .clk_cfg = LEDC_AUTO_CLK,
    };

    if((err = ledc_timer_config(&led_timer_config)) != ESP_OK)
    {
        ESP_LOGD(LED_TAG, "led_init(): ledc_timer_config returned %s", esp_err_to_name(err));
        return err;
    }

    ledc_channel_config_t led_channel_config = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = LED_GPIO,
        .duty = 0, //initially the LED will be off
        .hpoint = 0,
    };

    if((err = ledc_channel_config(&led_channel_config)) != ESP_OK)
    {
        ESP_LOGD(LED_TAG, "led_init(): ledc_channel_config returned %s", esp_err_to_name(err));
        return err;
    }

    if((err = gptimer_start(*timer_handle)) != ESP_OK)
    {
        ESP_LOGD(LED_TAG, "led_init(): gptimer_start returned %s", esp_err_to_name(err));
        return err;
    }

    return err;
}

/**
 * @name led_deinit
 * 
 * @brief function deletes the PWM and Timer modules
 * 
 * @param timer_handle holds the handle for the timer to be used in deleting the timer
 * 
 * @return err variable that lets you know if everything was successfully deinitialized or not
 * 
 * @authors Ryan Leahy
 * @date 02/15/2023
*/
esp_err_t led_deinit(gptimer_handle_t timer_handle)
{
    esp_err_t err;

    if((err = gptimer_disable(timer_handle)) != ESP_OK)
    {
        ESP_LOGD(LED_TAG, "led_deinit(): gptimer_disable returned %s", esp_err_to_name(err));
        return err;
    }

    if((err = gptimer_del_timer(timer_handle)) != ESP_OK)
    {
        ESP_LOGD(LED_TAG, "led_deinit(): gptimer_del_timer returned %s", esp_err_to_name(err));
        return err;
    }

    return err;
}
