#include "AppContext.h"
#include <SPIFFS.h>
#include "Log.h"

extern AppContext* g_appContext;

void pcm_data_callback(MP3FrameInfo &info, short *pcm_buffer_cb, size_t len, void *ref);

int32_t get_data_frames(Frame *frame, int32_t frame_count) {
    AppContext* context = g_appContext;
    if (context->pcm_buffer_len == 0) {
        if (context->audioFile && context->audioFile.available()) {
            int bytes_read = context->audioFile.read(context->read_buffer, sizeof(context->read_buffer));
            if (bytes_read > 0) {
                context->decoder.write(context->read_buffer, bytes_read);
            }
        } else {
            return 0;
        }
    }

    int32_t frames_to_provide = (context->pcm_buffer_len / 2 < frame_count) ? context->pcm_buffer_len / 2 : frame_count;

    if (frames_to_provide > 0) {
        for (int i = 0; i < frames_to_provide; i++) {
            frame[i].channel1 = context->pcm_buffer[i * 2];
            frame[i].channel2 = context->pcm_buffer[i * 2 + 1];
        }

        int samples_consumed = frames_to_provide * 2;
        context->pcm_buffer_len -= samples_consumed;
        if (context->pcm_buffer_len > 0) {
            memmove(context->pcm_buffer, context->pcm_buffer + samples_consumed, context->pcm_buffer_len * sizeof(int16_t));
        }
    }

    return frames_to_provide;
}

int32_t get_wav_data_frames(Frame *frame, int32_t frame_count) {
    return 0; // Simplified for now
}

void play_file(AppContext& context, String filename, bool from_spiffs, unsigned long seek_position) {
    if (context.audioFile) {
        context.audioFile.close();
    }

    if (from_spiffs) {
        context.audioFile = SPIFFS.open(filename);
    } else {
        context.audioFile = SD.open(filename);
    }

    if (!context.audioFile) {
        Log::printf("Failed to open file: %s\n", filename.c_str());
        return;
    }

    context.pcm_buffer_len = 0;
    context.decoder.begin();
    context.decoder.setDataCallback(pcm_data_callback);
    context.a2dp.set_data_callback_in_frames(get_data_frames);
}



void pcm_data_callback(MP3FrameInfo &info, short *pcm_buffer_cb, size_t len, void *ref){
    AppContext* context = g_appContext;

    // Update diagnostic info
    context->diag_sample_rate = info.samprate;
    context->diag_bits_per_sample = info.bitsPerSample;
    context->diag_channels = info.nChans;

    // Append new PCM data to the buffer
    if (context->pcm_buffer_len + len < sizeof(context->pcm_buffer) / sizeof(int16_t)) {
        memcpy(context->pcm_buffer + context->pcm_buffer_len, pcm_buffer_cb, len * sizeof(int16_t));
        context->pcm_buffer_len += len;
    } else {
        Log::printf("PCM buffer overflow!\n");
    }
}
