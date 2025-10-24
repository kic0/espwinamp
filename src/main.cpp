#include <Arduino.h>
#include "AudioTools.h"
#include "pins.h"
#include <SPI.h>
#include <SD.h>

// I2S Sine Wave Test (comment out to test SD card)
I2SStream out;
SineWaveGenerator<int16_t> sineWave(32000);
int16_t buffer[1024];

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // --- I2S Sine Wave Test ---
  // auto cfg = out.defaultConfig(TX_MODE);
  // cfg.pin_bck = I2S_BCLK_PIN;
  // cfg.pin_ws = I2S_LRCK_PIN;
  // cfg.pin_data = I2S_DOUT_PIN;
  // cfg.sample_rate = 32000;
  // cfg.channels = 1; // Mono
  // cfg.bits_per_sample = 16;
  // out.begin(cfg);
  // sineWave.begin(cfg, 440); // 440 Hz tone

  // --- SD Card Test ---
  // Using the default SPI bus
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }

  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);

  File root = SD.open("/");
  if (!root) {
    Serial.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(file.name());
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

void loop() {
  // --- I2S Sine Wave Test ---
  // size_t bytes_read = sineWave.readBytes((uint8_t*)buffer, sizeof(buffer));
  // if (bytes_read > 0) {
  //   out.write((uint8_t*)buffer, bytes_read);
  // }
}
