# ESP32 Winamp MP3 Player

This project is a Winamp-themed MP3 player for the ESP32 DEVKIT. It uses an SD card for file storage, an OLED display for the UI, and physical buttons for control. The device acts as a Bluetooth A2DP source, streaming audio to a connected speaker or headphones.

## Features

- **Bluetooth A2DP Source:** Streams MP3 audio to any A2DP-compatible speaker or headphones.
- **SD Card Support:** Playlists are defined as folders in the root directory of the SD card.
- **OLED Display Interface:** A 128x64 SSD1306 OLED screen displays a Winamp-themed user interface.
- **Single-Button Control:** All user input is handled by the single 'BOOT' button (GPIO 0), which supports short and long presses.
- **State Machine Logic:** The application is built around a robust state machine that handles Bluetooth discovery, connection, and multiple playback states.
- **Interactive "Now Playing" Screen:** While a song is playing, you can scroll through the current playlist and select a new song to play.
- **Auto-Connect:** The device saves the MAC address of the last connected speaker and will attempt to auto-reconnect on the next boot.
- **PlatformIO Build System:** The project is built using PlatformIO, which automatically manages all dependencies.

## Hardware Requirements

| Component           | Connection                  |
| ------------------- | --------------------------- |
| **ESP32 DEVKIT**    | -                           |
| **OLED Display**    | SDA: GPIO 16, SCL: GPIO 17  |
| **SD Card Reader**  | CS: GPIO 5, SCK: GPIO 18, MOSI: GPIO 23, MISO: GPIO 19 |
| **'BOOT' Button**   | GPIO 0 (built-in)           |

## Software Dependencies

All required libraries are managed automatically by PlatformIO. The project uses the following major libraries:

- `adafruit/Adafruit GFX Library`
- `adafruit/Adafruit SSD1306`
- `pschatzmann/ESP32-A2DP`
- `pschatzmann/arduino-libhelix`

## Setup and Installation

1.  **Prepare the SD Card:**
    *   Format an SD card as FAT32.
    *   Create folders in the root directory of the SD card. Each folder will be treated as a playlist.
    *   Copy your `.mp3` files into the playlist folders.

2.  **Clone the Repository:**
    ```bash
    git clone https://github.com/kic0/espwinamp/
    cd espwinamp
    ```

3.  **Install PlatformIO:** If you don't have PlatformIO installed, the build script will attempt to install it for you. It is recommended to have Python and `pip` available on your system.

## Building and Flashing

The project includes a convenient build script (`build.sh`) that handles compiling, flashing, and other common tasks.

### Compile the Code

To compile the project and ensure everything is set up correctly, run:
```bash
./build.sh
```

### Flash the Device

To compile and upload the firmware to your ESP32, connect the device and run:
```bash
./build.sh --flash
```

### Upload the Filesystem Image

To upload the `sample.mp3` file to the ESP32's internal flash, you must also flash the SPIFFS (SPI Flash File System) image.

```bash
./build.sh --uploadfs
```


### Erase and Flash

If you are flashing for the first time or have changed the partition table, you should erase the flash memory before uploading the new firmware.

**Warning:** This will erase all data on the ESP32's flash, but not the SD card.

```bash
./build.sh --erase-flash --flash
```
You will be prompted to power-cycle the device during the erase process. Follow the on-screen instructions.



### TODO

1. 3D printed case
2. Add a batery 
3. Add moar buttons for better UI/control
4. Wifi AP for lazy management of mp3 files in the SDcard
5. Add a minijack for wired headphones/speakers with a MAX98357A (not sure we have enough power for this tho)



### Disclamer 

This software was developed with the help of AI LLM models, no warranty or support is to be expected.

If you feel like donating:

 XMR: 46hHveE5bAQHv6AE9bbYSJMsJupA2je1BFBUYLmMp899i2yMC8MEn35QiUEZiS2Scd5QGKFrcDk5yZCxL91Rtm9FAz44MMA 
 
 BTC: bc1qu8ykfkqp3t8l7k37fanznh8wj5wuewdmvwfrs2 

