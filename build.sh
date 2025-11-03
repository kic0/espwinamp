#!/bin/bash
# Exit immediately if a command exits with a non-zero status.
set -e

# --- Parse Arguments ---
SHOULD_FLASH=false
SHOULD_UPLOAD_FS=false
SHOULD_ERASE=false

for arg in "$@"
do
    case $arg in
        --flash)
        SHOULD_FLASH=true
        shift
        ;;
        --upload-fs)
        SHOULD_UPLOAD_FS=true
        shift
        ;;
        --erase-flash)
        SHOULD_ERASE=true
        shift
        ;;
    esac
done

# --- Main Script ---

# Check if PlatformIO is installed, and install it if not.
if ! command -v pio &> /dev/null
then
    echo "PlatformIO could not be found, installing..."
    pip install platformio
fi

# Clean the project
echo "Cleaning project..."
pio run --target clean --environment esp32dev

# Install dependencies and compile the project
echo "Installing dependencies and compiling..."
pio run --environment esp32dev
cp .pio/build/esp32dev/firmware.bin .

# Erase flash if requested
if [ "$SHOULD_ERASE" = true ]; then
    echo "Erasing the flash..."
    pio run --target erase --environment esp32dev
fi

# Build and upload filesystem if requested
if [ "$SHOULD_UPLOAD_FS" = true ]; then
    echo "Building and uploading the SPIFFS filesystem image..."
    pio run --target buildfs --environment esp32dev
    pio run --target uploadfs --environment esp32dev
fi

# If --flash is passed, upload the firmware
if [ "$SHOULD_FLASH" = true ]; then
    echo "Flashing the firmware..."
    pio run --target upload --environment esp32dev
fi
