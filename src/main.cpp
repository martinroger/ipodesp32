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

//Reminders used to keep track of previous changes
char incAlbumName[255] = "incAlbum";
char prevAlbumName[255] = "PrevAlbum";
char incArtistName[255] = "incArtist";
char prevArtistName[255] = "PrevArtist";
char incTrackTitle[255] = "incTitle";
char prevTrackTitle[255] = "PrevTitle";
bool albumNameUpdated = false;
bool artistNameUpdated = false;
bool trackTitleUpdated = false;

#ifdef ENABLE_A2DP
/// @brief callback on changes of A2DP connection and AVRCP connection. Turns a LED on, enables the espod.
/// @param state New state passed by the callback.
/// @param ptr Not used.
void connectionStateChanged(esp_a2d_connection_state_t state, void* ptr) {
	switch (state)	{
		case ESP_A2D_CONNECTION_STATE_CONNECTED:
				#ifdef LED_BUILTIN
				digitalWrite(LED_BUILTIN,HIGH);
				#endif
			espod.disabled = false;
			break;
		case ESP_A2D_CONNECTION_STATE_DISCONNECTED:
				#ifdef LED_BUILTIN
				digitalWrite(LED_BUILTIN,LOW);
				#endif
			espod.disabled = true;
			break;
	}
}

/// @brief Callback for the change of playstate after connection. Aligns the state of the esPod to the state of the phone. Play should be called by the espod interaction
/// @param state The A2DP Stream to align to.
/// @param ptr Not used.
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
	}
}

/// @brief Play position callback returning the ms spent since start on every interval
/// @param play_pos Playing Position in ms
void avrc_rn_play_pos_callback(uint32_t play_pos) {
	espod.playPosition = play_pos;
	if(espod.playStatusNotificationState==NOTIF_ON) {
		espod.L0x04_0x27_PlayStatusNotification(0x04,play_pos);
	}
}

/// @brief Catch callback for the AVRC metadata. There can be duplicates !
/// @param id Metadata attribute ID : ESP_AVRC_MD_ATTR_xxx
/// @param text Text data passed around, sometimes it's a uint32_t
void avrc_metadata_callback(uint8_t id, const uint8_t *text) {
	switch (id)	{
		case ESP_AVRC_MD_ATTR_ALBUM:
			strcpy(incAlbumName,(char*)text);
			if((strcmp(incAlbumName,espod.albumName)!=0) || artistNameUpdated || trackTitleUpdated) { //If there is a difference, copy it over and set an updated flag
				strcpy(espod.albumName,incAlbumName);
				albumNameUpdated = true;
			}
			break;
		case ESP_AVRC_MD_ATTR_ARTIST:
			strcpy(incArtistName,(char*)text);
			if((strcmp(incArtistName,espod.artistName)!=0) || albumNameUpdated || trackTitleUpdated) { //If there is a difference, copy it over and set an updated flag
				strcpy(espod.artistName,incArtistName);
				artistNameUpdated = true;
			}
			break;
		case ESP_AVRC_MD_ATTR_TITLE:
			strcpy(incTrackTitle,(char*)text);
			if((strcmp(incTrackTitle,espod.trackTitle)!=0) || artistNameUpdated || albumNameUpdated) { //If there is a difference, copy it over and set an updated flag
				strcpy(espod.trackTitle,incTrackTitle);
				trackTitleUpdated = true;
			}
			break;
		case ESP_AVRC_MD_ATTR_PLAYING_TIME: //No checks on duration, always update
			espod.trackDuration = String((char*)text).toInt();
			break;
	}
	if(albumNameUpdated && artistNameUpdated && trackTitleUpdated) { //This is a possible condition even if playing from the same artist/album or two songs with the same title
		if(espod.waitMetadataUpdate) { //If this was expected, it means the trickeries with index were done directly on the espod
			artistNameUpdated = false;
			albumNameUpdated = false;
			trackTitleUpdated = false;
			espod.waitMetadataUpdate = false; //Given the duplicate conditions, this is irrelevant to keep that flag up
		}
		else { //This was "unprovoked update", try to determine if this is a PREV or a NEXT
			if((strcmp(espod.artistName,prevArtistName)==0) && (strcmp(espod.albumName,prevAlbumName)==0) && (strcmp(espod.trackTitle,prevTrackTitle)==0)) {
				//This is very certainly a Previous ... rewind the currentTrackIndex
				espod.trackListPosition = (espod.trackListPosition + TOTAL_NUM_TRACKS -1)%TOTAL_NUM_TRACKS; //Same thing as doing a -1 with some safety
				espod.currentTrackIndex = espod.trackList[espod.trackListPosition];
			}
			else { //Something is different, assume it is a next (the case for identicals is impossible)
				espod.trackListPosition = (espod.trackListPosition+1) % TOTAL_NUM_TRACKS;
				espod.trackList[espod.trackListPosition] = (espod.currentTrackIndex+1) % TOTAL_NUM_TRACKS;
				espod.currentTrackIndex++;
			}
			espod.L0x04_0x27_PlayStatusNotification(0x01,espod.currentTrackIndex);
		}
	}

}

#endif

/// @brief Callback function that passes intended operations from the esPod to the A2DP player
/// @param playCommand A2DP_xx command instruction. It does not match the PB_CMD_xx codes !!!
void playStatusHandler(byte playCommand) {
	#ifdef ENABLE_A2DP
  	switch (playCommand) {
		case A2DP_STOP:
			a2dp_sink.stop();
			//Stoppage notification is handled internally in the espod
			break;
		case A2DP_PLAY:
			a2dp_sink.play();
			//Watch out for possible metadata
			break;
		case A2DP_PAUSE:
			a2dp_sink.pause();
			break;
		case A2DP_REWIND:
			a2dp_sink.previous();
			break;
		case A2DP_NEXT:
			a2dp_sink.next();
			break;
		case A2DP_PREV: //We will assume that the timing difference for a PREV and a REWIND is similar on the AVRC TG and the CT/Radio
			a2dp_sink.previous();
			break;
	}
  	#endif
}

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
		a2dp_sink.set_auto_reconnect(true); //Auto-reconnect
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
 	
	//Prep and start up espod
	espod.attachPlayControlHandler(playStatusHandler);

	#ifdef ENABLE_A2DP
	//Let's wait for something to start before we enable espod and start the game.
		while(a2dp_sink.get_connection_state()!=ESP_A2D_CONNECTION_STATE_CONNECTED) {
			delay(10);
		}
	#endif

}

void loop() {
	if(espodRefreshTimer) {
		espod.refresh();
	}
	if(notificationsRefresh) { //currently does nothing...
		espod.cyclicNotify();
	}
}
