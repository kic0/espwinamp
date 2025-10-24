#include <Arduino.h>
#include "pins.h"
#include <SPI.h>
#include <SdFat.h>
#include <TFT_eSPI.h>

// Create an instance of the TFT_eSPI class
TFT_eSPI tft = TFT_eSPI();

// SdFat objects
SdFat sd;
SdFile root;

void setup() {
  Serial.begin(115200);

  // --- Display Initialization ---
  tft.init();
  tft.setRotation(1); // 1 for landscape
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("Initializing Display...");
  delay(1000); // Show the message for a moment

  // --- SD Card Test ---
  tft.println("Initializing SD Card...");
  if (!sd.begin(SD_CS_PIN)) {
    Serial.println("Card Mount Failed");
    sd.initErrorHalt(&Serial);
    tft.setTextColor(TFT_RED);
    tft.println("SD Card Failed!");
    while (1); // Halt on failure
  }

  tft.setTextColor(TFT_GREEN);
  tft.println("SD Card OK!");
  delay(1000);

  // List files on SD card to serial
  if (root.open("/")) {
    SdFile file;
    while (file.openNext(&root, O_RDONLY)) {
      char fileName[256];
      file.getName(fileName, sizeof(fileName));
      Serial.println(fileName);
      file.close();
    }
    root.close();
  } else {
    Serial.println("Failed to open root directory");
  }
}

void loop() {
  // Nothing to do here for now
}
