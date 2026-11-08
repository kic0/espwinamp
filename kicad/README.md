# KiCad Project: ESP32 Winamp Carrier Board

This directory contains the full **KiCad** (v6/v7/v8 format) project files for the **ESP32 Winamp MP3 Player**.

---

## Files in this Directory

- **`espwinamp.kicad_pro`**: KiCad Project configuration and design rule settings.
- **`espwinamp.kicad_sch`**: Complete electronic schematic diagram with all components, netlabels, and pin connections.
- **`espwinamp.kicad_pcb`**: 2-layer carrier PCB layout (85mm x 60mm) connecting the ESP32 DEVKIT, SSD1306 OLED, SPI MicroSD card module, and B103 Potentiometer with routed traces.

---

## Schematic Design Overview

| Component Reference | Value / Part | Footprint / Package | KiCad Net Connections |
| :--- | :--- | :--- | :--- |
| **`U1`** | `ESP32-DEVKIT-V1-30PIN` | 2x 1x15 2.54mm Pin Sockets | Pin 1 (+3V3), Pin 6 (POT_VOL), Pin 15 (GND), Pin 28 (SD_MOSI), Pin 23 (SD_MISO), Pin 22 (SD_SCK), Pin 21 (SD_CS), Pin 20 (OLED_SCL), Pin 19 (OLED_SDA), Pin 30 (VIN), Pin 29 (GND) |
| **`DISP1`** | `SSD1306_OLED_128x64` | 1x04 2.54mm Pin Header | Pin 1 (GND), Pin 2 (+3V3), Pin 3 (OLED_SCL), Pin 4 (OLED_SDA) |
| **`J1`** | `MicroSD_Module_SPI` | 1x06 2.54mm Pin Header | Pin 1 (SD_CS), Pin 2 (SD_SCK), Pin 3 (SD_MOSI), Pin 4 (SD_MISO), Pin 5 (+3V3), Pin 6 (GND) |
| **`RV1`** | `B103_10k` | Potentiometer Bourns 3386P / Rotary | Pin 1 (GND), Pin 2 (POT_VOL / GPIO 35), Pin 3 (+3V3) |

---

## PCB Specifications

- **Board Dimensions**: 85.0 mm x 60.0 mm
- **Layers**: 2 Copper Layers (Top: `F.Cu`, Bottom: `B.Cu`)
- **Board Thickness**: 1.6 mm FR4
- **Signal Traces**: 0.35 mm width
- **Power Traces (+3V3, GND)**: 0.50 mm – 0.60 mm width
- **Hole Pitch**: Standard 2.54 mm (0.1") through-hole headers for standard jumper wires or female header socket mounting.

---

## How to Open in KiCad

1. Install [KiCad](https://www.kicad.org/) (version 7 or 8 recommended, version 6 compatible).
2. Launch KiCad and select **File -> Open Project**.
3. Open `kicad/espwinamp.kicad_pro`.
4. Double click on `espwinamp.kicad_sch` to view or edit the Schematic Editor (Eeschema).
5. Double click on `espwinamp.kicad_pcb` to view or edit the PCB Editor (Pcbnew) and 3D Viewer (**View -> 3D Viewer**).
