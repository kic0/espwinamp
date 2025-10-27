# ESP32 Winamp MP3 Player

This project is an ESP32-based MP3 player with a Winamp-inspired theme. It plays MP3 files from an SD card and streams the audio to a Bluetooth speaker or headphones.

## Features

-   Plays MP3 files from a microSD card.
-   Streams audio via Bluetooth A2DP to a connected sink (e.g., headphones, speaker).
-   OLED display with a multi-screen user interface.
-   Two-button navigation with short and long press support.
-   Bluetooth device discovery and pairing.
-   Saves the paired Bluetooth device's address for auto-reconnection.
-   Playlist support (playlists are folders on the SD card).
-   Playback controls (play/pause, next, previous).
-   Built with PlatformIO for easy dependency management and building.

## Hardware Requirements

-   ESP32 DEVKIT
-   MicroSD card reader
-   MicroSD card with at least one MP3 file in the root directory
-   128x64 I2C OLED display (SSD1306)
-   Two push buttons for navigation

### Pinout

-   **OLED Display:**
    -   `SDA`: GPIO 16
    -   `SCL`: GPIO 17
-   **SD Card:**
    -   `CS`: GPIO 5
    -   `SCK`: GPIO 18
    -   `MOSI`: GPIO 23
    -   `MISO`: GPIO 19
-   **Buttons:**
    -   `BTN_SCROLL`: GPIO 0
    -   `BTN_SELECT`: GPIO 32

## Software Dependencies

This project relies on the following libraries:

-   `pschatzmann/ESP32-A2DP`: For Bluetooth A2DP source implementation.
-   `pschatzmann/arduino-libhelix`: For MP3 decoding.
-   `Adafruit GFX Library` and `Adafruit SSD1306`: For the OLED display.
-   `SD`: For reading files from the microSD card.

## Building and Flashing

The project includes a `build.sh` script that automates the build process.

### Prerequisites

-   Python and `pip` must be installed.

### Instructions

1.  **Clone the repository:**
    ```bash
    git clone https://github.com/kic0/espwinamp.git
    cd espwinamp
    ```

2.  **Run the build script:**
    ```bash
    ./build.sh
    ```
    This script will automatically install PlatformIO and all the necessary libraries, then compile the project.

3.  **Flash the firmware:**
    To flash the compiled firmware to your ESP32, run the build script with the `--flash` argument:
    ```bash
    ./build.sh --flash
    ```

## How It Works

The application is a state machine with the following states:

1.  **Startup:** The device displays a "Winamp" splash screen and attempts to auto-connect to a previously saved Bluetooth device.
2.  **Bluetooth Discovery:** If no saved device is found or the auto-connect fails, the device enters discovery mode. The user can scan for and select a Bluetooth audio device to connect to.
3.  **Playlist Selection:** After a successful Bluetooth connection, the device scans the SD card for folders, which are treated as playlists. The user can select a playlist to play.
4.  **Player:** The device plays the MP3 files from the selected playlist. The user can control playback with the buttons.

### Button Controls

-   **Scroll Button:**
    -   Short Press: Navigate through menus (BT devices, playlists), or skip to the next track in the player.
    -   Long Press: Skip to the previous track in the player.
-   **Select Button:**
    -   Short Press: Select a menu item, or play/pause the current track in the player.
    -   Long Press: Go back to the playlist selection screen from the player.
