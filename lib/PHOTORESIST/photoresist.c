#include "photoresist.h"
#include "esp_log.h"

/**
 * @name photoresist_init
 * 
 * @brief function handles initializing the ADC on GPIO 1 that reads the photo resistor value that is used to control the brightness of the LED.
 * 
 * @param adc_handle handle that holds the adc oneshot handle, helps us access the ADC we need to read the voltage.
 * @param calibration_hanlde handle that holds the adc calibration information for getting calibrated voltage values back.
 * 
 * @return err variable that lets you know if everything was successfully initialized or not
 * 
 * @authors Ryan Leahy
 * @date 02/22/2024
 * 
 * @cite https://docs.espressif.com/projects/esp-idf/en/v5.0/esp32s3/api-reference/peripherals/adc_oneshot.html
*/
esp_err_t photoresist_init(adc_oneshot_unit_handle_t *adc_handle, adc_cali_handle_t *calibration_handle)
{
    esp_err_t err;

    adc_oneshot_unit_init_cfg_t unit_config = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };

    //Add a oneshot handle to ADC1. Oneshot means it only reads when we tell it to, not continuously reading.
    if((err = adc_oneshot_new_unit(&unit_config, adc_handle)) != ESP_OK)
    {
        ESP_LOGD(PHOTORESIST_TAG, "photoresist_init(): adc_oneshot_new_unit returned %s", esp_err_to_name(err));
        return err;
    }

    adc_oneshot_chan_cfg_t channel_config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_11, 
    };

    /**
     * Configure ADC1 Channel 0 (GPIO1) to oneshot mode attached to our handle.
     * Bitwidth is the default for the esp32-s3 of 12 bits (4096 value resolution).
     * Attenuation is set to 11 dB to allow a voltage input range of 150mV ~ 2450mV.
    */ 
    if((err = adc_oneshot_config_channel(*adc_handle, ADC_CHANNEL_0, &channel_config)) != ESP_OK)
    {
        ESP_LOGD(PHOTORESIST_TAG, "photoresist_init(): adc_oneshot_config_channel returned %s", esp_err_to_name(err));
        return err;
    }


    adc_cali_curve_fitting_config_t calibration_config = {
        .unit_id = ADC_UNIT_1,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_11,
    };

    //Setup calibration handle for calibrating the ADC.
    if((err = adc_cali_create_scheme_curve_fitting(&calibration_config, calibration_handle)) != ESP_OK)
    {
        ESP_LOGD(PHOTORESIST_TAG, "photoresist_init(): adc_cali_create_scheme_curve_fitting returned %s", esp_err_to_name(err));
        return err;
    }

    return err;
}

/**
 * @name photoresist_deinit
 * 
 * @brief function handles deinitializing the ADC module
 * 
 * @param adc_handle the adc oneshot unit handle that will be deleted
 * @param calibration_handle the adc calibration handle that will be deleted
 * 
 * @return err variable that lets you know if everything was successfully deinitialized or not
 * 
 * @authors Ryan Leahy
 * @date 02/15/2024
*/
esp_err_t photoresist_deinit(adc_oneshot_unit_handle_t adc_handle, adc_cali_handle_t calibration_handle)
{
    esp_err_t err;

    if((err = adc_cali_delete_scheme_curve_fitting(calibration_handle)) != ESP_OK)
    {
        ESP_LOGD(PHOTORESIST_TAG, "photoresist_deinit(): adc_cali_delete_scheme_curve_fitting returned %s", esp_err_to_name(err));
        return err;
    }

    if((err = adc_oneshot_del_unit(adc_handle)) != ESP_OK)
    {
        ESP_LOGD(PHOTORESIST_TAG, "photoresist_deinit(): adc_oneshot_del_unit returned %s", esp_err_to_name(err));
        return err;
    }

    return err; 
}

/**
 * @name photoresist_read
 * 
 * @brief function handles reading the ADC and returning the value.
 * 
 * ADC mV  range: 150mV ~ 2450mV
 * 
 * @param adc_handle needed to access the correct adc to read its value.
 * @param calibration_handle needed to access the calibration information that helps us return a calibrated voltage value.
 * 
 * @return integer containing the calibrated voltage ADC reading in the range of 150mV-2450mV.
 * 
 * @authors Ryan Leahy
 * @date 02/21/2024
*/
int photoresist_read(adc_oneshot_unit_handle_t adc_handle, adc_cali_handle_t calibration_handle)
{
    esp_err_t err;
    int raw_ADC_reading;
    int calibrated_voltage;

    err = adc_oneshot_read(adc_handle, ADC_CHANNEL_0, &raw_ADC_reading);
    ESP_LOGD(PHOTORESIST_TAG, "photoresist_read(): adc_oneshot_read returned %s", esp_err_to_name(err));
    ESP_ERROR_CHECK(err);

    err = adc_cali_raw_to_voltage(calibration_handle, raw_ADC_reading, &calibrated_voltage);
    ESP_LOGD(PHOTORESIST_TAG, "photoresist_read(): adc_cali_raw_to_voltage returned %s", esp_err_to_name(err));
    ESP_ERROR_CHECK(err);

    return calibrated_voltage;
}