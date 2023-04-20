#ifndef PHOTORESIST_H
#define PHOTORESIST_H

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_err.h"

static const char* PHOTORESIST_TAG = "Photoresist";

esp_err_t photoresist_init(adc_oneshot_unit_handle_t *, adc_cali_handle_t *);
esp_err_t photoresist_deinit(adc_oneshot_unit_handle_t, adc_cali_handle_t);
      int photoresist_read(adc_oneshot_unit_handle_t adc_handle, adc_cali_handle_t calibration_handle);

#endif //PHOTORESIST_H