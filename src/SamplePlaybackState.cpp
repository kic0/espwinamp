#include "SamplePlaybackState.h"
#include "AppContext.h"
#include "ArtistSelectionState.h"
#include "Log.h"
#include <SPIFFS.h>

// Forward declarations from PlayerState.cpp
void play_file(AppContext& context, String filename, bool from_spiffs, unsigned long seek_position);
void draw_bitmap_from_spiffs(AppContext& context, const char *filename, int16_t x, int16_t y);

void SamplePlaybackState::enter(AppContext& context) {
    Log::printf("Entering Sample Playback State\n");
    start_time = millis();
    context.display.clearDisplay();
    Log::printf("Drawing splash screen...\n");
    draw_bitmap_from_spiffs(context, "/splash.bmp", 10, 0);
    context.display.display();
}

State* SamplePlaybackState::loop(AppContext& context) {
    Log::printf("SamplePlaybackState::loop\n");
    if (millis() - start_time >= 5000 && !context.audioFile) {
        Log::printf("Playing sample.mp3...\n");
        play_file(context, "/sample.mp3", true);
    }

    if (context.audioFile && !context.audioFile.available()) {
        return new ArtistSelectionState();
    }
    return nullptr;
}

void SamplePlaybackState::exit(AppContext& context) {
    if (context.audioFile) {
        context.audioFile.close();
    }
}
