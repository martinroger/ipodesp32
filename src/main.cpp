#include <Arduino.h>
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
	#ifdef AUDIOKIT
		#ifndef LED_BUILTIN
			#define LED_BUILTIN 22
		#else
			#undef LED_BUILTIN
			#define LED_BUILTIN 22
		#endif
		#ifdef USE_SD
				#ifndef LED_SD
					#define LED_SD 19
				#else
					#undef LED_SD
					#define LED_SD 19
				#endif
			#include "sdLogUpdate.h"
			bool sdLoggerEnabled = false;
		#endif
		#include "AudioTools/AudioLibs/I2SCodecStream.h"
		#include "AudioBoard.h"
		AudioInfo info(44100,2,16);
		DriverPins minimalPins;
		AudioBoard minimalAudioKit(AudioDriverES8388,minimalPins);
		I2SCodecStream i2s(minimalAudioKit);
		BluetoothA2DPSink a2dp_sink(i2s);
	#endif
#endif

#ifndef AUDIOKIT
	esPod espod(Serial2);
#else
	//HardwareSerial ipodSerial(1);
	//esPod espod(ipodSerial);
	esPod espod(Serial);
#endif
#ifndef REFRESH_INTERVAL
	#define REFRESH_INTERVAL 5
#endif
unsigned long lastTick_ts = 0;

char incAlbumName[255] 		= 	"incAlbum";
char incArtistName[255] 	= 	"incArtist";
char incTrackTitle[255] 	= 	"incTitle";
uint32_t incTrackDuration 	= 	0;
bool albumNameUpdated 		= 	false;
bool artistNameUpdated 		= 	false;
bool trackTitleUpdated 		= 	false;
bool trackDurationUpdated	=	false;

#pragma region A2DP/AVRC callbacks
#ifdef ENABLE_A2DP
/// @brief callback on changes of A2DP connection and AVRCP connection. Turns a LED on, enables the espod.
/// @param state New state passed by the callback.
/// @param ptr Not used.
void connectionStateChanged(esp_a2d_connection_state_t state, void* ptr) {
	switch (state)	{
		case ESP_A2D_CONNECTION_STATE_CONNECTED:
			#ifdef LED_BUILTIN
				digitalWrite(LED_BUILTIN,!digitalRead(LED_BUILTIN));
			#endif
			ESP_LOGI("A2DP_CB","ESP_A2D_CONNECTION_STATE_CONNECTED, espod enabled");
			espod.disabled = false;
			break;
		case ESP_A2D_CONNECTION_STATE_DISCONNECTED:
			#ifdef LED_BUILTIN
				digitalWrite(LED_BUILTIN,!digitalRead(LED_BUILTIN));
			#endif
			ESP_LOGI("A2DP_CB","ESP_A2D_CONNECTION_STATE_DISCONNECTED, espod disabled");
			espod.resetState();
			espod.disabled = true; //Todo check of this one, is risky
			break;
	}
}

/// @brief Callback for the change of playstate after connection. Aligns the state of the esPod to the state of the phone. Play should be called by the espod interaction
/// @param state The A2DP Stream to align to.
/// @param ptr Not used.
void audioStateChanged(esp_a2d_audio_state_t state,void* ptr) {
	switch (state)	
	{
		case ESP_A2D_AUDIO_STATE_STARTED:
			espod.playStatus = PB_STATE_PLAYING;
			ESP_LOGI("A2DP_CB","ESP_A2D_AUDIO_STATE_STARTED, espod.playStatus = PB_STATE_PLAYING");
			break;
		case ESP_A2D_AUDIO_STATE_REMOTE_SUSPEND:
			espod.playStatus = PB_STATE_PAUSED;
			ESP_LOGI("A2DP_CB","ESP_A2D_AUDIO_STATE_REMOTE_SUSPEND, espod.playStatus = PB_STATE_PAUSED");
			break;
		case ESP_A2D_AUDIO_STATE_STOPPED:
			espod.playStatus = PB_STATE_STOPPED;
			ESP_LOGI("A2DP_CB","ESP_A2D_AUDIO_STATE_STOPPED, espod.playStatus = PB_STATE_STOPPED");
			break;
	}
}

/// @brief Play position callback returning the ms spent since start on every interval
/// @param play_pos Playing Position in ms
void avrc_rn_play_pos_callback(uint32_t play_pos) {
	espod.playPosition = play_pos;
	ESP_LOGV("AVRC_CB","PlayPosition called");
	if(espod.playStatusNotificationState==NOTIF_ON && espod.trackChangeAckPending==0x00) 
	{
		espod.L0x04_0x27_PlayStatusNotification(0x04,play_pos);
	}
}

/// @brief Catch callback for the AVRC metadata. There can be duplicates !
/// @param id Metadata attribute ID : ESP_AVRC_MD_ATTR_xxx
/// @param text Text data passed around, sometimes it's a uint32_t
void avrc_metadata_callback(uint8_t id, const uint8_t *text) {
	switch (id)	
	{
		case ESP_AVRC_MD_ATTR_ALBUM:
			strcpy(incAlbumName,(char*)text); //Buffer the incoming album string
			if(espod.trackChangeAckPending>0x00)  //There is a pending metadata update
			{
				if(!albumNameUpdated)  //The album Name has not been updated yet
				{
					strcpy(espod.albumName,incAlbumName);
					albumNameUpdated = true;
					ESP_LOGI("AVRC_CB","Album rxed, ACK pending, albumNameUpdated to %s",espod.albumName);
				}
				else 
				{
					ESP_LOGI("AVRC_CB","Album rxed, ACK pending, already updated to %s",espod.albumName);
				}
			}
			else //There is no pending track change from iPod : active or passive track change from avrc target
			{
				if(strcmp(incAlbumName,espod.albumName)!=0)  //Different incoming metadata
				{
					strcpy(espod.prevAlbumName,espod.albumName);
					strcpy(espod.albumName,incAlbumName);
					albumNameUpdated = true;
					ESP_LOGI("AVRC_CB","Album rxed, NO ACK pending, albumNameUpdated to %s",espod.albumName);
				}
				else  //Despammer for double sends
				{
					ESP_LOGI("AVRC_CB","Album rxed, NO ACK pending, already updated to %s",espod.albumName);
				}
			}
			break;


		case ESP_AVRC_MD_ATTR_ARTIST:
			strcpy(incArtistName,(char*)text); //Buffer the incoming artist string
			if(espod.trackChangeAckPending>0x00) //There is a pending metadata update
			{
				if(!artistNameUpdated)  //The artist name has not been updated yet
				{
					strcpy(espod.artistName,incArtistName);
					artistNameUpdated = true;
					ESP_LOGI("AVRC_CB","Artist rxed, ACK pending, artistNameUpdated to %s",espod.artistName);
				}
				else 
				{
					ESP_LOGI("AVRC_CB","Artist rxed, ACK pending, already updated to %s",espod.artistName);
				}
			}
			else  //There is no pending track change from iPod : active or passive track change from avrc target
			{
				if(strcmp(incArtistName,espod.artistName)!=0)  //Different incoming metadata
				{
					strcpy(espod.prevArtistName,espod.artistName);
					strcpy(espod.artistName,incArtistName);
					artistNameUpdated = true;
					ESP_LOGI("AVRC_CB","Artist rxed, NO ACK pending, artistNameUdpated to %s",espod.artistName);
				}
				else  //Despammer for double sends
				{
					ESP_LOGI("AVRC_CB","Artist rxed, NO ACK pending, already updated to %s",espod.artistName);
				}
			}
			break;


		case ESP_AVRC_MD_ATTR_TITLE: //Title change triggers the NEXT track assumption if unexpected. It is too intensive to try to do NEXT/PREV guesswork
			strcpy(incTrackTitle,(char*)text); //Buffer the incoming track title
			if(espod.trackChangeAckPending>0x00)//There is a pending metadata update
			{ 
				if(!trackTitleUpdated) //The track title has not been updated yet
				{ 
					strcpy(espod.trackTitle,incTrackTitle);
					trackTitleUpdated = true;

					ESP_LOGI("AVRC_CB","Title rxed, ACK pending, trackTitleUpdated to %s",espod.trackTitle);
				}
				else 
				{
					ESP_LOGI("AVRC_CB","Title rxed, ACK pending, already updated to %s",espod.trackTitle);
				}
			}
			else { //There is no pending track change from iPod : active or passive track change from avrc target
				if(strcmp(incTrackTitle,espod.trackTitle)!=0)  //Different from current track Title -> Systematic NEXT
				{
					//Assume it is Next, perform cursor operations
					espod.trackListPosition = (espod.trackListPosition + 1 ) % TOTAL_NUM_TRACKS;
					espod.prevTrackIndex = espod.currentTrackIndex;
					espod.currentTrackIndex = (espod.currentTrackIndex + 1 ) % TOTAL_NUM_TRACKS;
					espod.trackList[espod.trackListPosition] = (espod.currentTrackIndex);
					//Copy new title and flag that data has been updated
					strcpy(espod.prevTrackTitle,espod.trackTitle);
					strcpy(espod.trackTitle,incTrackTitle);
					trackTitleUpdated = true;
					ESP_LOGI("AVRC_CB","Title rxed, NO ACK pending, AUTONEXT, trackTitleUpdated to %s\n\ttrackPos %d trackIndex %d",espod.trackTitle,espod.trackListPosition,espod.currentTrackIndex);
				}
				else //Despammer for double sends
				{ 
					ESP_LOGI("AVRC_CB","Title rxed, NO ACK pending, same name : %s",espod.trackTitle);
				}
			}
			break;


		case ESP_AVRC_MD_ATTR_PLAYING_TIME: 
			incTrackDuration = String((char*)text).toInt();
			if(espod.trackChangeAckPending>0x00) //There is a pending metadata update
			{ 
				if(!trackDurationUpdated) //The duration has not been updated yet
				{ 
					espod.trackDuration = incTrackDuration;
					trackDurationUpdated = true;
					ESP_LOGI("AVRC_CB","Duration rxed, ACK pending, trackDurationUpdated to %d",espod.trackDuration);
				}
				else 
				{
					ESP_LOGI("AVRC_CB","Duration rxed, ACK pending, already updated to %d",espod.trackDuration);
				}
			}
			else { //There is no pending track change from iPod : active or passive track change from avrc target
				if(incTrackDuration != espod.trackDuration) //Different incoming metadata
				{ 
					espod.trackDuration = incTrackDuration;
					trackDurationUpdated = true;
					ESP_LOGI("AVRC_CB","Duration rxed, NO ACK pending, trackDurationUpdated to %d",espod.trackDuration);
				}
				else //Despammer for double sends
				{ 
					ESP_LOGI("AVRC_CB","Duration rxed, NO ACK pending, already updated to %d",espod.trackDuration);
				}
			}
			break;
	}

	//Check if it is time to send a notification
	if(albumNameUpdated && artistNameUpdated && trackTitleUpdated && trackDurationUpdated )
	{ 
		//If all fields have received at least one update and the trackChangeAckPending is still hanging. The failsafe for this one is in the espod.refresh()
		if (espod.trackChangeAckPending>0x00) 
		{
			ESP_LOGI("AVRC_CB","Artist+Album+Title+Duration +++ ACK Pending 0x%x\n\tPending duration: %d",espod.trackChangeAckPending,millis()-espod.trackChangeTimestamp);
			espod.L0x04_0x01_iPodAck(iPodAck_OK,espod.trackChangeAckPending);
			espod.trackChangeAckPending = 0x00;
			ESP_LOGI("AVRC_CB","trackChangeAckPending reset to 0x00");
		}
		albumNameUpdated 	= 	false;
		artistNameUpdated 	= 	false;
		trackTitleUpdated 	= 	false;
		trackDurationUpdated=	false;
		ESP_LOGI("AVRC_CB","Artist+Album+Title+Duration : True -> False");
		//Inform the car
		if (espod.playStatusNotificationState==NOTIF_ON) 
		{
			espod.L0x04_0x27_PlayStatusNotification(0x01,espod.currentTrackIndex);
		}
	}
}

#endif
#pragma endregion

/// @brief Callback function that passes intended operations from the esPod to the A2DP player
/// @param playCommand A2DP_xx command instruction. It does not match the PB_CMD_xx codes !!!
void playStatusHandler(byte playCommand) {
	#ifdef ENABLE_A2DP
  	switch (playCommand) {
		case A2DP_STOP:
			a2dp_sink.stop();
			ESP_LOGI("A2DP_CB","A2DP_STOP");
			break;
		case A2DP_PLAY:
			a2dp_sink.play();
			ESP_LOGI("A2DP_CB","A2DP_PLAY");
			break;

		case A2DP_PAUSE:
			a2dp_sink.pause();
			ESP_LOGI("A2DP_CB","A2DP_PAUSE");
			break;

		case A2DP_REWIND:
			a2dp_sink.previous();
			ESP_LOGI("A2DP_CB","A2DP_REWIND");
			break;

		case A2DP_NEXT:
			a2dp_sink.next();
			ESP_LOGI("A2DP_CB","A2DP_NEXT");
			break;

		case A2DP_PREV: 
			a2dp_sink.previous();
			ESP_LOGI("A2DP_CB","A2DP_PREV");
			break;
	}
  	#endif
}

void setup() {
	esp_log_level_set("*",ESP_LOG_NONE); //Necessary not to spam the Serial
	ESP_LOGI("SETUP","setup() start");
	#ifdef USE_SD //Main check for FW and start logging
		pinMode(LED_SD,OUTPUT);
		pinMode(SD_DETECT,INPUT);
		pinMode(5,INPUT_PULLUP);
		pinMode(18,INPUT_PULLUP);
		if(digitalRead(SD_DETECT) == LOW) {
			if(initSD()) 
			{
				digitalWrite(LED_SD,LOW); //Turn the SD LED ON
				#ifdef LOG_TO_SD
				sdLoggerEnabled = initSDLogger();
				if(sdLoggerEnabled) esp_log_level_set("*", ESP_LOG_INFO);
				digitalWrite(LED_SD,sdLoggerEnabled);
				#endif
				//Attempt to update
				updateFromFS(SD_MMC);
			}
		}
		if(!digitalRead(18)) esp_log_level_set("*", ESP_LOG_INFO); //Backdoor to force Serial logs in case of no SD. Button 5
	#endif
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
			WSEL  ->  25
			DIN   ->  22
			BCLK  ->  26
			*/
		#endif
		#ifdef AUDIOKIT
			minimalPins.addI2C(PinFunction::CODEC, 32, 33);
			minimalPins.addI2S(PinFunction::CODEC, 0, 27, 25, 26, 35);
			auto cfg = i2s.defaultConfig();
			cfg.copyFrom(info);
			i2s.begin(cfg);
		#endif
		ESP_LOGI("SETUP","a2dp_sink callbacks attach start");
		a2dp_sink.set_auto_reconnect(true); //Auto-reconnect
		a2dp_sink.set_on_connection_state_changed(connectionStateChanged);
		a2dp_sink.set_on_audio_state_changed(audioStateChanged);
		a2dp_sink.set_avrc_metadata_callback(avrc_metadata_callback);
		a2dp_sink.set_avrc_metadata_attribute_mask(ESP_AVRC_MD_ATTR_TITLE|ESP_AVRC_MD_ATTR_ARTIST|ESP_AVRC_MD_ATTR_ALBUM|ESP_AVRC_MD_ATTR_PLAYING_TIME);
		a2dp_sink.set_avrc_rn_play_pos_callback(avrc_rn_play_pos_callback,1);
		#ifdef AUDIOKIT
			a2dp_sink.start("MiNiPoD56");
		#else
			a2dp_sink.start("espiPod 2");
		#endif
		ESP_LOGI("SETUP","a2dp_sink started : %s",a2dp_sink.get_name());
		delay(5);
		#ifdef LED_BUILTIN
			pinMode(LED_BUILTIN,OUTPUT);
			digitalWrite(LED_BUILTIN,LOW);
		#endif
	#endif

	#ifndef AUDIOKIT
		Serial2.setRxBufferSize(4096);
		Serial2.setTxBufferSize(4096);
		Serial2.begin(19200);
	#else
		// ipodSerial.setPins(19,22);
		// ipodSerial.setRxBufferSize(4096);
		// ipodSerial.setTxBufferSize(4096);
		// ipodSerial.begin(19200);
		// digitalWrite(LED_BUILTIN,HIGH);
		Serial.setRxBufferSize(1024);
		Serial.setTxBufferSize(1024);
		Serial.begin(19200);
	#endif
 	
	//Prep and start up espod
	espod.attachPlayControlHandler(playStatusHandler);

	#ifdef ENABLE_A2DP
	//Let's wait for something to start before we enable espod and start the game.
		ESP_LOGI("SETUP","Waiting for peer");
		while(a2dp_sink.get_connection_state()!=ESP_A2D_CONNECTION_STATE_CONNECTED) {
			delay(10);
		}
		delay(50);
		ESP_LOGI("SETUP","Peer connected: %s",a2dp_sink.get_peer_name());
	#endif
	ESP_LOGI("SETUP","Setup finished");
}

void loop() {
	if(millis()-lastTick_ts>REFRESH_INTERVAL) {
		espod.refresh();
		lastTick_ts = millis();
	}
}
