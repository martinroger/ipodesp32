#include "ESP32AudioProvider.h"

#include "esPod_conf.h"

// Static pointer to the current instance for callback use
static ESP32AudioProvider* s_instance = nullptr;

I2SStream ESP32AudioProvider::i2s; // Static I2SStream instance

ESP32AudioProvider::ESP32AudioProvider()
{
    _metadataMutex = xSemaphoreCreateMutex();
    s_instance = this;
}

void ESP32AudioProvider::read_data_stream(const uint8_t *data, uint32_t length)
{
    i2s.write(data, length);
}

void ESP32AudioProvider::initializeA2DPSink()
{
#ifdef AUDIOKIT
    minimalPins.addI2C(PinFunction::CODEC, 32, 33);
    minimalPins.addI2S(PinFunction::CODEC, 0, 27, 25, 26, 35);
    auto cfg = i2s.defaultConfig();
    cfg.copyFrom(info);
    i2s.begin(cfg);
#else
    a2dp_sink.set_stream_reader(read_data_stream, false);
    auto cfg = i2s.defaultConfig(TX_MODE);
    cfg.pin_ws = 25;   // Default is 15
    cfg.pin_data = 26; // Default is 22
    cfg.pin_bck = 27;  // Default is 14
    cfg.sample_rate = 44100;
    cfg.i2s_format = I2S_LSB_FORMAT;
    i2s.begin(cfg);
#endif

    a2dp_sink.set_auto_reconnect(true, 10000);
    a2dp_sink.set_on_connection_state_changed(connectionStateChanged);
    a2dp_sink.set_on_audio_state_changed(audioStateChanged);
    a2dp_sink.set_avrc_metadata_callback(avrc_metadata_callback); // Only function pointer allowed
    a2dp_sink.set_avrc_metadata_attribute_mask(ESP_AVRC_MD_ATTR_TITLE | ESP_AVRC_MD_ATTR_ARTIST |
                                               ESP_AVRC_MD_ATTR_ALBUM | ESP_AVRC_MD_ATTR_PLAYING_TIME);
    a2dp_sink.set_avrc_rn_play_pos_callback(avrc_rn_play_pos_callback, 1);

    a2dp_sink.start(A2DP_SINK_NAME);

    ESP_LOGI("SETUP", "a2dp_sink started: %s", A2DP_SINK_NAME);
    delay(5);
}

esp_err_t ESP32AudioProvider::start()
{
    initializeA2DPSink();
    return ESP_OK;
}

bool ESP32AudioProvider::is_connected()
{
    return a2dp_sink.get_connection_state() == ESP_A2D_CONNECTION_STATE_CONNECTED;
}

bool ESP32AudioProvider::is_playing()
{
    // Implement based on a2dp_sink or internal state
    return a2dp_sink.get_audio_state() == ESP_A2D_AUDIO_STATE_STARTED;
}

const char* ESP32AudioProvider::get_device_name()
{
    // Return the connected device name if available
    return a2dp_sink.get_peer_name();
}

// Static callback
void ESP32AudioProvider::avrc_metadata_callback(uint8_t id, const uint8_t *text)
{
    ESP32AudioProvider* self = s_instance;
    if (!self || !text) return;
    if (self->_metadataMutex) xSemaphoreTake(self->_metadataMutex, portMAX_DELAY);

    switch (id) {
        case ESP_AVRC_MD_ATTR_TITLE:
            strncpy(self->_title, (const char*)text, sizeof(self->_title) - 1);
            self->_title[sizeof(self->_title) - 1] = '\0';
            break;
        case ESP_AVRC_MD_ATTR_ARTIST:
            strncpy(self->_artist, (const char*)text, sizeof(self->_artist) - 1);
            self->_artist[sizeof(self->_artist) - 1] = '\0';
            break;
        case ESP_AVRC_MD_ATTR_ALBUM:
            strncpy(self->_album, (const char*)text, sizeof(self->_album) - 1);
            self->_album[sizeof(self->_album) - 1] = '\0';
            break;
        case ESP_AVRC_MD_ATTR_GENRE:
            strncpy(self->_genre, (const char*)text, sizeof(self->_genre) - 1);
            self->_genre[sizeof(self->_genre) - 1] = '\0';
            break;
        case ESP_AVRC_MD_ATTR_PLAYING_TIME:
            self->_duration = atoi((const char*)text);
            break;
        default:
            break;
    }
    if (self->_metadataMutex) xSemaphoreGive(self->_metadataMutex);
}

// Metadata getters
const char* ESP32AudioProvider::get_playing_song_title() {
    if (_metadataMutex) xSemaphoreTake(_metadataMutex, portMAX_DELAY);
    const char* ret = _title;
    if (_metadataMutex) xSemaphoreGive(_metadataMutex);
    return ret;
}

const char* ESP32AudioProvider::get_playing_song_artist() {
    if (_metadataMutex) xSemaphoreTake(_metadataMutex, portMAX_DELAY);
    const char* ret = _artist;
    if (_metadataMutex) xSemaphoreGive(_metadataMutex);
    return ret;
}

const char* ESP32AudioProvider::get_playing_song_album() {
    if (_metadataMutex) xSemaphoreTake(_metadataMutex, portMAX_DELAY);
    const char* ret = _album;
    if (_metadataMutex) xSemaphoreGive(_metadataMutex);
    return ret;
}

const char* ESP32AudioProvider::get_playing_song_genre() {
    if (_metadataMutex) xSemaphoreTake(_metadataMutex, portMAX_DELAY);
    const char* ret = _genre;
    if (_metadataMutex) xSemaphoreGive(_metadataMutex);
    return ret;
}

uint32_t ESP32AudioProvider::get_playing_song_total_time() {
    if (_metadataMutex) xSemaphoreTake(_metadataMutex, portMAX_DELAY);
    uint32_t ret = _duration;
    if (_metadataMutex) xSemaphoreGive(_metadataMutex);
    return ret;
}

uint32_t ESP32AudioProvider::get_playing_song_elapsed_time() {
    // If you have a callback for play position, update _position there.
    if (_metadataMutex) xSemaphoreTake(_metadataMutex, portMAX_DELAY);
    uint32_t ret = _position;
    if (_metadataMutex) xSemaphoreGive(_metadataMutex);
    return ret;
}

bool ESP32AudioProvider::play()
{
    // Implement play command via AVRCP
    a2dp_sink.play();
    return true;
}

bool ESP32AudioProvider::pause()
{
    a2dp_sink.pause();
    return true;
}

bool ESP32AudioProvider::stop()
{
    a2dp_sink.stop();
    return true;
}

bool ESP32AudioProvider::next()
{
    a2dp_sink.next();
    return true;
}

bool ESP32AudioProvider::previous()
{
    a2dp_sink.previous();
    return true;
}

bool ESP32AudioProvider::seek(long position)
{
    // Implement seek if supported
    return false;
}

void ESP32AudioProvider::connectionStateChanged(esp_a2d_connection_state_t state, void *ptr)
{
    // Example: log the state change, expand as needed
    switch (state)
    {
        case ESP_A2D_CONNECTION_STATE_CONNECTED:
            ESP_LOGI("ESP32AudioProvider", "A2DP Connected");
            break;
        case ESP_A2D_CONNECTION_STATE_DISCONNECTED:
            ESP_LOGI("ESP32AudioProvider", "A2DP Disconnected");
            break;
        default:
            ESP_LOGI("ESP32AudioProvider", "A2DP Connection State Changed: %d", state);
            break;
    }
    // Notify registered callback if present
    if (s_instance && s_instance->_connStateCb) {
        s_instance->_connStateCb(state, s_instance->_connStateCbContext);
    }
    // Add further handling if needed (e.g., notify listeners, reset metadata, etc.)
}

void ESP32AudioProvider::audioStateChanged(esp_a2d_audio_state_t state, void *ptr)
{
    ESP32AudioProvider* self = s_instance;
    if (!self) return;
    switch (state)
    {
        case ESP_A2D_AUDIO_STATE_STARTED:
            if (self->_metadataMutex) xSemaphoreTake(self->_metadataMutex, portMAX_DELAY);
            self->_playState = PB_STATE_PLAYING;
            if (self->_metadataMutex) xSemaphoreGive(self->_metadataMutex);
            ESP_LOGI("ESP32AudioProvider", "A2DP Audio Started");
            break;
        case ESP_A2D_AUDIO_STATE_REMOTE_SUSPEND:
            if (self->_metadataMutex) xSemaphoreTake(self->_metadataMutex, portMAX_DELAY);
            self->_playState = PB_STATE_PAUSED;
            if (self->_metadataMutex) xSemaphoreGive(self->_metadataMutex);
            ESP_LOGI("ESP32AudioProvider", "A2DP Audio Remote Suspend");
            break;
        case ESP_A2D_AUDIO_STATE_STOPPED:
            if (self->_metadataMutex) xSemaphoreTake(self->_metadataMutex, portMAX_DELAY);
            self->_playState = PB_STATE_STOPPED;
            if (self->_metadataMutex) xSemaphoreGive(self->_metadataMutex);
            ESP_LOGI("ESP32AudioProvider", "A2DP Audio Stopped");
            break;
        default:
            break;
    }
}

void ESP32AudioProvider::avrc_rn_play_pos_callback(uint32_t play_pos)
{
    ESP32AudioProvider* self = s_instance;
    if (!self) return;
    if (self->_metadataMutex) xSemaphoreTake(self->_metadataMutex, portMAX_DELAY);
    self->_position = play_pos;
    if (self->_metadataMutex) xSemaphoreGive(self->_metadataMutex);
    ESP_LOGV("ESP32AudioProvider", "Play position updated: %u ms", play_pos);
}
