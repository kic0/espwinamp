#include "SamplePlaybackState.h"
#include "AppContext.h"
#include "ArtistSelectionState.h"
#include "Log.h"
#include <SPIFFS.h>

void play_file(AppContext& context, String filename, bool from_spiffs, unsigned long seek_position);
void stop_audio_playback(AppContext& context); // Forward declare the correct stop function

void SamplePlaybackState::enter(AppContext& context) {
    Log::printf("Entering Sample Playback State\n");
    play_file(context, "/data/sample.mp3", true);
    start_time = millis();
}

State* SamplePlaybackState::loop(AppContext& context) {
    // Start playback after a short delay to allow the UI to draw
    // This state has two stages: waiting for playback to start, and waiting for it to finish.
    if (!playback_started) {
        // Stage 1: Wait for playback to start
        if (context.is_playing && context.playback_started) {
            playback_started = true; // Move to stage 2
        }
    } else {
        // Stage 2: Wait for playback to finish
        if (!context.is_playing && !context.playback_started) {
            Log::printf("Sample playback finished.\n");
            return new ArtistSelectionState();
        }
    }

    // Timeout for the whole process
    if (millis() - start_time >= 10000) { // 10-second timeout
        Log::printf("Sample playback timed out. Continuing...\n");
        return new ArtistSelectionState();
    }

    return nullptr;
}

void SamplePlaybackState::exit(AppContext& context) {
    // Correctly stop the audio playback to leave the system in a clean state
    stop_audio_playback(context);
    context.sample_playback_is_active = false;
}
