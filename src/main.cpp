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
	#ifdef LED_BUILTIN
		switch (state)	{
			case ESP_A2D_CONNECTION_STATE_CONNECTED:
				digitalWrite(LED_BUILTIN,HIGH);
				break;
			case ESP_A2D_CONNECTION_STATE_DISCONNECTED:
				digitalWrite(LED_BUILTIN,LOW);
				break;
			default:
				break;
		}
	#endif
}

//Force play Status sync periodically (might be a bad idea)
void forcePlayStatusSync() {
	switch (a2dp_sink.get_audio_state()) {
		case ESP_A2D_AUDIO_STATE_STARTED:
			espod.playStatus = PB_STATE_PLAYING;
			break;
		case ESP_A2D_AUDIO_STATE_REMOTE_SUSPEND:
			espod.playStatus = PB_STATE_PAUSED;
			break;
		case ESP_A2D_AUDIO_STATE_STOPPED:
			espod.playStatus = PB_STATE_STOPPED;
			break;
		default:
			break;
	}
}

//Callback to align the iPod playback status to the A2DP stream status
//Could be using an overloaded version of forcePlayStatusSync in the ptr ?
void audioStateChanged(esp_a2d_audio_state_t state,void* ptr) {
	switch (state)	{
		case ESP_A2D_AUDIO_STATE_STARTED:
			espod.playStatus = PB_STATE_PLAYING;
			break;
		case ESP_A2D_AUDIO_STATE_REMOTE_SUSPEND:
			espod.playStatus = PB_STATE_PAUSED;
			break;
		case ESP_A2D_AUDIO_STATE_STOPPED:
			espod.playStatus = PB_STATE_STOPPED;
			break;
		default:
			break;
	}
}
#endif

void playStatusHandler(byte playCommand) {
	#ifdef ENABLE_A2DP
	/*
	Force-refresh title :
	esp_avrc_ct_send_metadata_cmd(2,ESP_AVRC_MD_ATTR_TITLE);
	*/
  	switch (playCommand) {
		case A2DP_STOP:
			/* code */
			break;
		case A2DP_PLAY:
			/* code */
			break;
		case A2DP_PAUSE:
			/* code */
			break;
		case A2DP_REWIND:
			/* code */
			break;
		case A2DP_NEXT:
			/* code */
			break;
		case A2DP_PREV:
			/* code */
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
	switch (id)	{
		case ESP_AVRC_MD_ATTR_ALBUM:
			strcpy(espod.albumName,(char*)text);
			break;
		case ESP_AVRC_MD_ATTR_ARTIST:
			strcpy(espod.artistName,(char*)text);
			break;
		case ESP_AVRC_MD_ATTR_TITLE:
			strcpy(espod.trackTitle,(char*)text);
			break;
		case ESP_AVRC_MD_ATTR_PLAYING_TIME:
			espod.trackDuration = String((char*)text).toInt();
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

  	while(Serial.available()) Serial.read(); //Flush the RX Buffer
 	
	//Prep and start up espod
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
