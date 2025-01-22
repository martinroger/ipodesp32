#include <Arduino.h>

#include "AudioTools.h"
#include "BluetoothA2DPSink.h"
#include "AudioTools/AudioLibs/I2SCodecStream.h"
#include "AudioBoard.h"
AudioInfo info(44100,2,16);
DriverPins minimalPins;
AudioBoard minimalAudioKit(AudioDriverES8388,minimalPins);
I2SCodecStream i2s(minimalAudioKit);
BluetoothA2DPSink a2dp_sink(i2s);

#pragma region A2DP/AVRC callbacks



void setup() {
	ESP_LOGI("SETUP","setup() start");
	minimalPins.addI2C(PinFunction::CODEC, 32, 33);
	minimalPins.addI2S(PinFunction::CODEC, 0, 27, 25, 26, 35);
	auto cfg = i2s.defaultConfig();
	cfg.copyFrom(info);
	i2s.begin(cfg);
	ESP_LOGI("SETUP","a2dp_sink callbacks attach start");
	a2dp_sink.set_auto_reconnect(true); //Auto-reconnect
	a2dp_sink.start("MiNiPoD56");
	ESP_LOGI("SETUP","Waiting for peer");
	while(a2dp_sink.get_connection_state()!=ESP_A2D_CONNECTION_STATE_CONNECTED) {
		delay(10);
	}
	delay(50);
	ESP_LOGI("SETUP","Peer connected: %s",a2dp_sink.get_peer_name());

}

void loop() {
	delay(100); //Yield
}
