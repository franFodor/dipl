#include "oled.h"

static const char *TAG = "example";
lvgl_port_display_cfg_t disp_cfg;

struct guitar_string_s strings[NUM_STRINGS] = {
  {6, 82.41, "E2"},
  {5, 110.00, "A2"},
  {4, 146.83, "D3"},
  {3, 196.00, "G3"},
  {2, 246.94, "B3"},
  {1, 329.63, "E4"}
};

void oled_task(void *pvParameters) {
    ESP_LOGI("TAG", "OLED test task starting");
    
    // Wait a bit for display to initialize
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Create a label - MUST use LVGL_PORT_LOCK/UNLOCK
    lvgl_port_lock(0);
    lv_disp_t *disp = lv_disp_get_default();
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
    lvgl_port_unlock();
    
    // Counter for updating the label
    int counter = 0;

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
}

void setup_oled() {
    ESP_LOGI(TAG, "Initialize I2C bus");
    i2c_master_bus_handle_t i2c_bus = NULL;
    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .i2c_port = I2C_HOST,
        .sda_io_num = EXAMPLE_PIN_NUM_SDA,
        .scl_io_num = EXAMPLE_PIN_NUM_SCL,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &i2c_bus));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t io_config = {
        .dev_addr = EXAMPLE_I2C_HW_ADDR,
        .scl_speed_hz = EXAMPLE_LCD_PIXEL_CLOCK_HZ,
        .control_phase_bytes = 1,               // According to SSD1306 datasheet
        .lcd_cmd_bits = EXAMPLE_LCD_CMD_BITS,   // According to SSD1306 datasheet
        .lcd_param_bits = EXAMPLE_LCD_PARAM_BITS, // According to SSD1306 datasheet
        .dc_bit_offset = 6,                     // According to SSD1306 datasheet
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus, &io_config, &io_handle));

    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_dev_config_t panel_config = {
        .bits_per_pixel = 1,
        .reset_gpio_num = EXAMPLE_PIN_NUM_RST,
    };
   esp_lcd_panel_ssd1306_config_t ssd1306_config = {
        .height = EXAMPLE_LCD_V_RES,
    };
    panel_config.vendor_config = &ssd1306_config;
    ESP_LOGI(TAG, "Install SSD1306 panel driver");
    ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(io_handle, &panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    ESP_LOGI(TAG, "Initialize LVGL");
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_port_init(&lvgl_cfg);

    disp_cfg = (lvgl_port_display_cfg_t){
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES,
        .double_buffer = true,
        .hres = EXAMPLE_LCD_H_RES,
        .vres = EXAMPLE_LCD_V_RES,
        .monochrome = true,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        },
        .flags = {
            .swap_bytes = false,
            .sw_rotate = false,
        }
    };
    
    lvgl_port_add_disp(&disp_cfg);
    ESP_LOGI(TAG, "Done OLED setup");
}

