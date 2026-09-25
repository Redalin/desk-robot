#include "mic.h"
#include <driver/i2s.h>

static const i2s_port_t MIC_I2S_PORT = I2S_NUM_1;

void Mic::beginI2S(uint8_t sckPin, uint8_t wsPin, uint8_t sdPin, float gain) {
  isI2S_ = true;
  gain_ = gain;
  queue_ = xQueueCreate(16, FRAME_BYTES);

  i2s_config_t cfg = {};
  cfg.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX);
  cfg.sample_rate = 16000;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count = 4;
  cfg.dma_buf_len = FRAME_SAMPLES;
  cfg.use_apll = false;

  i2s_driver_install(MIC_I2S_PORT, &cfg, 0, nullptr);

  i2s_pin_config_t pins = {};
  pins.bck_io_num = sckPin;
  pins.ws_io_num = wsPin;
  pins.data_out_num = I2S_PIN_NO_CHANGE;
  pins.data_in_num = sdPin;
  i2s_set_pin(MIC_I2S_PORT, &pins);

  xTaskCreatePinnedToCore(taskEntry, "mic", 4096, this, 2, nullptr, 0);
  Serial.printf("[mic] INMP441/MS3625 I2S ready (SCK: %d, WS: %d, SD: %d, gain: %.1f)\n",
                sckPin, wsPin, sdPin, gain);
}

void Mic::beginAnalog(uint8_t adcPin, float gain) {
  isI2S_ = false;
  pin_ = adcPin;
  gain_ = gain;
  analogReadResolution(12);
  pinMode(pin_, INPUT);

  queue_ = xQueueCreate(16, FRAME_BYTES);  // ~0.5 s of frames
  xTaskCreatePinnedToCore(taskEntry, "mic", 4096, this, 2, nullptr, 0);
  Serial.printf("[mic] Analog ADC ready (PIN: %d, gain: %.1f)\n", pin_, gain);
}

bool Mic::nextFrame(uint8_t* out) {
  return queue_ != nullptr && xQueueReceive(queue_, out, 0) == pdTRUE;
}

void Mic::taskEntry(void* self) { static_cast<Mic*>(self)->task(); }

void Mic::task() {
  static int16_t frame[FRAME_SAMPLES];
  static int32_t i2sBuf[FRAME_SAMPLES];
  float dcBias = 1650.0f;

  for (;;) {
    if (!streaming_) {
      level_ = 0.0f;
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }

    if (isI2S_) {
      size_t bytesRead = 0;
      esp_err_t res = i2s_read(MIC_I2S_PORT, i2sBuf, sizeof(i2sBuf), &bytesRead, portMAX_DELAY);
      if (res != ESP_OK || bytesRead == 0) {
        vTaskDelay(pdMS_TO_TICKS(2));
        continue;
      }
      size_t samples = bytesRead / sizeof(int32_t);
      float sumSq = 0;
      for (size_t i = 0; i < samples; ++i) {
        // INMP441 outputs 24-bit MSB-aligned data in a 32-bit slot.
        // Shift right by 14 for optimal sensitivity and 16-bit PCM range.
        int32_t sample = (i2sBuf[i] >> 14);
        sample = static_cast<int32_t>(sample * gain_);
        sample = constrain(sample, -32768, 32767);
        frame[i] = static_cast<int16_t>(sample);

        float f = frame[i] / 32768.0f;
        sumSq += f * f;
      }
      if (samples > 0) {
        level_ = sqrtf(sumSq / samples);
      }
    } else {
      int64_t nextSampleUs = esp_timer_get_time();
      for (size_t i = 0; i < FRAME_SAMPLES; ++i) {
        nextSampleUs += (i & 1) ? 63 : 62;
        int64_t now = esp_timer_get_time();
        if (nextSampleUs > now) {
          delayMicroseconds(static_cast<uint32_t>(nextSampleUs - now));
        }

        int raw = analogRead(pin_);
        dcBias = 0.995f * dcBias + 0.005f * static_cast<float>(raw);
        float ac = static_cast<float>(raw) - dcBias;

        float s = ac * 16.0f * gain_;
        s = constrain(s, -32768.0f, 32767.0f);
        frame[i] = static_cast<int16_t>(s);
      }

      float sumSq = 0;
      for (size_t i = 0; i < FRAME_SAMPLES; ++i) {
        float f = frame[i] / 32768.0f;
        sumSq += f * f;
      }
      level_ = sqrtf(sumSq / FRAME_SAMPLES);
      vTaskDelay(pdMS_TO_TICKS(1));
    }

    if (streaming_) {
      if (uxQueueSpacesAvailable(queue_) == 0) {
        static uint8_t scratch[FRAME_BYTES];
        xQueueReceive(queue_, scratch, 0);
      }
      xQueueSend(queue_, frame, 0);
    }
  }
}
