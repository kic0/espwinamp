#include <Arduino.h>
#include "pins.h"
#include <SPI.h>
#include <SD.h>
#include <TFT_eSPI.h>

// Create an instance of the TFT_eSPI class
TFT_eSPI tft = TFT_eSPI();

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
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("Card Mount Failed");
    tft.setTextColor(TFT_RED);
    tft.println("SD Card Failed!");
    while (1); // Halt on failure
  }

  tft.setTextColor(TFT_GREEN);
  tft.println("SD Card OK!");
  delay(1000);

  // List files on SD card to serial
  File root = SD.open("/");
  if (root) {
    File file = root.openNextFile();
    while (file) {
      Serial.println(file.name());
      file = root.openNextFile();
    }
    root.close();
  } else {
    Serial.println("Failed to open root directory");
  }
}

void loop() {
  // Nothing to do here for now
}
