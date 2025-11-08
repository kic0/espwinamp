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
    // Start playback after a short delay to allow the UI to draw
    if (!playback_attempted && context.is_a2dp_ready) {
        Log::printf("A2DP ready, playing sample.mp3...\n");
        play_file(context, "/data/sample.mp3", true, 0);
        playback_attempted = true;
    }

    // Wait for playback to be confirmed by the A2DP callback
    if (playback_attempted && context.playback_started) {
        playback_confirmed = true;
    }

    // Once playback is confirmed to have started, wait for it to stop
    if (playback_confirmed && !context.playback_started && !context.is_playing) {
        Log::printf("Sample playback finished.\n");
        return new ArtistSelectionState();
    }

    // Timeout if A2DP never becomes ready or playback never starts
    if (millis() - start_time >= 10000) {
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
