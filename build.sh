#!/bin/bash
# Exit immediately if a command exits with a non-zero status.
set -e

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
