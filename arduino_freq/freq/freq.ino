//Libraries 
#include <driver/i2s.h>
#include <arduinoFFT.h>
#include <math.h>

#define I2S_WS   25
#define I2S_DIN  22
#define I2S_BCK  26
#define I2S_PORT I2S_NUM_0

#define bufferLen 512

arduinoFFT FFT = arduinoFFT();

double vReal[bufferLen];
double vImag[bufferLen];

void setup() {
  Serial.begin(115200);
  initI2S();

  xTaskCreate(ReadingTask,"audio",4096,NULL,1,NULL);
}

void loop() 
{
  
}

void initI2S() 
{
  const i2s_config_t i2s_config = {
    .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 10000,
    .bits_per_sample = i2s_bits_per_sample_t(32),
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S | I2S_COMM_FORMAT_I2S_MSB),
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = 256,
    .use_apll = false
  };

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  
  const i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_BCK,
    .ws_io_num = I2S_WS,
    .data_out_num = -1,
    .data_in_num = I2S_DIN
  };

  i2s_set_pin(I2S_PORT, &pin_config);
  i2s_start(I2S_PORT);
}

void ReadingTask(void* parameter)
{
  int32_t local[bufferLen];
  size_t bytesIn = 0;

  for (;;)
  {
    i2s_read(I2S_PORT, &local, sizeof(local), &bytesIn, portMAX_DELAY);

    for (size_t i = 0; i < bufferLen; i++) {
        int32_t sample = local[i];
        //Serial.println(sample, HEX);
        vReal[i] = sample;
        vImag[i] = 0;
      }
    
      // Perform FFT 
      FFT.DCRemoval();
      FFT.Windowing(vReal, bufferLen, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
      FFT.Compute(vReal,vImag, bufferLen,FFT_FORWARD);
      FFT.ComplexToMagnitude(vReal, vImag, bufferLen);

      double peak = FFT.MajorPeak(vReal, bufferLen,10000);

      Serial.println(peak);
  }
}