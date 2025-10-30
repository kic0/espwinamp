#include <Arduino.h>
#include <SD.h>
#include <BluetoothA2DPSource.h>
#include <MP3DecoderHelix.h>

// ---------- Configuration ----------
const int SD_CS = 5;            // SD card CS pin

// ---------- Globals ----------
BluetoothA2DPSource a2dp;
libhelix::MP3DecoderHelix decoder;
File mp3File;
uint8_t read_buffer[1024];
int16_t pcm_buffer[4096];
int32_t pcm_buffer_len = 0;
int32_t pcm_buffer_offset = 0;

// forward declaration
int32_t get_sound_data(uint8_t *data, int32_t len);
void pcm_data_callback(MP3FrameInfo &info, short *pcm_buffer_cb, size_t len, void *ref);


// ---------- Helper: Find MP3 ----------
String findFirstMP3() {
  File root = SD.open("/");
  if (!root) return String();

  File f = root.openNextFile();
  while (f) {
    if (!f.isDirectory() && strcasecmp(f.name() + strlen(f.name()) - 4, ".MP3") == 0) {
      String name = f.name();
      f.close();
      root.close();
      return name;
    }
    f = root.openNextFile();
  }
  root.close();
  return String();
}


// A2DP callback
int32_t get_sound_data(uint8_t *data, int32_t len) {
    Serial.printf("[A2DP] Requesting %d bytes\n", len);

    // If we have cached PCM data, use it first
    if (pcm_buffer_len > 0) {
        int32_t to_copy = (pcm_buffer_len > len) ? len : pcm_buffer_len;
        memcpy(data, (uint8_t *)pcm_buffer + pcm_buffer_offset, to_copy);
        pcm_buffer_len -= to_copy;
        pcm_buffer_offset += to_copy;
        if (pcm_buffer_len == 0) pcm_buffer_offset = 0;
        Serial.printf("[A2DP] Copied %d bytes from PCM buffer\n", to_copy);
        return to_copy;
    }

    // Otherwise read more MP3 frames and let Helix decode
    int bytes_read = mp3File.read(read_buffer, sizeof(read_buffer));
    Serial.printf("[MP3] Read %d bytes from file\n", bytes_read);
    if (bytes_read <= 0) { // EOF – restart
        mp3File.seek(0);
        bytes_read = mp3File.read(read_buffer, sizeof(read_buffer));
        if (bytes_read <= 0) {
            Serial.println("[MP3] File is empty, stopping.");
            return 0;
        }
    }

    decoder.write(read_buffer, bytes_read);

    // After decoding, we should have PCM data in pcm_buffer
    if (pcm_buffer_len > 0) {
        int32_t to_copy = (pcm_buffer_len > len) ? len : pcm_buffer_len;
        memcpy(data, (uint8_t *)pcm_buffer + pcm_buffer_offset, to_copy);
        pcm_buffer_len -= to_copy;
        pcm_buffer_offset += to_copy;
        if (pcm_buffer_len == 0) pcm_buffer_offset = 0;
        Serial.printf("[A2DP] Copied %d bytes from newly decoded PCM buffer\n", to_copy);
        return to_copy;
    }

    Serial.println("[A2DP] No PCM data available, returning 0.");
    return 0; // nothing ready
}


// pcm data callback
void pcm_data_callback(MP3FrameInfo &info, short *pcm_buffer_cb, size_t len, void *ref){
    Serial.printf("[CALLBACK] PCM data received. Length: %u\n", len);
    memcpy(pcm_buffer, pcm_buffer_cb, len * sizeof(int16_t));
    pcm_buffer_len = len;
    pcm_buffer_offset = 0;
}


// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  Serial.println("\n\n--- RUNNING LATEST DEBUG VERSION ---");

  // 1. SD init
  if (!SD.begin(SD_CS)) {
    Serial.println("[SD] init failed!");
    while (1);
  }
  Serial.println("[SD] ready");

  // 2. Find MP3
  String mp3Name = findFirstMP3();
  if (mp3Name.isEmpty()) {
    Serial.println("[ERROR] No MP3 file found!");
    while (1);
  }
  Serial.print("[MP3] Playing: ");
  Serial.println(mp3Name);

  mp3File = SD.open(mp3Name, FILE_READ);
  if (!mp3File) {
    Serial.println("[ERROR] Can't open MP3");
    while (1);
  }

  // 3. Decoder init
  decoder.setDataCallback(pcm_data_callback);

  // 4. A2DP source
  a2dp.set_data_callback(get_sound_data);
  a2dp.start("ESP32_MP3");
  Serial.println("[BT] A2DP source ready");

}

void loop() {
  // all work is done in the a2dp callback
  delay(1000);
}
