#include <Arduino.h>
#include "Arduino_Helpers.h"
#include "AH/Timing/MillisMicrosTimer.hpp"
#include "esPod.h"
#ifdef ENABLE_A2DP
#include "AudioTools.h"
#include "BluetoothA2DPSink.h"
  #ifdef USE_EXTERNAL_DAC_UDA1334A
    I2SStream i2s;
    BluetoothA2DPSink a2dp_sink(i2s);
  #endif

  #ifdef USE_INTERNAL_DAC
    AnalogAudioStream out;
    BluetoothA2DPSink a2dp_sink(out);
  #endif
#endif

#ifdef DEBUG_MODE
esPod espod(USBSerial);
#else
esPod espod(Serial);
#endif

Timer<millis> espodRefreshTimer = 5;
Timer<millis> notificationsRefresh = 500;

#ifdef ENABLE_A2DP
//Placeholder for doing interesting things with regards to the connection state... e.g. if there is a client connected
void connectionStateChanged(esp_a2d_connection_state_t state, void* ptr) {
  //do something when the connection state is changed
  switch (state)
  {
  case ESP_A2D_CONNECTION_STATE_CONNECTED:
    #ifdef LED_BUILTIN
    digitalWrite(LED_BUILTIN,HIGH);
    #endif
    break;
  case ESP_A2D_CONNECTION_STATE_DISCONNECTED:
    #ifdef LED_BUILTIN
    digitalWrite(LED_BUILTIN,LOW);
    #endif
    break;
  default:
    break;
  }

}

//Force play Status sync
void forcePlayStatusSync() {
  switch (a2dp_sink.get_audio_state())
  {
  case ESP_A2D_AUDIO_STATE_STARTED:
    espod._playStatus = PB_STATE_PLAYING;
    break;
  case ESP_A2D_AUDIO_STATE_REMOTE_SUSPEND:
    espod._playStatus = PB_STATE_PAUSED;
    break;
  case ESP_A2D_AUDIO_STATE_STOPPED:
    espod._playStatus = PB_STATE_STOPPED;
    break;
  default:
    break;
  }
}

//Callback to align the iPod playback status to the A2DP stream status
//Could be using an overloaded version of forcePlayStatusSync in the ptr ?
void audioStateChanged(esp_a2d_audio_state_t state,void* ptr) {
  switch (state)
  {
  case ESP_A2D_AUDIO_STATE_STARTED:
    espod._playStatus = PB_STATE_PLAYING;
    break;
  case ESP_A2D_AUDIO_STATE_REMOTE_SUSPEND:
    espod._playStatus = PB_STATE_PAUSED;
    break;
  case ESP_A2D_AUDIO_STATE_STOPPED:
    espod._playStatus = PB_STATE_STOPPED;
    break;
  default:
    break;
  }
}
#endif

void playStatusHandler(byte playCommand) {
  #ifdef ENABLE_A2DP
  switch (playCommand)
  {
  case PB_CMD_TOGGLE: //Toggle Play Pause, the state update is already handled in the espod class
    if(espod._playStatus == PB_STATE_PLAYING) { //Playing
      //Send the play instruction to A2DP
      a2dp_sink.play();
      esp_avrc_ct_send_metadata_cmd(2,ESP_AVRC_MD_ATTR_TITLE);
    }
    else if(espod._playStatus == PB_STATE_PAUSED) { //Paused
      //Send the pause instruction to A2DP
      a2dp_sink.pause();
      esp_avrc_ct_send_metadata_cmd(2,ESP_AVRC_MD_ATTR_TITLE);
    }
    else { //Stopped
      //Send the stop instruction to A2DP
      a2dp_sink.stop();
      esp_avrc_ct_send_metadata_cmd(2,ESP_AVRC_MD_ATTR_TITLE);
    }
    break;
  case PB_CMD_STOP: //Stop
    //Send the Stop instruction to A2DP
    a2dp_sink.stop();
    break;
  case PB_CMD_NEXT_TRACK: //Next
    //Send the next instruction to A2DP
    a2dp_sink.next();
    esp_avrc_ct_send_metadata_cmd(2,ESP_AVRC_MD_ATTR_TITLE); //Trying to force a title update here
    break;
  case PB_CMD_PREVIOUS_TRACK: //Prev
    //Send the prev instruction to A2DP
    a2dp_sink.previous();
    esp_avrc_ct_send_metadata_cmd(2,ESP_AVRC_MD_ATTR_TITLE);
    break;
  case PB_CMD_NEXT: //next
    //Send the next instruction to A2DP
    a2dp_sink.next();
    esp_avrc_ct_send_metadata_cmd(2,ESP_AVRC_MD_ATTR_TITLE);
    break;
  case PB_CMD_PREV: //Prev
    //Send the prev instruction to A2DP
    a2dp_sink.previous();
    esp_avrc_ct_send_metadata_cmd(2,ESP_AVRC_MD_ATTR_TITLE);
    break;
  case PB_CMD_PLAY: //Play
    a2dp_sink.play();
    esp_avrc_ct_send_metadata_cmd(2,ESP_AVRC_MD_ATTR_TITLE);
    break;
  case PB_CMD_PAUSE: //Pause
    a2dp_sink.pause();
    esp_avrc_ct_send_metadata_cmd(2,ESP_AVRC_MD_ATTR_TITLE);
    break;
  default:
    break;
  }
  #endif
}

#ifdef ENABLE_A2DP
void avrc_rn_play_pos_callback(uint32_t play_pos) {
  //Serial.printf("Play position is %d (%d seconds)\n", play_pos, (int)round(play_pos/1000.0));
}

void avrc_metadata_callback(uint8_t id, const uint8_t *text) {
  //Serial.printf("==> AVRC metadata rsp: attribute id 0x%x, %s\n", id, text);
  switch (id)
  {
  case ESP_AVRC_MD_ATTR_ALBUM:
    strcpy(espod._albumName,(char*)text);
    break;
  case ESP_AVRC_MD_ATTR_ARTIST:
    strcpy(espod._artistName,(char*)text);
    break;
  case ESP_AVRC_MD_ATTR_TITLE:
    strcpy(espod._trackTitle,(char*)text);
    break;
  case ESP_AVRC_MD_ATTR_PLAYING_TIME:
    espod._trackDuration = String((char*)text).toInt();
    break;
  default:
    break;
  }
  espod.notifyTrackChange = true;
}
#endif

void setup() {
  #ifdef ENABLE_A2DP
    #ifdef USE_EXTERNAL_DAC_UDA1334A
    auto cfg = i2s.defaultConfig(TX_MODE);
    cfg.pin_ws = 25;
    cfg.pin_bck = 26;
    cfg.pin_data = 22;
    cfg.sample_rate = 44100;
    cfg.i2s_format = I2S_LSB_FORMAT;
    
    i2s.begin(cfg);

    /*
    Default pins are as follows : 
    WSEL  ->  GPIO 25
    DIN   ->  GPIO 22
    BCLK  ->  GPIO 26
    */
    #endif
    //a2dp_sink.set_i2s_config(i2s_config);
    //a2dp_sink.set_auto_reconnect(true); //Auto-reconnect
    a2dp_sink.set_on_connection_state_changed(connectionStateChanged);
    a2dp_sink.set_on_audio_state_changed(audioStateChanged);
    a2dp_sink.set_avrc_metadata_callback(avrc_metadata_callback);
    a2dp_sink.set_avrc_metadata_attribute_mask(ESP_AVRC_MD_ATTR_TITLE|ESP_AVRC_MD_ATTR_ARTIST|ESP_AVRC_MD_ATTR_ALBUM|ESP_AVRC_MD_ATTR_PLAYING_TIME);
    a2dp_sink.set_avrc_rn_play_pos_callback(avrc_rn_play_pos_callback);
    a2dp_sink.start("espiPod");

    #ifdef LED_BUILTIN
    pinMode(LED_BUILTIN,OUTPUT);
    digitalWrite(LED_BUILTIN,LOW);
    #endif
  
  #endif

  #ifdef DEBUG_MODE
    USBSerial.setRxBufferSize(4096);
    USBSerial.begin(19200);
    Serial.setTxBufferSize(4096);
    Serial.begin(115200);
    while(USBSerial.available()) USBSerial.read();
  #else
    Serial.setRxBufferSize(4096);
    Serial.setTxBufferSize(4096);
    Serial.begin(19200);

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
    forcePlayStatusSync();
  }
}
