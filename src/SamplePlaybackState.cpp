#include "SamplePlaybackState.h"
#include "AppContext.h"
#include "ArtistSelectionState.h"
#include "Log.h"
#include <SPIFFS.h>

void play_file(AppContext& context, String filename, bool from_spiffs, unsigned long seek_position);
void stop_audio_playback(AppContext& context); // Forward declare the correct stop function

void SamplePlaybackState::enter(AppContext& context) {
    Log::printf("Entering Sample Playback State\n");
    start_time = millis();
    context.ui_dirty = true;
}

State* SamplePlaybackState::loop(AppContext& context) {
    if (millis() - start_time >= 5000 && !context.audioFile) {
        Log::printf("Playing sample.mp3...\n");
        play_file(context, "/sample.mp3", true, 0);
    }

    if (context.audioFile && !context.audioFile.available()) {
        Log::printf("Sample playback finished.\n");
        return new ArtistSelectionState();
    }

    if (millis() - start_time >= 20000) {
        Log::printf("Sample playback timed out. Continuing...\n");
        return new ArtistSelectionState();
    }

    return nullptr;
}

void SamplePlaybackState::exit(AppContext& context) {
    // Correctly stop the audio playback to leave the system in a clean state
    stop_audio_playback(context);
}
