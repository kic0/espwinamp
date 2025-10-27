# ESP32 Winamp MP3 Player

This project is an ESP32-based MP3 player with a Winamp-inspired theme. It plays MP3 files from an SD card and streams the audio to a Bluetooth speaker or headphones.

## Features

-   Plays MP3 files from a microSD card.
-   Streams audio via Bluetooth A2DP to a connected sink (e.g., headphones, speaker).
-   Built with PlatformIO for easy dependency management and building.

## Hardware Requirements

-   ESP32 DEVKIT
-   MicroSD card reader
-   MicroSD card with at least one MP3 file in the root directory

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

The ESP32 is configured as a Bluetooth A2DP source. When a Bluetooth sink connects, the device finds the first MP3 file in the root of the microSD card and begins streaming it. The audio is decoded on-the-fly using the `libhelix` library.

The device will appear with the Bluetooth name "ESP32_MP3".
