//http://musicweb.ucsd.edu/~trsmyth/analysis/analysis.pdf

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/i2s_std.h"
#include "driver/i2s_common.h"
#include "esp_system.h"
#include "esp_log.h"
#include "dsps_fft2r.h"
#include "dsps_wind.h"
#include "esp_spiffs.h"

#include "oled.h"
#include "server.h"

#define SAMPLE_RATE     16000 
#define SAMPLE_BITS     I2S_DATA_BIT_WIDTH_32BIT
#define SAMPLE_CHANNELS 1
#define FFT_SIZE        4096

#define I2S_BCK_IO      26
#define I2S_WS_IO       25
#define I2S_DI_IO       33

// TODO make buffers on heap to reduce stack size
#define I2S_READER_TASK_PRIO   5
#define I2S_READER_STACK_SIZE  5000
#define PROCESSOR_TASK_PRIO    4
#define PROCESSOR_STACK_SIZE   50000

QueueHandle_t audio_data_queue;

static i2s_chan_handle_t rx_handle;

// Light frequency smoothing
static float last_freq = 0.0f;
static int no_detection_count = 0;  // Track consecutive non-detections

typedef struct {
    const char* name;
    float frequency;
} note_t;

note_t chromatic_scale[] = {
    {"E2", 82.41}, {"F2", 87.31}, {"F#2", 92.50}, {"G2", 98.00}, {"G#2", 103.83},
    {"A2", 110.00}, {"A#2", 116.54}, {"B2", 123.47}, {"C3", 130.81}, {"C#3", 138.59},
    {"D3", 146.83}, {"D#3", 155.56}, {"E3", 164.81}, {"F3", 174.61}, {"F#3", 185.00},
    {"G3", 196.00}, {"G#3", 207.65}, {"A3", 220.00}, {"A#3", 233.08}, {"B3", 246.94},
    {"C4", 261.63}, {"C#4", 277.18}, {"D4", 293.66}, {"D#4", 311.13}, {"E4", 329.63},
    {"F4", 349.23}, {"F#4", 369.99}, {"G4", 392.00}, {"G#4", 415.30}, {"A4", 440.00},
    {"A#4", 466.16}, {"B4", 493.88}
};

#define NUM_NOTES (sizeof(chromatic_scale) / sizeof(chromatic_scale[0]))

typedef struct {
    int32_t *samples;
    size_t sample_count;
} audio_buffer_t;

static void i2s_reader_task(void* pvParameters) {
    int32_t* sample_buffer = (int32_t*)malloc(FFT_SIZE * sizeof(int32_t));
    if (sample_buffer == NULL) {
        ESP_LOGE("I2S", "Failed to allocate sample buffer");
        vTaskDelete(NULL);
    }
    
    audio_buffer_t audio_buf;
    audio_buf.samples = sample_buffer;
    
    while (1) {
        size_t bytes_read = 0;
        i2s_channel_read(rx_handle, audio_buf.samples,
                         FFT_SIZE * sizeof(int32_t), &bytes_read, portMAX_DELAY);
        audio_buf.sample_count = bytes_read / sizeof(int32_t);
        xQueueSend(audio_data_queue, &audio_buf, portMAX_DELAY);
    }
    
    free(sample_buffer);
}

static float quadratic_interpolation(float* magnitudes, int peak_bin) {
    if (peak_bin <= 0 || peak_bin >= (FFT_SIZE/2 - 1))
        return (float)peak_bin * SAMPLE_RATE / FFT_SIZE;

    float gamma = magnitudes[peak_bin - 1]; // y(1)
    float beta  = magnitudes[peak_bin];     // y(0)
    float alpha = magnitudes[peak_bin + 1]; // y(-1)

    float denom = (gamma - 2.0f * beta + alpha);
    if (fabsf(denom) < 1e-12f)
        return (float)peak_bin * SAMPLE_RATE / FFT_SIZE;

    // derived peak shift:
    // p = (alpha - gamma) / (2 * (gamma - 2β + alpha))
    float p = (alpha - gamma) / (2.0f * denom);

    float peak = (float)peak_bin + p;
    return peak * SAMPLE_RATE / FFT_SIZE;
}

static const char* find_closest_note(float frequency, float* cents_offset) {
    const char* note = "Unknown";
    float min_diff = 1e9f;
    for (int i = 0; i < NUM_NOTES; i++) {
        float diff = fabsf(frequency - chromatic_scale[i].frequency);
        if (diff < min_diff) {
            min_diff = diff;
            note = chromatic_scale[i].name;
            *cents_offset = 1200.0f * log2f(frequency / chromatic_scale[i].frequency);
        }
    }
    return note;
}

static float harmonic_product_spectrum(float* magnitudes, int half_size, float* hps) {
    float freq_res = (float)SAMPLE_RATE / FFT_SIZE;

    const int R = 4; // number of harmonics (r = 1..R)

    // initialize with 1 so we can form a product
    for (int i = 0; i < half_size; i++) {
        hps[i] = 1.0f;
    }

    // Y(ω) = ∏ |X(ω r)|
    for (int r = 1; r <= R; r++) {
        int limit = half_size / r;
        for (int i = 0; i < limit; i++) {
            hps[i] *= (magnitudes[i * r] + 1e-12f);
        }
        // bins that cannot support this harmonic are invalid
        for (int i = limit; i < half_size; i++) {
            hps[i] = 0.0f;
        }
    }

    // search band
    int min_bin = (int)floorf(70.0f / freq_res);
    int max_bin = (int)ceilf(600.0f / freq_res);
    if (min_bin < 1) min_bin = 1;
    if (max_bin >= half_size) max_bin = half_size - 1;

    // find maximum Y(ω_i)
    int best_bin = min_bin;
    float best_val = hps[min_bin];

    for (int i = min_bin + 1; i <= max_bin; i++) {
        if (hps[i] > best_val) {
            best_val = hps[i];
            best_bin = i;
        }
    }

    // --- OCTAVE CORRECTION RULE ---
    // check if second peak one octave down exists and is strong enough
    int lower_bin = best_bin / 2;
    if (lower_bin >= min_bin) {
        float chosen_amp = hps[best_bin];
        float lower_amp = hps[lower_bin];

        // lower_peak ≈ 1/2 chosen AND amplitude ratio > threshold
        float approx_half = fabsf(lower_amp - chosen_amp * 0.5f);
        float ratio = lower_amp / (chosen_amp + 1e-12f);
        // TODO NARIHTAJ THRESHOLD 
        // suitable for 5 harmonics - change accordingly
        const float RATIO_THRESHOLD = 0.05f; 

        if (approx_half < chosen_amp * 0.25f && ratio > RATIO_THRESHOLD) {
            best_bin = lower_bin;
        }
    }

    float precise_freq = quadratic_interpolation(hps, best_bin);
    return precise_freq;
}

static void guitar_frequency_analysis(float* magnitudes, float* hps) {
    // compress magnitudes to reduce dominance of harmonics (dynamic range compression)
    for (int i = 1; i < FFT_SIZE/2; i++) {
        magnitudes[i] = powf(magnitudes[i] + 1e-20f, 0.65f);
    }

    float freq = harmonic_product_spectrum(magnitudes, FFT_SIZE/2, hps);
    
    if (freq > 0.0f) {
        // only accept if within 50% of last frequency or if it's the first reading
        if (last_freq > 0.0f) {
            float ratio = freq / last_freq;
            // reject if frequency ratio is too extreme (octave jump or half)
            // but allow detection to reset after 10 consecutive rejections (silence detected)
            if (ratio < 0.5f || ratio > 2.0f) {
                no_detection_count++;
                // After 10 frames of rejection, allow a frequency reset
                if (no_detection_count < 10) {
                    // ignore this detection - likely a harmonic jump
                    return;
                } else {
                    // Too many rejections - reset and allow new frequency
                    last_freq = 0.0f;
                    no_detection_count = 0;
                }
            } else {
                no_detection_count = 0;  // Reset counter on successful detection
            }
        }
        
        last_freq = freq;
        float cents;
        const char* note = find_closest_note(freq, &cents);
        web_server_update_note(note, freq, cents);
        ESP_LOGI("tag", "NOTE: %s  %.2f Hz cent: %.2f", note, freq, cents);
    } else {
        // No detection, increment counter
        no_detection_count++;
        // Reset tracking after 20 frames of no detection (silence)
        if (no_detection_count > 20) {
            last_freq = 0.0f;
            no_detection_count = 0;
        }
    }
}

static void audio_processor_task(void* pvParameters) {
    // allocate buffers on heap
    float* audio_buffer = (float*)malloc(FFT_SIZE * sizeof(float));
    if (audio_buffer == NULL) {
        ESP_LOGE("Processor", "Failed to allocate audio_buffer");
        vTaskDelete(NULL);
    }

    float* hps = (float*)malloc(FFT_SIZE / 2 * sizeof(float));
    if (hps == NULL) {
        ESP_LOGE("Processor", "Failed to allocate hps");
        vTaskDelete(NULL);
    }

    float* magnitude = (float*)malloc((FFT_SIZE / 2) * sizeof(float));
    if (magnitude == NULL) {
        ESP_LOGE("Processor", "Failed to allocate magnitude");
        free(audio_buffer);
        vTaskDelete(NULL);
    }
    
    // NAVODNO STAVIT STATIC ISPREAD FLOAT da ne bude u tasku nego globalno
    //float audio_buffer[FFT_SIZE];
    float hann_window[FFT_SIZE];
    float fft_buffer[FFT_SIZE * 2];
    //float magnitude[FFT_SIZE / 2];
    audio_buffer_t audio_buf;

    dsps_fft2r_init_fc32(NULL, FFT_SIZE);
    dsps_wind_hann_f32(hann_window, FFT_SIZE);

    while (1) {
        xQueueReceive(audio_data_queue, &audio_buf, portMAX_DELAY);

        // convert + window
        for (int i = 0; i < FFT_SIZE; i++) {
            float sample = 0.0f;
            if (i < (int)audio_buf.sample_count) {
                sample = ((float)audio_buf.samples[i] / (float)INT32_MAX);
            }

            audio_buffer[i] = sample * hann_window[i];

            fft_buffer[2 * i]     = audio_buffer[i];
            fft_buffer[2 * i + 1] = 0.0f;
        }

        // FFT processing
        dsps_fft2r_fc32(fft_buffer, FFT_SIZE);
        dsps_bit_rev_fc32(fft_buffer, FFT_SIZE);
        dsps_cplx2reC_fc32(fft_buffer, FFT_SIZE);

        // compute magnitudes
        for (int i = 0; i < FFT_SIZE / 2; i++) {
            float real = fft_buffer[2 * i];
            float imag = fft_buffer[2 * i + 1];
            magnitude[i] = sqrtf(real * real + imag * imag);
        }

        guitar_frequency_analysis(magnitude, hps);
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    free(hps);
    free(magnitude);
    free(audio_buffer);
}

static void setup_i2s() {
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    i2s_new_channel(&chan_cfg, NULL, &rx_handle);

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(SAMPLE_BITS, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_BCK_IO,
            .ws = I2S_WS_IO,
            .dout = I2S_GPIO_UNUSED,
            .din = I2S_DI_IO,
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

void app_main() {
    spiffs_init();
    wifi_ap_start();
    setup_i2s();

    audio_data_queue = xQueueCreate(2, sizeof(audio_buffer_t));
    web_server_start();

    xTaskCreate(i2s_reader_task, "I2S_Reader",
                I2S_READER_STACK_SIZE, NULL, I2S_READER_TASK_PRIO, NULL);

    xTaskCreate(audio_processor_task, "Audio_Processor",
                PROCESSOR_STACK_SIZE, NULL, PROCESSOR_TASK_PRIO, NULL);
}
