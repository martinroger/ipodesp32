#include <Arduino.h>
#include "Arduino_Helpers.h"
#include "AH/Timing/MillisMicrosTimer.hpp"
#include "esPod.h"
#ifdef ENABLE_A2DP
#include "BluetoothA2DPSink.h"
BluetoothA2DPSink a2dp_sink;
#endif

#ifdef DEBUG_MODE
esPod espod(USBSerial);
#else
esPod espod(Serial);
#endif

Timer<millis> espodRefreshTimer = 10;
Timer<millis> notificationsRefresh = 500;

void playStatusHandler(byte playCommand) {
  #ifdef ENABLE_A2DP
  switch (playCommand)
  {
  case 0x01: //Toggle Play Pause
    if(espod._playStatus == 0x01) { //Playing
      //Send the play instruction to A2DP
      a2dp_sink.play();
    }
    else if(espod._playStatus == 0x02) { //Paused
      //Send the pause instruction to A2DP
      a2dp_sink.pause();
    }
    else { //Stopped
      //Send the stop instruction to A2DP
      a2dp_sink.stop();
    }
    break;
  case 0x02: //Stop
    //Send the Stop instruction to A2DP
    a2dp_sink.stop();
    break;
  case 0x03: //Next
    //Send the next instruction to A2DP
    a2dp_sink.next();
    break;
  case 0x04: //Prev
    //Send the prev instruction to A2DP
    a2dp_sink.previous();
    break;
  case 0x08: //next
    //Send the next instruction to A2DP
    a2dp_sink.next();
    break;
  case 0x09: //Prev
    //Send the prev instruction to A2DP
    a2dp_sink.previous();
    break;
  case 0x0A: //Play
    a2dp_sink.play();
    break;
  case 0x0B: //Pause
    a2dp_sink.pause();
    break;
  default:
    break;
  }
  #endif
}

#ifdef ENABLE_A2DP
void avrc_metadata_callback(uint8_t id, const uint8_t *text) {
  //Serial.printf("==> AVRC metadata rsp: attribute id 0x%x, %s\n", id, text);
  switch (id)
  {
  case 0x04:
    strcpy(espod._albumName,(char*)text);
    break;
  case 0x02:
    strcpy(espod._artistName,(char*)text);
    break;
  case 0x01:
    strcpy(espod._trackTitle,(char*)text);
    break;
  // case ESP_AVRC_MD_ATTR_GENRE:
  //   strcpy(espod._trackGenre,(char*)&text);
  //   break;
  default:
    break;
  }
}
#endif

void setup() {
  #ifdef ENABLE_A2DP
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
    a2dp_sink.set_avrc_metadata_attribute_mask(ESP_AVRC_MD_ATTR_TITLE|ESP_AVRC_MD_ATTR_ARTIST|ESP_AVRC_MD_ATTR_ALBUM|ESP_AVRC_MD_ATTR_GENRE);
    a2dp_sink.start("espiPod");
  
  #endif

  #ifdef DEBUG_MODE
    USBSerial.begin(19200);
    USBSerial.setRxBufferSize(4096);
    Serial.setTxBufferSize(4096);
    Serial.begin(115200);
    while(USBSerial.available()) USBSerial.read();
  #else
    Serial.begin(19200);
    Serial.setRxBufferSize(4096);
    Serial.setTxBufferSize(4096);
  #endif
  while(Serial.available()) Serial.read();
  espod.attachPlayControlHandler(playStatusHandler);
}

void loop() {
  if(espodRefreshTimer) {
    espod.refresh();
  }
  if(notificationsRefresh) {
    espod.cyclicNotify();
  }
}
