#pragma once
				#ifndef LED_SD
					#define LED_SD 19
				#else
					#undef LED_SD
					#define LED_SD 19
				#endif
#include <Arduino.h>
#include <Update.h>
#include <FS.h>
#include <SD_MMC.h>
#define SD_DETECT 	34
#define SD_CLK		14
#define SD_DATA2	12
#define SD_DATA3	13
#define SD_CMD		15
#define SD_DATA0	2
#define SD_DATA1	4

#ifdef TAG
#undef TAG
#endif

// /// @brief Cyclically flush the FILE buffer to the actual file
// /// @param  None
// void sdcard_flush_cyclic(void);

/// @brief Starts the logger instance
/// @return Returns true if it is all successfully started, false otherwise
bool initSDLogger();

/// @brief Starts the 4 wire SD_MMC and checks the card type is valid
/// @return True if successful init, false otherwise
bool initSD();


/// @brief Looks for a "/update.bin" file on the SD card and updates the system automatically
/// @param fs SD_MMC or SD if it has been successfully mounted
void updateFromFS(fs::FS &fs);
