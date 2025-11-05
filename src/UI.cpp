#include "AppContext.h"
#include <SPIFFS.h>

void calculate_scroll_offset(int &selected_item, int item_count, int &scroll_offset, int center_offset) {
    if (selected_item >= item_count) {
        selected_item = 0;
    } else if (selected_item < 0) {
        selected_item = item_count - 1;
    }

    if (item_count <= 4) {
        scroll_offset = 0;
        return;
    }

    if (selected_item < scroll_offset + center_offset) {
        scroll_offset = selected_item - center_offset;
        if (scroll_offset < 0) {
            scroll_offset = 0;
        }
    } else if (selected_item >= scroll_offset + 4 - center_offset) {
        scroll_offset = selected_item - (3 - center_offset);
        if (scroll_offset > item_count - 4) {
            scroll_offset = item_count - 4;
        }
    }
}

uint16_t read16(File &f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read(); // MSB
  return result;
}

uint32_t read32(File &f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); // MSB
  return result;
}

void draw_bitmap_from_spiffs(AppContext& context, const char *filename, int16_t x, int16_t y) {
  File bmpFile;
  int bmpWidth, bmpHeight;
  uint8_t bmpDepth;
  uint32_t bmpImageoffset;
  uint8_t sdbuffer[3 * 128];

  if ((x >= context.display.width()) || (y >= context.display.height()))
    return;

  bmpFile = SPIFFS.open(filename, "r");
  if (!bmpFile) {
    Serial.print("File not found: ");
    Serial.println(filename);
    return;
  }

  if (read16(bmpFile) != 0x4D42) {
    Serial.println("Invalid BMP signature");
    return;
  }

  read32(bmpFile);
  read32(bmpFile);
  bmpImageoffset = read32(bmpFile);
  read32(bmpFile);
  bmpWidth = read32(bmpFile);
  bmpHeight = read32(bmpFile);

  if (read16(bmpFile) != 1) {
    Serial.println("Unsupported BMP format (planes)");
    return;
  }

  bmpDepth = read16(bmpFile);
  if ((bmpDepth != 24) || (read32(bmpFile) != 0)) {
    Serial.println("Unsupported BMP format (depth or compression)");
    return;
  }

  uint32_t rowSize = (bmpWidth * 3 + 3) & ~3;

  bool flip = true;
  if (bmpHeight < 0) {
    bmpHeight = -bmpHeight;
    flip = false;
  }

  int w = bmpWidth;
  int h = bmpHeight;
  if ((x + w - 1) >= context.display.width())
    w = context.display.width() - x;
  if ((y + h - 1) >= context.display.height())
    h = context.display.height() - y;

  for (int j = 0; j < h; j++) {
    int row = flip ? bmpHeight - 1 - j : j;
    bmpFile.seek(bmpImageoffset + row * rowSize);
    bmpFile.read(sdbuffer, sizeof(sdbuffer));
    for (int i = 0; i < w; i++) {
      uint8_t r = sdbuffer[i * 3 + 2];
      uint8_t g = sdbuffer[i * 3 + 1];
      uint8_t b = sdbuffer[i * 3];
      if ((r + g + b) > 128 * 3) {
        context.display.drawPixel(x + i, y + j, SSD1306_WHITE);
      }
    }
  }
  bmpFile.close();
}
