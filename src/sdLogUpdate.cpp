#include <sdLogUpdate.h>

static const char* TAG = "sdLogUpdate";

bool initSD() {
	bool ret = false;
	pinMode(LED_SD,OUTPUT);
	pinMode(SD_DETECT,INPUT);
	if(SD_MMC.setPins(SD_CLK,SD_CMD,SD_DATA0,SD_DATA1,SD_DATA2,SD_DATA3)) {
		digitalWrite(LED_SD,LOW);
		ret = true;
	}
	else digitalWrite(LED_SD,HIGH); //Turn of if setPins didn't work

	if(!digitalRead(SD_DETECT)) { //Check the SD Detect pin. Not sure about the logic
		digitalWrite(LED_SD,LOW);
		//ret = true;
	}
	else digitalWrite(LED_SD,HIGH); //Turn off if no SD card detected

	if(SD_MMC.begin()) {
		ret = true;
		digitalWrite(LED_SD,LOW);
		if(SD_MMC.cardType() == CARD_NONE || SD_MMC.cardType() == CARD_UNKNOWN) {
			ret = false;
			digitalWrite(LED_SD,HIGH);
		}
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

void performUpdate(Stream &updateSource, size_t updateSize) {
  if (Update.begin(updateSize)) {
	size_t written = Update.writeStream(updateSource);
	if (written == updateSize) {
		ESP_LOGI(TAG,"yes");
	//   Serial.println("Written : " + String(written) + " successfully");
	} else {
	//   Serial.println("Written only : " + String(written) + "/" + String(updateSize) + ". Retry?");
	}
	if (Update.end()) {
	//   Serial.println("OTA done!");
	  if (Update.isFinished()) {
		// Serial.println("Update successfully completed. Rebooting.");
	  } else {
		// Serial.println("Update not finished? Something went wrong!");
	  }
	} else {
	//   Serial.println("Error Occurred. Error #: " + String(Update.getError()));
	}

  } else {
	// Serial.println("Not enough space to begin OTA");
  }
}

void updateFromFS(fs::FS &fs) {
  File updateBin = fs.open("/update.bin");
  if (updateBin) {
	if (updateBin.isDirectory()) {
	//   Serial.println("Error, update.bin is not a file");
	  updateBin.close();
	  return;
	}

	size_t updateSize = updateBin.size();

	if (updateSize > 0) {
	//   Serial.println("Try to start update");
	  performUpdate(updateBin, updateSize);
	} else {
	//   Serial.println("Error, file is empty");
	}

	updateBin.close();

	// when finished remove the binary from sd card to indicate end of the process
	fs.remove("/update.bin");
  } else {
	// Serial.println("Could not load update.bin from sd root");
  }
}