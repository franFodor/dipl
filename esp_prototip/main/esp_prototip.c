#include <stdio.h>
#include <stdlib.h>

#include "driver/i2s_common.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"

#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include <stdint.h>
#include <string.h>


#define BUFFER_SIZE 1024

static const char* TAG = "ESP32";

static i2s_chan_handle_t rx_handle;

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

static void i2s_read_task(void *args) {
  int32_t *r_buf = (int32_t *)calloc(1, BUFFER_SIZE);
  assert(r_buf);
  size_t r_bytes = 0;

  ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));

  while(1) {
    if (i2s_channel_read(rx_handle, r_buf, BUFFER_SIZE, &r_bytes, 1000) == ESP_OK) {
      ESP_LOGI(TAG, "Read task %d bytes", r_bytes);
    } else {
      ESP_LOGI(TAG, "Read task fail!");
    }

    int64_t sum = 0;
    for(int i = 0; i < r_bytes / sizeof(int32_t); i++) {
      sum += abs(r_buf[i]);
    }

    int32_t average_amplitude = sum / (r_bytes / sizeof(int32_t));
    ESP_LOGI(TAG, "%d", average_amplitude);
    // printf("%d\n", average_amplitude);
    
    vTaskDelay(pdMS_TO_TICKS(200));
  }

  free(r_buf);
  //vTaskDelete(NULL);
}

void app_main(void) {
  i2s_setup();

  xTaskCreate(i2s_read_task, "i2s_read_task", 4096, NULL, 5, NULL);
}
