#include <Arduino.h>
#include "BluetoothA2DPSink.h"

BluetoothA2DPSink a2dp_sink;

const int BUTTON = 13;
const int BUTTON_PRESSED = 40;

// void connection_state_changed(esp_a2d_connection_state_t state, void *ptr){
//   Serial.println(a2dp_sink.to_str(state));
// }

// void audio_state_changed(esp_a2d_audio_state_t state, void *ptr){
//   Serial.println(a2dp_sink.to_str(state));
// }

void avrc_metadata_callback(uint8_t id, const uint8_t *text) {
  Serial.printf("==> AVRC metadata rsp: attribute id 0x%x, %s\n", id, text);
}

void setup() {
  Serial.begin(115200);
  #ifdef USE_INTERNAL_DAC
    static const i2s_config_t i2s_config = {
      .mode = (i2s_mode_t) (I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN),
      .sample_rate = 44100, // corrected by info from bluetooth
      .bits_per_sample = (i2s_bits_per_sample_t) 16, /* the DAC module will only take the 8bits from MSB */
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
      .communication_format = (i2s_comm_format_t)I2S_COMM_FORMAT_STAND_MSB,
      .intr_alloc_flags = 0, // default interrupt priority
      .dma_buf_count = 8,
      .dma_buf_len = 64,
      .use_apll = false
    };
  #endif
  #ifdef USE_EXTERNAL_DAC_UDA1334A
    static const i2s_config_t i2s_config = {
      .mode = (i2s_mode_t) (I2S_MODE_MASTER | I2S_MODE_TX),
      .sample_rate = 44100,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
      .communication_format = (i2s_comm_format_t) I2S_COMM_FORMAT_STAND_MSB ,
      .intr_alloc_flags = 0, // default interrupt priority
      .dma_buf_count = 8,
      .dma_buf_len = 64,
      .use_apll = false,
      .tx_desc_auto_clear = true // avoiding noise in case of data unavailability
    };
  #endif
  a2dp_sink.set_i2s_config(i2s_config);
  //a2dp_sink.set_auto_reconnect(true); //Auto-reconnect

  //a2dp_sink.set_on_connection_state_changed(connection_state_changed);
  //a2dp_sink.set_on_audio_state_changed(audio_state_changed);
  a2dp_sink.set_avrc_metadata_callback(avrc_metadata_callback);
  a2dp_sink.set_avrc_metadata_attribute_mask(ESP_AVRC_MD_ATTR_TITLE|ESP_AVRC_MD_ATTR_ARTIST|ESP_AVRC_MD_ATTR_ALBUM|ESP_AVRC_MD_ATTR_PLAYING_TIME);
  a2dp_sink.start("MMMH");
  Serial.println("Started");

}

void loop() {
  delay(1000);
  // Serial.println(touchRead(BUTTON));
  // if(touchRead(BUTTON)<BUTTON_PRESSED) {
  //   if(a2dp_sink.is_connected()) {
  //     a2dp_sink.pause();
  //     delay(1500);
  //     a2dp_sink.disconnect();
  //   }
  //   else {
  //     a2dp_sink.reconnect();
  //     delay(5000);
  //     a2dp_sink.play();
  //   }
  // }


}
