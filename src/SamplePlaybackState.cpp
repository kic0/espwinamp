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
    if (!playback_started && millis() - start_time >= 200) {
        Log::printf("Playing sample.mp3...\n");
        play_file(context, "/data/sample.mp3", true, 0);
        playback_started = true;
    }

    // The audio task will set is_playing to true once playback starts.
    // Only after it has started and then finished (is_playing becomes false again)
    // should we transition.
    if (playback_started && context.is_playing) {
        // This flag helps us know that playback has successfully started
        // and we should transition only when it stops.
        context.sample_playback_is_active = true;
    }

    if (context.sample_playback_is_active && !context.is_playing) {
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
    context.sample_playback_is_active = false;
}
