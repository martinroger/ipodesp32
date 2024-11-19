#include <sdLogUpdate.h>

static const char* TAG = "SD MMC";

static FILE *log_file;


int log_to_sd_card(const char* fmt, va_list args) {
	static bool static_fatal_error = false;
	static const uint32_t WRITE_CACHE_CYCLE = 1;
	static uint32_t counter_write = 0;
	int iresult;

	// #1 Write to file
	if (log_file == NULL) 
	{
		printf("%s() ABORT. file handle log_file is NULL\n", __FUNCTION__);
		return -1;
	}
	if (static_fatal_error == false) 
	{
		iresult = vfprintf(log_file, fmt, args);
		if (iresult < 0) 
		{
			printf("%s() ABORT. failed vfprintf() -> logging disabled \n",__FUNCTION__);
		// MARK FATAL
		static_fatal_error = true;
		return iresult;
		}

		// #2 Smart commit after x writes
		if (counter_write++ % WRITE_CACHE_CYCLE == 0) fsync(fileno(log_file));
	}
	// #3 ALWAYS Write to stdout!
	//return vprintf(fmt, args);
	return iresult;
}

void sdcard_flush_cyclic(void) {
	if (log_file)	fsync(fileno(log_file));
}

bool initSDLogger() {
	bool ret = false;
	ESP_LOGI(TAG,"Opening/creating stdout.log with append.");
	log_file = fopen("/sdcard/stdout.log",FILE_APPEND);
	if(log_file == NULL)
	{
		ESP_LOGE(TAG,"Failed to open stdout.log, aborting SD card logging");
	}
	else
	{
		ESP_LOGI(TAG,"Redirecting stdout to /stdout.log");
		esp_log_set_vprintf(&log_to_sd_card);
		fprintf(log_file,"---NEW LOG---");
		fsync(fileno(log_file));
		esp_log_level_set("*",ESP_LOG_DEBUG);
		ESP_LOGI(TAG,"SD MMC Logger successfully started");
		ret = true;
	}
	return ret;
}

bool initSD() {
	bool ret = false;
	if(SD_MMC.setPins(SD_CLK,SD_CMD,SD_DATA0,SD_DATA1,SD_DATA2,SD_DATA3)) {
		if(SD_MMC.begin()) {
			ret = true;
			if(SD_MMC.cardType() == CARD_NONE || SD_MMC.cardType() == CARD_UNKNOWN) {
				ESP_LOGW(TAG,"Card type invalid or unknown");
				ret = false;
			}
		}
	}
	else {
		ESP_LOGW(TAG,"Could not set SD_MMC pins"); //Turn of if setPins didn't work
		return false;
	}
	return ret;
}

// void listDir(fs::FS &fs, const char *dirname, uint8_t levels) {
//   Serial.printf("Listing directory: %s\n", dirname);
//   File root = fs.open(dirname);
//   if (!root) {
// 	Serial.println("Failed to open directory");
// 	return;
//   }
//   if (!root.isDirectory()) {
// 	Serial.println("Not a directory");
// 	return;
//   }
//   File file = root.openNextFile();
//   while (file) {
// 	if (file.isDirectory()) {
// 	  Serial.print("  DIR : ");
// 	  Serial.println(file.name());
// 	  if (levels) {
// 		listDir(fs, file.path(), levels - 1);
// 	  }
// 	} else {
// 	  Serial.print("  FILE: ");
// 	  Serial.print(file.name());
// 	  Serial.print("  SIZE: ");
// 	  Serial.println(file.size());
// 	}
// 	file = root.openNextFile();
//   }
// }

// void createDir(fs::FS &fs, const char *path) {
//   Serial.printf("Creating Dir: %s\n", path);
//   if (fs.mkdir(path)) {
// 	Serial.println("Dir created");
//   } else {
// 	Serial.println("mkdir failed");
//   }
// }

// void removeDir(fs::FS &fs, const char *path) {
//   Serial.printf("Removing Dir: %s\n", path);
//   if (fs.rmdir(path)) {
// 	Serial.println("Dir removed");
//   } else {
// 	Serial.println("rmdir failed");
//   }
// }

// void readFile(fs::FS &fs, const char *path) {
// 	 Serial.printf("Reading file: %s\n", path);
//   File file = fs.open(path);
//   if (!file) {
// 	Serial.println("Failed to open file for reading");
// 	return;
//   }
//   Serial.print("Read from file: ");
//   while (file.available()) {
// 	Serial.write(file.read());
//   }
// }

// void writeFile(fs::FS &fs, const char *path, const char *message) {
//   Serial.printf("Writing file: %s\n", path);
//   File file = fs.open(path, FILE_WRITE);
//   if (!file) {
// 	Serial.println("Failed to open file for writing");
// 	return;
//   }
//   if (file.print(message)) {
// 	Serial.println("File written");
//   } else {
// 	Serial.println("Write failed");
//   }
// }

void appendFile(fs::FS &fs, const char *path, const char *message) {
  //Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if (!file) {
	//Serial.println("Failed to open file for appending");
	return;
  }
  if (file.print(message)) {
	//Serial.println("Message appended");
  } else {
	//Serial.println("Append failed");
  }
}

void renameFile(fs::FS &fs, const char *path1, const char *path2) {
//   Serial.printf("Renaming file %s to %s\n", path1, path2);
  if (fs.rename(path1, path2)) {
	// Serial.println("File renamed");
  } else {
	// Serial.println("Rename failed");
  }
}

void deleteFile(fs::FS &fs, const char *path) {
//   Serial.printf("Deleting file: %s\n", path);
  if (fs.remove(path)) {
	// Serial.println("File deleted");
  } else {
	// Serial.println("Delete failed");
  }
}

/// @brief Performs the update based on a the stream source
/// @param updateSource Streamed update source
/// @param updateSize Total size to update
/// @return True if successful, false otherwise
bool performUpdate(Stream &updateSource, size_t updateSize) {
	bool ret = false;
	if (Update.begin(updateSize)) 
	{
		size_t written = Update.writeStream(updateSource);
		if (written == updateSize) 
		{
			ESP_LOGI(TAG,"Written : %d successfully",written);
		} 
		else 
		{
			ESP_LOGI(TAG,"Written only : %d / %d. Retry?",written,updateSize);
		}
		if (Update.end()) 
		{
			ESP_LOGI(TAG,"Update done!");
			if (Update.isFinished()) 
			{
				ESP_LOGI(TAG,"Update successfully completed. Rebooting.");
				ret = true;
			} 
			else ESP_LOGE(TAG,"Update not finished? Something went wrong!");
		} 
		else ESP_LOGE(TAG,"Error Occurred. Error #: %d",Update.getError());
	} 
	else ESP_LOGE(TAG,"Not enough space to begin OTA");
	return ret;
}

void updateFromFS(fs::FS &fs) {
	bool restartRequired = false;
	if(fs.exists("/update.bin")) {
		File updateBin = fs.open("/update.bin");
		if (updateBin) 
		{
			if (updateBin.isDirectory()) 
			{
				ESP_LOGE(TAG,"Error, update.bin is not a file");
				updateBin.close();
				return;
			}

			size_t updateSize = updateBin.size();

			if (updateSize > 0) 
			{
				ESP_LOGI(TAG,"Attempting update with update.bin");
				restartRequired = performUpdate(updateBin, updateSize);
			} 
			else 
			{
				ESP_LOGE(TAG,"Error, update file is empty");
			}
			updateBin.close();

			fs.remove("/update.bin");
			if(restartRequired) ESP.restart();
		} 
		else 
		{
			ESP_LOGE(TAG,"Could not load update.bin from SD root");
		}
	}
	else
	{
		ESP_LOGI(TAG,"No update.bin file found");
	}
}