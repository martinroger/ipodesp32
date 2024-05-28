#include <Arduino.h>
#include "Arduino_Helpers.h"
#include "AH/Timing/MillisMicrosTimer.hpp"
//#include "esPod.h"
#ifdef ENABLE_A2DP
	#include "AudioTools.h"
	#include "BluetoothA2DPSink.h"
		#ifdef USE_EXTERNAL_DAC_UDA1334A
			I2SStream i2s;
			BluetoothA2DPSink a2dp_sink(i2s);
		#endif

#endif

Timer<millis> loopTimer = 1000;

#ifdef ENABLE_A2DP

void connection_state_changed(esp_a2d_connection_state_t state, void *ptr){
  Serial.println(a2dp_sink.to_str(state));
}

// for esp_a2d_audio_state_t see https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/bluetooth/esp_a2dp.html#_CPPv421esp_a2d_audio_state_t
void audio_state_changed(esp_a2d_audio_state_t state, void *ptr){
  Serial.println(a2dp_sink.to_str(state));
}

void avrc_rn_play_pos_callback(uint32_t play_pos) {
	Serial.printf("Play position is %d (%d seconds)\n", play_pos, (int)round(play_pos/1000.0));
}

void avrc_metadata_callback(uint8_t id, const uint8_t *text) {
  Serial.printf("==> AVRC metadata rsp: attribute id 0x%x, %s\n", id, text);
  if (id == ESP_AVRC_MD_ATTR_PLAYING_TIME) {
    uint32_t playtime = String((char*)text).toInt();
    Serial.printf("==> Playing time is %d ms (%d seconds)\n", playtime, (int)round(playtime/1000.0));
  }
}

void avrc_rn_track_change_callback(uint8_t *id) {
  Serial.println("Track Change");
  for (uint8_t i = 0; i < 8; i++)
  {
    Serial.printf("  Byte %d : 0x%x \n",i,*(id+i));
  }

}
#endif

void setup() {
  Serial.begin(115200);
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
		//a2dp_sink.set_on_connection_state_changed(connection_state_changed);
		//a2dp_sink.set_on_audio_state_changed(audio_state_changed);
		a2dp_sink.set_avrc_metadata_callback(avrc_metadata_callback);
		a2dp_sink.set_avrc_metadata_attribute_mask(ESP_AVRC_MD_ATTR_ALBUM|ESP_AVRC_MD_ATTR_ARTIST|ESP_AVRC_MD_ATTR_GENRE|ESP_AVRC_MD_ATTR_NUM_TRACKS|ESP_AVRC_MD_ATTR_PLAYING_TIME|ESP_AVRC_MD_ATTR_TITLE|ESP_AVRC_MD_ATTR_TRACK_NUM);
		a2dp_sink.set_avrc_rn_play_pos_callback(avrc_rn_play_pos_callback,1);
    a2dp_sink.set_avrc_rn_track_change_callback(avrc_rn_track_change_callback);
		a2dp_sink.start("espiPod");

		#ifdef LED_BUILTIN
			pinMode(LED_BUILTIN,OUTPUT);
			digitalWrite(LED_BUILTIN,LOW);
		#endif
	#endif

  	//while(Serial.available()) Serial.read(); //Flush the RX Buffer


}

void loop() {
// if(loopTimer) {


// }
}
