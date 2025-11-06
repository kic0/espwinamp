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
    context.ui_dirty = true;
}

State* SamplePlaybackState::loop(AppContext& context) {
    // Play sample after 5 seconds
    if (millis() - start_time >= 5000 && !context.audioFile) {
        Log::printf("Playing sample.mp3...\n");
        play_file(context, "/sample.mp3", true, 0);
    }

    // Transition after audio is finished
    if (context.audioFile && !context.audioFile.available()) {
        Log::printf("Sample playback finished.\n");
        return new ArtistSelectionState();
    }

    // Also transition after a 20-second timeout to prevent getting stuck
    if (millis() - start_time >= 20000) {
        Log::printf("Sample playback timed out. Continuing...\n");
        return new ArtistSelectionState();
    }

    return nullptr;
}

void SamplePlaybackState::exit(AppContext& context) {
    if (context.audioFile) {
        context.audioFile.close();
    }
}
