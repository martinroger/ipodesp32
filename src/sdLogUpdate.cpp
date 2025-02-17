#include <sdLogUpdate.h>

#ifdef TAG
    #undef TAG
#endif
#define TAG "SD_MMC"

File log_file;

int log_to_sd_card(const char* fmt, va_list args) {
    int ret;
    char buf[256];
    ret = vsnprintf(buf, sizeof(buf), fmt, args);
    if (log_file) {
        log_file.print(buf);
        log_file.flush();
    }
    return ret;
}

bool initSDLogger() {
    bool ret = false;
    if (!digitalRead(5)) SD_MMC.remove("/esp.log");
    log_file = SD_MMC.open("/esp.log", FILE_APPEND);
    if (!log_file) {
        ret = false;
        return ret;
    } else {
        esp_log_set_vprintf(log_to_sd_card);
        esp_log_level_set("*", ESP_LOG_INFO);
        ret = true;
    }
    return ret;
}

bool initSD() {
    bool ret = false;
    if (SD_MMC.setPins(SD_CLK, SD_CMD, SD_DATA0, SD_DATA1, SD_DATA2, SD_DATA3)) {
        if (SD_MMC.begin()) {
            ret = true;
            if (SD_MMC.cardType() == CARD_NONE || SD_MMC.cardType() == CARD_UNKNOWN) {
                ESP_LOGW(TAG, "Card type invalid or unknown");
                ret = false;
            }
        }
    } else {
        ESP_LOGW(TAG, "Could not set SD_MMC pins"); // Turn off if setPins didn't work
        return false;
    }
    return ret;
}

/// @brief Performs the update based on a the stream source
/// @param updateSource Streamed update source
/// @param updateSize Total size to update
/// @return True if successful, false otherwise
bool performUpdate(Stream &updateSource, size_t updateSize) {
    bool ret = false;
    if (Update.begin(updateSize)) {
        size_t written = Update.writeStream(updateSource);
        if (written == updateSize) {
            ESP_LOGI(TAG, "Written : %d successfully", written);
        } else {
            ESP_LOGI(TAG, "Written only : %d / %d. Retry?", written, updateSize);
        }
        if (Update.end()) {
            ESP_LOGI(TAG, "Update done!");
            if (Update.isFinished()) {
                ESP_LOGI(TAG, "Update successfully completed. Rebooting.");
                ret = true;
            } else {
                ESP_LOGE(TAG, "Update not finished? Something went wrong!");
            }
        } else {
            ESP_LOGE(TAG, "Error Occurred. Error #: %d", Update.getError());
        }
    } else {
        ESP_LOGE(TAG, "Not enough space to begin OTA");
    }
    return ret;
}

void updateFromFS(fs::FS &fs) {
    bool restartRequired = false;
    if (fs.exists("/update.bin")) {
        File updateBin = fs.open("/update.bin");
        if (updateBin) {
            if (updateBin.isDirectory()) {
                ESP_LOGE(TAG, "Error, update.bin is not a file");
                updateBin.close();
                return;
            }

            size_t updateSize = updateBin.size();

            if (updateSize > 0) {
                ESP_LOGI(TAG, "Attempting update with update.bin");
                restartRequired = performUpdate(updateBin, updateSize);
            } else {
                ESP_LOGE(TAG, "Error, update file is empty");
            }
            updateBin.close();

            fs.remove("/firmware.cur");
            fs.rename("/update.bin", "/firmware.cur");
            if (restartRequired) ESP.restart();
        } else {
            ESP_LOGE(TAG, "Could not load update.bin from SD root");
        }
    } else {
        ESP_LOGI(TAG, "No update.bin file found");
    }
}