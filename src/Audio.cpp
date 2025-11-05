#include "AppContext.h"

AppContext* appContext_Audio;

void pcm_data_callback(MP3FrameInfo &info, short *pcm_buffer_cb, size_t len, void *ref){
    if (appContext_Audio->pcm_buffer_len + len < sizeof(appContext_Audio->pcm_buffer) / sizeof(int16_t)) {
        memcpy(appContext_Audio->pcm_buffer + appContext_Audio->pcm_buffer_len, pcm_buffer_cb, len * sizeof(int16_t));
        appContext_Audio->pcm_buffer_len += len;
    }
}

int32_t get_data_frames(Frame *frame, int32_t frame_count) {
    if (appContext_Audio->pcm_buffer_len == 0) {
        if (appContext_Audio->audioFile && appContext_Audio->audioFile.available()) {
            int bytes_read = appContext_Audio->audioFile.read(appContext_Audio->read_buffer, sizeof(appContext_Audio->read_buffer));
            if (bytes_read > 0) {
                appContext_Audio->decoder.write(appContext_Audio->read_buffer, bytes_read);
            }
        } else {
            return 0;
        }
    }

    int32_t frames_to_provide = (appContext_Audio->pcm_buffer_len / 2 < frame_count) ? appContext_Audio->pcm_buffer_len / 2 : frame_count;

    if (frames_to_provide > 0) {
        for (int i = 0; i < frames_to_provide; i++) {
            frame[i].channel1 = appContext_Audio->pcm_buffer[i * 2];
            frame[i].channel2 = appContext_Audio->pcm_buffer[i * 2 + 1];
        }

        int samples_consumed = frames_to_provide * 2;
        appContext_Audio->pcm_buffer_len -= samples_consumed;
        if (appContext_Audio->pcm_buffer_len > 0) {
            memmove(appContext_Audio->pcm_buffer, appContext_Audio->pcm_buffer + samples_consumed, appContext_Audio->pcm_buffer_len * sizeof(int16_t));
        }
    }

    return frames_to_provide;
}

int32_t get_wav_data_frames(Frame *frame, int32_t frame_count) {
    return 0; // Simplified for now
}
