#include <driver/i2s.h>
#include <opus.h>

#include "main.h"

#define OPUS_OUT_BUFFER_SIZE 1276  // 1276 bytes is recommended by opus_encode
#define SAMPLE_RATE 16000
#define BUFFER_SAMPLES (320 * 2) // for 16k, 2chl, 20ms
// #define BUFFER_SAMPLES (320 * 1) // for 16k, 1chl, 20ms

#define I2S_DATA_OUT_PORT I2S_NUM_0
#define I2S_DATA_IN_PORT I2S_DATA_OUT_PORT
#define MCLK_PIN -1
#define DAC_BCLK_PIN 8
#define DAC_LRCLK_PIN 7
#define DAC_DATA_PIN 43
#define ADC_DATA_PIN 44

#define OPUS_ENCODER_BITRATE 30000
#define OPUS_ENCODER_COMPLEXITY 0

int32_t respeaker_input_buffer[BUFFER_SAMPLES];
int32_t respeaker_output_buffer[BUFFER_SAMPLES];

void i2s_read_32_to_16bit(i2s_port_t i2s_num, void *dest, size_t size, size_t *bytes_read, TickType_t ticks_to_wait);

void oai_init_audio_capture() {
  i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_SLAVE | I2S_MODE_TX | I2S_MODE_RX),
      .sample_rate = SAMPLE_RATE,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
      .communication_format = I2S_COMM_FORMAT_I2S_MSB,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 8,
      .dma_buf_len = BUFFER_SAMPLES,
      .use_apll = 1,
      .tx_desc_auto_clear = true,
  };
  if (i2s_driver_install(I2S_DATA_IN_PORT, &i2s_config, 0, NULL) != ESP_OK) {
    printf("Failed to configure I2S driver for audio input/output");
    return;
  }

  i2s_pin_config_t pin_config_out = {
    .mck_io_num   = MCLK_PIN,
    .bck_io_num   = DAC_BCLK_PIN,
    .ws_io_num    = DAC_LRCLK_PIN,
    .data_out_num = DAC_DATA_PIN,
    .data_in_num  = ADC_DATA_PIN,
  };
  if (i2s_set_pin(I2S_DATA_IN_PORT, &pin_config_out) != ESP_OK) {
    printf("Failed to set I2S pins for audio output");
    return;
  }
  i2s_zero_dma_buffer(I2S_DATA_IN_PORT);
}

opus_int16 *output_buffer = NULL;
OpusDecoder *opus_decoder = NULL;

void oai_init_audio_decoder() {
  int decoder_error = 0;
  opus_decoder = opus_decoder_create(SAMPLE_RATE, 2, &decoder_error);
  if (decoder_error != OPUS_OK) {
    printf("Failed to create OPUS decoder");
    return;
  }

  output_buffer = (opus_int16 *)malloc(BUFFER_SAMPLES * sizeof(opus_int16));
}

void oai_audio_decode(uint8_t *data, size_t size) {
  int decoded_size =
      opus_decode(opus_decoder, data, size, output_buffer, BUFFER_SAMPLES, 0);
  for (uint16_t i = 0; i < BUFFER_SAMPLES; i++)
  {
    respeaker_output_buffer[i] = ((int32_t)(*(output_buffer + i))) << (16 - 1); // replace 0 to 1, 2, or -1, -2 to increase/decrease output volume
  }

  if (decoded_size > 0) {
    size_t bytes_written = 0;
    i2s_write(I2S_DATA_OUT_PORT, respeaker_output_buffer, BUFFER_SAMPLES * sizeof(int32_t),
              &bytes_written, portMAX_DELAY);
  }
}

OpusEncoder *opus_encoder = NULL;
opus_int16 *encoder_input_buffer = NULL;
uint8_t *encoder_output_buffer = NULL;

void oai_init_audio_encoder() {
  int encoder_error;
  opus_encoder = opus_encoder_create(SAMPLE_RATE, 2, OPUS_APPLICATION_VOIP,
                                     &encoder_error);
  if (encoder_error != OPUS_OK) {
    printf("Failed to create OPUS encoder");
    return;
  }

  if (opus_encoder_init(opus_encoder, SAMPLE_RATE, 2, OPUS_APPLICATION_VOIP) !=
      OPUS_OK) {
    printf("Failed to initialize OPUS encoder");
    return;
  }

  opus_encoder_ctl(opus_encoder, OPUS_SET_BITRATE(OPUS_ENCODER_BITRATE));
  opus_encoder_ctl(opus_encoder, OPUS_SET_COMPLEXITY(OPUS_ENCODER_COMPLEXITY));
  opus_encoder_ctl(opus_encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
  encoder_input_buffer = (opus_int16 *)malloc(BUFFER_SAMPLES);
  encoder_output_buffer = (uint8_t *)malloc(OPUS_OUT_BUFFER_SIZE);
}

void oai_send_audio(PeerConnection *peer_connection) {
  size_t bytes_read = 0;

  i2s_read_32_to_16bit(I2S_DATA_OUT_PORT, encoder_input_buffer, BUFFER_SAMPLES, &bytes_read,
           portMAX_DELAY);

  auto encoded_size =
      opus_encode(opus_encoder, encoder_input_buffer, BUFFER_SAMPLES / 2,
                  encoder_output_buffer, OPUS_OUT_BUFFER_SIZE);
  // printf("encode: %ld\n", encoded_size);

  peer_connection_send_audio(peer_connection, encoder_output_buffer,
                             encoded_size);
}

void i2s_read_32_to_16bit(i2s_port_t i2s_num, void *dest, size_t size, size_t *bytes_read, TickType_t ticks_to_wait)
{
  opus_int16 *dest_cp = (opus_int16 *)dest;
  i2s_read(i2s_num, respeaker_input_buffer, size * 4, bytes_read, ticks_to_wait);
  
  for (uint16_t i = 0; i < size; i++)
  {
    dest_cp[i] = (opus_int16)(respeaker_input_buffer[i] >> (16 - 0)); // replace 0 to 1, 2, or -1, -2 to increase/decrease input volume
  }
}
