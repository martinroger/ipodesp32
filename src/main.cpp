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

esPod espod(Serial2);

Timer<millis> espodRefreshTimer = 5;
Timer<millis> notificationsRefresh = 500;

//Reminders used to keep track of previous changes
char incAlbumName[255] = "incAlbum";
//char prevAlbumName[255] = "PrevAlbum";
char incArtistName[255] = "incArtist";
//char prevArtistName[255] = "PrevArtist";
char incTrackTitle[255] = "incTitle";
//char prevTrackTitle[255] = "PrevTitle";
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
			#ifdef DEBUG_MODE
				Serial.println("ESP_A2D_CONNECTION_STATE_CONNECTED, espod enabled");
			#endif
			espod.disabled = false;
			break;
		case ESP_A2D_CONNECTION_STATE_DISCONNECTED:
			#ifdef LED_BUILTIN
				digitalWrite(LED_BUILTIN,LOW);
			#endif
			#ifdef DEBUG_MODE
				Serial.println("ESP_A2D_CONNECTION_STATE_DISCONNECTED, espod disabled");
			#endif
			espod.resetState();
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
			#ifdef DEBUG_MODE
				Serial.println("ESP_A2D_AUDIO_STATE_STARTED, espod.playStatus = PB_STATE_PLAYING");
			#endif
			break;
		case ESP_A2D_AUDIO_STATE_REMOTE_SUSPEND:
			espod.playStatus = PB_STATE_PAUSED;
			#ifdef DEBUG_MODE
				Serial.println("ESP_A2D_AUDIO_STATE_REMOTE_SUSPEND, espod.playStatus = PB_STATE_PAUSED");
			#endif
			break;
		case ESP_A2D_AUDIO_STATE_STOPPED:
			espod.playStatus = PB_STATE_STOPPED;
			#ifdef DEBUG_MODE
				Serial.println("ESP_A2D_AUDIO_STATE_STOPPED, espod.playStatus = PB_STATE_STOPPED");
			#endif
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
	//TODO : systematically handle L0x04_0x27_PlayStatusNotification firing after track change ?
	//TODO : complete overhaul ! Also drop the main.cpp prevXXX
	/* Introducing :
	TRACK_CHANGE_TIMEOUT <- Delay until which the ack sent anyways
	trackChangeAckPending <- Send a L0x04_0x01 ACK to the value of this byte with L0x04_0x01_iPodAck(iPodAck_OK,cmdID), then resets it to 0x00
	trackChangeTimestamp <- Send the ACK anyways after millis() minus this value exceeds TRACK_CHANGE_TIMEOUT in espod.refresh()
	*/
	switch (id)	{
		case ESP_AVRC_MD_ATTR_ALBUM:
			strcpy(incAlbumName,(char*)text); //Buffer the incoming album string
			if(espod.trackChangeAckPending>0x00) { //There is a pending metadata update
				if(!albumNameUpdated) { //The album Name has not been updated yet
					strcpy(espod.prevAlbumName,espod.albumName);
					strcpy(espod.albumName,incAlbumName);
					albumNameUpdated = true;
					#ifdef DEBUG_MODE
						Serial.printf("Album rxed, ACK pending, albumNameUpdate to %s from %s \n",incAlbumName,espod.prevAlbumName);
					#endif
				}
				else {
					#ifdef DEBUG_MODE
						Serial.printf("Album rxed %s, ACK pending, already updated to %s \n",incAlbumName,espod.albumName);
					#endif
				}
			}
			else { //There is no pending track change
				if(strcmp(incAlbumName,espod.albumName)!=0) { //If not the current albumName
					strcpy(espod.prevAlbumName,espod.albumName);
					strcpy(espod.albumName,incAlbumName);
					albumNameUpdated = true;
					//Use the albumNameUpdated flag here ?
					#ifdef DEBUG_MODE
						Serial.printf("Album rxed, NO ACK pending, albumNameUpdate to %s from %s \n",incAlbumName,espod.prevAlbumName);
					#endif
				}
				else {
					#ifdef DEBUG_MODE
						Serial.printf("Album rxed, NO ACK pending, same name : %s \n",incAlbumName);
					#endif
				}
				//TODO : check if previously active album ?
				//No Next management 
			}
			// if((strcmp(incAlbumName,espod.albumName)!=0) || artistNameUpdated || trackTitleUpdated) { //If there is a difference, copy it over and set an updated flag
			// 	strcpy(espod.albumName,incAlbumName);
			// 	albumNameUpdated = true;
			// }
			break;
		case ESP_AVRC_MD_ATTR_ARTIST:
			strcpy(incArtistName,(char*)text); //Buffer the incoming artist string
			if(espod.trackChangeAckPending>0x00) {
				if(!artistNameUpdated) {
					strcpy(espod.prevArtistName,espod.artistName);
					strcpy(espod.artistName,incArtistName);
					artistNameUpdated = true;
					#ifdef DEBUG_MODE
						Serial.printf("Artist rxed, ACK pending, artistNameUpdated to %s from %s \n",incArtistName,espod.prevArtistName);
					#endif
				}
				else {
					#ifdef DEBUG_MODE
						Serial.printf("Artist rxed %s, ACK pending, already updated to %s \n",incArtistName,espod.artistName);
					#endif
				}
			}
			else { //No pending track change, update if different from current
				if(strcmp(incArtistName,espod.artistName)!=0) {
					strcpy(espod.prevArtistName,espod.artistName);
					strcpy(espod.artistName,incArtistName);
					artistNameUpdated = true;
					//Use the artistNameUpdate flag here ?
					#ifdef DEBUG_MODE
						Serial.printf("Artist rxed, NO ACK pending, artistNameUdpated to %s from %s \n",incArtistName,espod.prevArtistName);
					#endif
				}
				else {
					#ifdef DEBUG_MODE
						Serial.printf("Artist rxed, NO ACK pending, same name : %s \n",incArtistName);
					#endif
				}
				//TODO : check that other comparisons are indeed invalid
			}
			// if((strcmp(incArtistName,espod.artistName)!=0) || albumNameUpdated || trackTitleUpdated) { //If there is a difference, copy it over and set an updated flag
			// 	strcpy(espod.artistName,incArtistName);
			// 	artistNameUpdated = true;
			// }
			break;
		case ESP_AVRC_MD_ATTR_TITLE:
			strcpy(incTrackTitle,(char*)text); //Buffer the incoming track title
			if(espod.trackChangeAckPending>0x00) {
				if(!trackTitleUpdated) {
					strcpy(espod.prevTrackTitle,espod.trackTitle);
					strcpy(espod.trackTitle,incTrackTitle);
					trackTitleUpdated = true;
					#ifdef DEBUG_MODE
						Serial.printf("Title rxed, ACK pending, trackTitleUpdated to %s from %s \n",incTrackTitle,espod.prevTrackTitle);
					#endif
				}
				else {
					#ifdef DEBUG_MODE
						Serial.printf("Title rxed %s, ACK pending, already updated to %s \n",incTrackTitle,espod.prevTrackTitle);
					#endif
				}
			}
			else { //Unexpected track change
				if(strcmp(incTrackTitle,espod.trackTitle)!=0) { //If it is not the current track name (weakness of having a playlist with all the same title)
					if(strcmp(incTrackTitle,espod.prevTrackTitle)!=0) {//It is also not the previous track
						espod.trackListPosition = (espod.trackListPosition+1) % TOTAL_NUM_TRACKS;
						espod.currentTrackIndex = (espod.currentTrackIndex + 1 ) % TOTAL_NUM_TRACKS;
						espod.trackList[espod.trackListPosition] = (espod.currentTrackIndex);
						#ifdef DEBUG_MODE
							Serial.printf("NO ACK pending, NEXT, pos %d index %d\n",espod.trackListPosition,espod.currentTrackIndex);
						#endif
					}
					else { //It IS the previous track
						espod.trackListPosition = (espod.trackListPosition + TOTAL_NUM_TRACKS -1) % TOTAL_NUM_TRACKS; //Same thing as doing a -1 with some safety
						espod.currentTrackIndex = espod.trackList[espod.trackListPosition];
						#ifdef DEBUG_MODE
							Serial.printf("NO ACK pending, PREV, pos %d index %d\n",espod.trackListPosition,espod.currentTrackIndex);
						#endif
					}
					//In any case copy the incoming string
					strcpy(espod.prevTrackTitle,espod.trackTitle);
					strcpy(espod.trackTitle,incTrackTitle);
					trackTitleUpdated = true;
					#ifdef DEBUG_MODE
						Serial.printf("\t Title now %s from %s",espod.trackTitle,espod.prevTrackTitle);
					#endif
				}
				else {
					#ifdef DEBUG_MODE
						Serial.printf("Title rxed, NO ACK pending, same name : %s \n",incTrackTitle);
					#endif
				}
			}
			// if((strcmp(incTrackTitle,espod.trackTitle)!=0) || artistNameUpdated || albumNameUpdated) { //If there is a difference, copy it over and set an updated flag
			// 	strcpy(espod.trackTitle,incTrackTitle);
			// 	trackTitleUpdated = true;
			// }
			break;
		case ESP_AVRC_MD_ATTR_PLAYING_TIME: //No checks on duration, always update
			espod.trackDuration = String((char*)text).toInt();
			#ifdef DEBUG_MODE
				Serial.printf("trackDuration rxed : %d \n",espod.trackDuration);
			#endif
			break;
	}
	//Check if it is ti,e to send a notification
	if(albumNameUpdated && artistNameUpdated && trackTitleUpdated ) { 
		//If all fields have received at least one update and the trackChangeAckPending is still hanging. The failsafe for this one is in the espod.refresh()
		if (espod.trackChangeAckPending>0x00) {
			#ifdef DEBUG_MODE
				Serial.printf("Artist+Album+Title +++ ACK Pending 0x%x\n",espod.trackChangeAckPending);
			#endif
			espod.L0x04_0x01_iPodAck(iPodAck_OK,espod.trackChangeAckPending);
			espod.trackChangeAckPending = 0x00;
			#ifdef DEBUG_MODE
				Serial.println("trackChangeAckPending reset to 0x00");
			#endif 
		}
		albumNameUpdated 	= 	false;
		artistNameUpdated 	= 	false;
		trackTitleUpdated 	= 	false;
		#ifdef DEBUG_MODE
			Serial.println("Artist+Album+Title true -> False");
		#endif
		//Inform the car
		espod.L0x04_0x27_PlayStatusNotification(0x01,espod.currentTrackIndex);

		// if(espod.waitMetadataUpdate) { //If this was expected, it means the trickeries with index were done directly on the espod
		// 	artistNameUpdated = false;
		// 	albumNameUpdated = false;
		// 	trackTitleUpdated = false;
		// 	espod.waitMetadataUpdate = false; //Given the duplicate conditions, this is irrelevant to keep that flag up
		// }
		// else { //This was "unprovoked update", try to determine if this is a PREV or a NEXT
		// 	if((strcmp(espod.artistName,prevArtistName)==0) && (strcmp(espod.albumName,prevAlbumName)==0) && (strcmp(espod.trackTitle,prevTrackTitle)==0)) {
		// 		//This is very certainly a Previous ... rewind the currentTrackIndex
		// 		espod.trackListPosition = (espod.trackListPosition + TOTAL_NUM_TRACKS -1)%TOTAL_NUM_TRACKS; //Same thing as doing a -1 with some safety
		// 		espod.currentTrackIndex = espod.trackList[espod.trackListPosition];
		// 	}
		// 	else { //Something is different, assume it is a next (the case for identicals is impossible)
		// 		espod.trackListPosition = (espod.trackListPosition+1) % TOTAL_NUM_TRACKS;
		// 		espod.trackList[espod.trackListPosition] = (espod.currentTrackIndex+1) % TOTAL_NUM_TRACKS;
		// 		espod.currentTrackIndex++;
		// 	}
		// 	espod.L0x04_0x27_PlayStatusNotification(0x01,espod.currentTrackIndex);
		// }
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
			WSEL  ->  25
			DIN   ->  22
			BCLK  ->  26
			*/
		#endif
		a2dp_sink.set_auto_reconnect(true); //Auto-reconnect
		a2dp_sink.set_on_connection_state_changed(connectionStateChanged);
		a2dp_sink.set_on_audio_state_changed(audioStateChanged);
		a2dp_sink.set_avrc_metadata_callback(avrc_metadata_callback);
		a2dp_sink.set_avrc_metadata_attribute_mask(ESP_AVRC_MD_ATTR_TITLE|ESP_AVRC_MD_ATTR_ARTIST|ESP_AVRC_MD_ATTR_ALBUM|ESP_AVRC_MD_ATTR_PLAYING_TIME);
		a2dp_sink.set_avrc_rn_play_pos_callback(avrc_rn_play_pos_callback);
		a2dp_sink.start("espiPod 2");

		#ifdef LED_BUILTIN
			pinMode(LED_BUILTIN,OUTPUT);
			digitalWrite(LED_BUILTIN,LOW);
		#endif
	#endif

	#ifdef DEBUG_MODE
		Serial.setTxBufferSize(4096);
		Serial.begin(115200);
  	#endif
		Serial2.setRxBufferSize(4096);
		Serial2.setTxBufferSize(4096);
		Serial2.begin(19200);

 	
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
