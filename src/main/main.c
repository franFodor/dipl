//http://musicweb.ucsd.edu/~trsmyth/analysis/analysis.pdf

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/i2s_common.h"
#include "esp_system.h"
#include "esp_log.h"
#include "dsps_fft2r.h"
#include "dsps_wind.h"
#include "esp_spiffs.h"

#include "server.h"
#include "note.h"
#include "chord.h"

#define SAMPLE_RATE     16000
#define SAMPLE_BITS     I2S_DATA_BIT_WIDTH_32BIT
#define SAMPLE_CHANNELS 1
#define FFT_SIZE        4096

#define I2S_BCK_IO      26
#define I2S_WS_IO       25
#define I2S_DI_IO       33

#define SILENCE_THRESHOLD  0.002f

#define PROCESSOR_TASK_PRIO    4
#define PROCESSOR_STACK_SIZE   70000

static i2s_chan_handle_t rx_handle;

static void audio_processor_task(void* pvParameters) {
    int32_t* sample_buffer = (int32_t*)malloc(FFT_SIZE * sizeof(int32_t));
    float*   audio_buffer  = (float*)malloc(FFT_SIZE * sizeof(float));
    float*   hps           = (float*)malloc(FFT_SIZE / 2 * sizeof(float));
    float*   magnitude     = (float*)malloc(FFT_SIZE / 2 * sizeof(float));

    if (!sample_buffer || !audio_buffer || !hps || !magnitude) {
        ESP_LOGE("Processor", "Buffer allocation failed");
        vTaskDelete(NULL);
    }

    float hann_window[FFT_SIZE];
    float fft_buffer[FFT_SIZE * 2];
    chord_result_t chord_result;

    dsps_fft2r_init_fc32(NULL, FFT_SIZE);
    dsps_wind_hann_f32(hann_window, FFT_SIZE);

    note_init();
    chord_init();

    while (1) {
        // Sleep until a web request arrives
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // Read one fresh frame directly from I2S
        size_t bytes_read = 0;
        i2s_channel_read(rx_handle, sample_buffer,
                         FFT_SIZE * sizeof(int32_t), &bytes_read, pdMS_TO_TICKS(1000));
        int n = (int)(bytes_read / sizeof(int32_t));

        // RMS silence check — skip FFT if quiet
        float rms = 0.0f;
        for (int i = 0; i < n; i++) {
            float s = (float)sample_buffer[i] / (float)INT32_MAX;
            rms += s * s;
        }
        rms = sqrtf(rms / n);

        if (rms < SILENCE_THRESHOLD) {
            if (web_server_get_mode() == DETECTION_MODE_NOTE)
                web_server_update_note("None", 0.0f, 0.0f);
            else
                web_server_update_chord("None", NULL, 0);
            web_server_signal_result_ready();
            continue;
        }

        // Apply Hann window and build complex FFT input
        for (int i = 0; i < FFT_SIZE; i++) {
            float sample = (i < n)
                ? ((float)sample_buffer[i] / (float)INT32_MAX)
                : 0.0f;
            audio_buffer[i]       = sample * hann_window[i];
            fft_buffer[2 * i]     = audio_buffer[i];
            fft_buffer[2 * i + 1] = 0.0f;
        }

        dsps_fft2r_fc32(fft_buffer, FFT_SIZE);
        dsps_bit_rev_fc32(fft_buffer, FFT_SIZE);
        dsps_cplx2reC_fc32(fft_buffer, FFT_SIZE);

        for (int i = 0; i < FFT_SIZE / 2; i++) {
            float real = fft_buffer[2 * i];
            float imag = fft_buffer[2 * i + 1];
            magnitude[i] = sqrtf(real * real + imag * imag);
        }

        if (web_server_get_mode() == DETECTION_MODE_NOTE) {
            note_frequency_analysis(magnitude, hps);
        } else {
            chord_detect(magnitude, audio_buffer, &chord_result);
            if (chord_result.valid) {
                web_server_update_chord(chord_result.name,
                    (const char (*)[8])chord_result.notes,
                    chord_result.note_count);
                ESP_LOGI("chord", "Detected: %s", chord_result.name);
            } else {
                web_server_update_chord("None", NULL, 0);
            }
        }

        web_server_signal_result_ready();
    }

    free(sample_buffer);
    free(audio_buffer);
    free(hps);
    free(magnitude);
}

static void setup_i2s(void) {
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    i2s_new_channel(&chan_cfg, NULL, &rx_handle);

    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(SAMPLE_BITS, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_BCK_IO,
            .ws   = I2S_WS_IO,
            .dout = I2S_GPIO_UNUSED,
            .din  = I2S_DI_IO,
        }
    };

    i2s_channel_init_std_mode(rx_handle, &std_cfg);
    i2s_channel_enable(rx_handle);
}

static void spiffs_init(void) {
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 20,
        .format_if_mount_failed = true
    };
    esp_vfs_spiffs_register(&conf);
}

void app_main(void) {
    spiffs_init();
    wifi_ap_start();
    setup_i2s();
    web_server_start();

    TaskHandle_t proc_handle;
    xTaskCreate(audio_processor_task, "Audio_Processor",
                PROCESSOR_STACK_SIZE, NULL, PROCESSOR_TASK_PRIO, &proc_handle);
    web_server_set_processor_task(proc_handle);
}
