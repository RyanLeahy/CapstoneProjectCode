#include "main.h"

/**
 * @name main
 * 
 * @brief function handles main logic of program. Initializes devices, enables interrupts, sets up events, loops through main program logic.
 * 
 * @authors Ryan Leahy
 * @date 04/13/2024
 * 
 * @cite https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf
*/
void app_main()
{
    /**
     * 
     * Setup portion
     * 
    */

    //For lowering power consumption
    uart_set_baudrate(UART_NUM_0, 9600);

    esp_pm_config_esp32s3_t power_config = {
        .light_sleep_enable = true,
        .max_freq_mhz = 20,
        .min_freq_mhz = 10
    };

    esp_pm_configure(&power_config);

    esp_err_t err;

    //Application specific variables
    bno055_vec3_t angle;
    float speed = 0;
    bool led_on = false;
    bool is_led_on = false;
    int led_on_val = 0;

    //Device specific variables
    i2c_number_t i2c_num = I2C_NUMBER_0; //I2C number for the BNO055 IMU
    nmea_parser_handle_t nmea_handle; //used to interact with the event handle for the NMEA.
    adc_oneshot_unit_handle_t adc_handle; //used to grab data from ADC for photoresistor
    adc_cali_handle_t adc_calibration_handle; //used to calibrate the ADC so the difference between chips is mitigated
    gptimer_handle_t led_timer_handle; //used for the timer that turns the led on and off
    timer_event_handler_args_t event_handler_args = { //used to pass the arguments for the timer handler
        .is_led_on = &is_led_on,
        .led_on = &led_on,
        .led_on_val = &led_on_val,
    };


    if((BNO055_init(&i2c_num)) != ESP_OK)
        goto end_prog;
    
    if((M20048_init(&nmea_handle, &speed)) != ESP_OK)
        goto end_prog;

    if((photoresist_init(&adc_handle, &adc_calibration_handle)) != ESP_OK)
        goto end_prog;

    if((led_init(&led_timer_handle, &event_handler_args)) != ESP_OK)
        goto end_prog;
    
    /**
     * 
     * Loop portion
     * 
    */
    while(true)
    {
        //TEST CODE
        /*
        led_on = true;
        bno055_get_euler(i2c_num, &angle);
        led_on_val = raw_ADC_to_LED_val(photoresist_read(adc_handle, adc_calibration_handle));
        ESP_LOGI(TAG, "Speed: %f", speed);
        ESP_LOGI(TAG, "Photoresist value: %i", photoresist_read(adc_handle, adc_calibration_handle));
        ESP_LOGI(TAG, "ON value: %i", led_on_val);
        ESP_LOGI(TAG, "Angle x = %f  y = %f", angle.x, angle.y);
        vTaskDelay(500/ portTICK_PERIOD_MS);
        */
    


       //PROD CODE
       if(is_led_on == false) //ensure that the ambient light reading is only read when the led is off to ensure no feedback occurs
        led_on_val = raw_ADC_to_LED_val(photoresist_read(adc_handle, adc_calibration_handle));

       bno055_get_euler(i2c_num, &angle);
       led_on = is_out_of_level(&angle, &speed);
       ESP_LOGI(TAG, "Angle x = %f  y = %f Speed: %f LED on value: %i", angle.x, angle.y, speed, led_on_val);
       vTaskDelay(600/ portTICK_PERIOD_MS); //Ensure that the delay value is not divisible by the alarm clock value in led.c or you'll introduce feedback to the photocell from the LED.
    }

    /**
     * 
     * Error/Exit portion
     * 
    */
end_prog:
    err = bno055_close(i2c_num);
    ESP_LOGI(BNO055_TAG, "bno055_close() returned %s \n", esp_err_to_name(err));

    err = nmea_parser_remove_handler(nmea_handle, M20048_event_handler);
    ESP_LOGI(M20048_TAG, "nmea_parser_remove_handler() returned %s \n", esp_err_to_name(err));

    err = nmea_parser_deinit(nmea_handle);
    ESP_LOGI(M20048_TAG, "nmea_parser_deinit() returned %s \n", esp_err_to_name(err));

    err = photoresist_deinit(adc_handle, adc_calibration_handle);
    ESP_LOGI(PHOTORESIST_TAG, "photoresist_deinit() returned %s \n", esp_err_to_name(err));

    err = led_deinit(led_timer_handle);
    ESP_LOGI(LED_TAG, "led_deinit() returned %s \n", esp_err_to_name(err));

    ESP_LOGI(TAG, "Finished\n");
 
}

/**
 * @name raw_ADC_to_percent
 * 
 * @brief function takes in a calibrated ADC voltage reading and converts it to a int value between 0 - 1023
 * 
 * @param raw_ADC_reading integer value with a range of 150 - 2450 (most liekly) pulled from the ADC and calibrated
 * 
 * @return int value with a range of 0 - 1023
 * 
 * @authors Ryan Leahy
 * @date 02/28/2023
*/
int raw_ADC_to_LED_val(int raw_ADC_reading)
{
    //TODO: refine formula
    float intermediate = (raw_ADC_reading - 500)/2598.0; //reduces the raw adc value range down to 0.0 - 1.0

    if(intermediate > 1) //make sure that we can't get more than 1.0
        intermediate = 1;
    
    if(intermediate < 0.1) //make sure we have a minimum on value of 10%
        intermediate = 0.1;

    return intermediate*1023; //scale it back up to 0 - 1023
}

/**
 * @name is_out_of_level
 * 
 * @brief function analyzes input values to determine if the device is out of level
 * 
 * @param angle bno055_vect3_t structure pointer holding the current angle data
 * @param speed float pointer holding the current speed data
 * 
 * @return bool indicating if the device is out of level
 * 
 * @authors Ryan Leahy
 * @date 02/28/2023
*/
bool is_out_of_level(bno055_vec3_t* angle, float* speed)
{
    static uint8_t state;
    bool out_of_level = false;
    float x = angle->x, y = angle->y;
    float combined_angle = sqrt(pow(x, 2) + pow(y, 2));

    //first time entering this function state will be initialized to 0
    if(state == 0)
        state = initial_state;

    switch(state)
    {
        case initial_state: //nothing is on
            if(combined_angle >= threshold_angle && (*speed >= lower_speed && *speed <= upper_speed)) //if the angle and speed are in the ranges, led turns on
            {
                state = threshold_angle_and_speed;
                out_of_level = true;
            }
            else //if no state change occurs, keep led off
                out_of_level = false;
            break;
        case threshold_angle_and_speed:
            if(combined_angle < threshold_angle && (*speed < lower_speed && *speed > upper_speed)) //both the angle and speed need to return to normal to turn led off and return to initial state
            {
                state = initial_state;
                out_of_level = false;
            }
            else //if no state change occurs, keep led on
                out_of_level = true;
            break;
        default:
            out_of_level = false;
    }
    
    return out_of_level;
}