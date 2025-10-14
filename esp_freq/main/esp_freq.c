#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2s_common.h"
#include "driver/i2s_std.h"

#include "Yin.h"

#define BUFFER_SIZE 2048
// needs to be the same as in Yin.h
#define SAMPLE_RATE 44100
#define YIN_THRESHOLD 0.2f

Yin yin;
static const char* TAG = "ESP32";
static i2s_chan_handle_t rx_handle;

static void i2s_setup(void) {
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);

  ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_handle));

  i2s_std_config_t std_cfg = {
    .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
    .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
    .gpio_cfg = {
      .mclk = I2S_GPIO_UNUSED,
      .bclk = GPIO_NUM_26,
      .ws = GPIO_NUM_25,
      .dout = I2S_GPIO_UNUSED,
      .din = GPIO_NUM_33,
      .invert_flags = {
        .mclk_inv = false,
        .bclk_inv = false,
        .ws_inv = false,
      },
    },
  };

  ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle, &std_cfg));
  ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));

  ESP_LOGI(TAG, "I2S RX setup complete.");
  return;
}

static void i2s_read_task(void *args) {
  int32_t *r_buf = (int32_t *)malloc(BUFFER_SIZE * sizeof(int32_t));
  int16_t *s_buf = (int16_t *)malloc(BUFFER_SIZE * sizeof(int16_t));

  if (r_buf == NULL || s_buf == NULL) {
    ESP_LOGI(TAG, "Memory allocation fail!");
    exit(1);
  }

  size_t r_bytes = 0;

  while(1) {
    if (i2s_channel_read(rx_handle, r_buf, BUFFER_SIZE * sizeof(int32_t), &r_bytes, 1000) == ESP_OK) {
      //ESP_LOGI(TAG, "Read task %zu bytes", r_bytes)
      for (int i = 0; i < BUFFER_SIZE; i++) {
        //printf("%lx ", r_buf[i]);
        s_buf[i] = (r_buf[i] >> 16);
        //printf("%x %d\n", s_buf[i], s_buf[i]);
      } 
    } else {
      ESP_LOGI(TAG, "Read task fail!");
    }

    float pitch = Yin_getPitch(&yin, s_buf);

    ESP_LOGI(TAG, "Pitch: %.2f Hz", pitch);

    vTaskDelay(pdMS_TO_TICKS(200));
  }

  free(r_buf);
  free(s_buf);

  vTaskDelete(NULL);
}

void app_main(void) {
  i2s_setup();
  Yin_init(&yin, BUFFER_SIZE, YIN_THRESHOLD);

  xTaskCreate(i2s_read_task, "I2S_Read", 50096, NULL, 5, NULL);
}
