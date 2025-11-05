#include <stdio.h>

#include "esp_log.h"
#include "driver/i2s_common.h"
#include "driver/i2s_std.h"

#include "oled.h"

// https://github.com/ashokfernandez/Yin-Pitch-Tracking
#include "Yin.h"

#define BUFFER_SIZE 2048
// needs to be the same as in Yin.h
#define SAMPLE_RATE 44100
#define YIN_THRESHOLD 0.2f
#define NUM_STRINGS 6

Yin yin;
static const char* TAG = "ESP32";
static i2s_chan_handle_t rx_handle;
volatile float pitch = 0;

struct guitar_string_s {
  int string_number;
  float target_freq;
  const char *note;
};

// https://en.wikipedia.org/wiki/Guitar_tunings
struct guitar_string_s strings[NUM_STRINGS] = {
  {6, 82.41, "E2"},
  {5, 110.00, "A2"},
  {4, 146.83, "D3"},
  {3, 196.00, "G3"},
  {2, 246.94, "B3"},
  {1, 329.63, "E4"}
};

static void setup_i2s(void) {
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

    float local_pitch = Yin_getPitch(&yin, s_buf);

    if (local_pitch > 0) {
      pitch = local_pitch;
    }

    ESP_LOGI(TAG, "Pitch: %.2f Hz", pitch);

    vTaskDelay(pdMS_TO_TICKS(200));
  }

  free(r_buf);
  free(s_buf);

  vTaskDelete(NULL);
}

static void oled_task(void *args) {
  lv_disp_t *disp = lvgl_port_add_disp(&disp_cfg);
  lv_disp_set_rotation(disp, LV_DISP_ROTATION_180);
  lv_obj_t *scr = lv_disp_get_scr_act(disp);

  // pitch
  lv_obj_t *label_pitch = lv_label_create(scr);
  // lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_obj_set_width(label_pitch, lv_display_get_physical_horizontal_resolution(disp));
  lv_obj_align(label_pitch, LV_ALIGN_TOP_MID, 0, 0);

  // strings
  lv_obj_t *label_str1 = lv_label_create(scr);
  // lv_obj_set_width(label_str1, lv_display_get_physical_horizontal_resolution(disp));
  // lv_label_set_long_mode(label_str1, LV_LABEL_LONG_MODE_SCROLL);
  lv_obj_align(label_str1, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);

  lv_obj_t *label_str2 = lv_label_create(scr);
  // lv_obj_set_width(label_str2, lv_display_get_physical_horizontal_resolution(disp));
  // lv_label_set_long_mode(label_str2, LV_LABEL_LONG_MODE_SCROLL);
  lv_obj_align(label_str2, LV_ALIGN_OUT_BOTTOM_MID, 0, 40);

  // default
  lv_label_set_text(label_pitch, "Play a string...");
  lv_label_set_text(label_str1, "");
  lv_label_set_text(label_str2, "");

  char pitch_buf[16];
  char string1_buf[32];
  char string2_buf[32];

  while(1) {
  float local_pitch = 90;
    if (lvgl_port_lock(0) && local_pitch > 10) {
      int handled = 0;
      snprintf(pitch_buf, sizeof(pitch_buf), "READ %.1fHz", local_pitch);
      lv_label_set_text(label_pitch, pitch_buf);

      for (int i = 0; i < NUM_STRINGS - 1; i++) {
        float low = strings[i].target_freq;
        float high = strings[i + 1].target_freq;

        if (local_pitch >= low && local_pitch <= high) {
          snprintf(string1_buf, sizeof(string1_buf),
                   "String %d %.1fHz",
                   strings[i].string_number, strings[i].target_freq);
          snprintf(string2_buf, sizeof(string2_buf),
                   "String %d %.1fHz",
                   strings[i + 1].string_number, strings[i + 1].target_freq);

          lv_label_set_text(label_str1, string1_buf);
          lv_label_set_text(label_str2, string2_buf);

          handled = 1;
        }
      }

    // below lowest string or above highest string
    if (!handled) {
      if (local_pitch < strings[NUM_STRINGS - 1].target_freq) {
        snprintf(string1_buf, sizeof(string1_buf),
                 "String %d %.1fHz",
                 strings[0].string_number, strings[0].target_freq);
      }
      else if (local_pitch > strings[0].target_freq) {
        snprintf(string1_buf, sizeof(string1_buf),
                 "String %d %.1fHz",
                 strings[NUM_STRINGS - 1].string_number, strings[NUM_STRINGS - 1].target_freq);
      }

      lv_label_set_text(label_str1, string1_buf);
      lv_label_set_text(label_str2, "");
    }

    lvgl_port_unlock();
  }

    vTaskDelay(pdMS_TO_TICKS(500));
  }
  
  vTaskDelete(NULL);
}

void app_main(void) {
  setup_i2s();
  Yin_init(&yin, BUFFER_SIZE, YIN_THRESHOLD);

  setup_oled();

  xTaskCreate(i2s_read_task, "I2S_Read", 32768, NULL, 5, NULL);
  xTaskCreate(oled_task, "Oled_Display", 16384, NULL, 5, NULL);
}
