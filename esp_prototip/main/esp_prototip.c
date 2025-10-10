#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "driver/i2s_common.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "dsps_fft2r.h"
#include "dsps_wind_hann.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_dsp.h"
#include "sdkconfig.h"
#include "math.h"

#define BUFFER_SIZE 512

static const char* TAG = "ESP32";
static i2s_chan_handle_t rx_handle;

float y[BUFFER_SIZE * 2];
float wind[BUFFER_SIZE];

static void i2s_setup(void) {
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);

  ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_handle));

  i2s_std_config_t std_cfg = {
    .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(48000),
    .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
    .gpio_cfg = {
      .mclk = I2S_GPIO_UNUSED,
      .bclk = GPIO_NUM_26,
      .ws = GPIO_NUM_25,
      .dout = I2S_GPIO_UNUSED,
      .din = GPIO_NUM_22,
      .invert_flags = {
        .mclk_inv = false,
        .bclk_inv = false,
        .ws_inv = false,
      },
    },
  };

  ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle, &std_cfg));

  ESP_LOGI(TAG, "I2S RX setup complete.");
  return;
}

static void compute_fft(float *y, size_t n) {
  dsps_fft2r_fc32(y, BUFFER_SIZE);
  dsps_bit_rev_fc32(y, BUFFER_SIZE);
  dsps_cplx2reC_fc32(y, BUFFER_SIZE);

  // TODO
  for (int i = 0 ; i < BUFFER_SIZE / 2 ; i++) {
      y[i] = 10 * log10f((y[i * 2 + 0] * y[i * 2 + 0] + y[i * 2 + 1] * y[i * 2 + 1]) / BUFFER_SIZE);
  }

  // Show power spectrum in 64x10 window from -100 to 0 dB from 0..N/4 samples
  ESP_LOGW(TAG, "Signal x1");
  dsps_view(y, BUFFER_SIZE / 2, 64, 10,  -600, 400, '|');
}

static void i2s_read_task(void *args) {
  int32_t r_buf[BUFFER_SIZE];
  size_t r_bytes = 0;

  ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));

  while(1) {
    if (i2s_channel_read(rx_handle, r_buf, sizeof(r_buf), &r_bytes, 1000) == ESP_OK) {
      ESP_LOGI(TAG, "Read task %zu bytes, buffer size %d", r_bytes, sizeof(r_buf));
      for (int i = 0; i < BUFFER_SIZE; i++) {
        y[i * 2 + 0] = r_buf[i] * wind[i];
        y[i * 2 + 1] = 0;
      } 
    } else {
      ESP_LOGI(TAG, "Read task fail!");
    }

    // TODO remove later to avoid uneccesary calls, currenly just for testing
    compute_fft(y, BUFFER_SIZE * 2);

    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  vTaskDelete(NULL);
}

void app_main(void) {
  // i2s
  i2s_setup();

  // esp_dsp - fft
  ESP_ERROR_CHECK(dsps_fft2r_init_fc32(NULL, CONFIG_DSP_MAX_FFT_SIZE));
  dsps_wind_hann_f32(wind, BUFFER_SIZE);

  xTaskCreate(i2s_read_task, "i2s_read_task", 4096, NULL, 5, NULL);
}
