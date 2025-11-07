#include "AppContext.h"
#include <SPIFFS.h>
#include "Log.h"
#include <esp_a2dp_api.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern AppContext* g_appContext;

void pcm_data_callback(MP3FrameInfo &info, short *pcm_buffer_cb, size_t len, void *ref);
int32_t get_data_frames(Frame *frame, int32_t frame_count);

void audioTask(void* parameter) {
    AppContext* context = g_appContext;
    while (true) {
        if (context->stop_requested) {
            if (context->is_playing) {
                Log::printf("Audio task: Stop requested.\n");
                esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_STOP);
                if (context->audioFile) {
                    context->audioFile.close();
                }
                context->decoder.end();
                taskENTER_CRITICAL(&context->pcm_buffer_mutex);
                context->pcm_buffer_len = 0;
                taskEXIT_CRITICAL(&context->pcm_buffer_mutex);
                context->is_playing = false;
                Log::printf("Audio task: Playback stopped and cleaned up.\n");
            }
            context->stop_requested = false;
        }

        char filename_to_play[256] = "";
        taskENTER_CRITICAL(&context->pcm_buffer_mutex);
        if (context->new_song_to_play[0] != '\0') {
            strncpy(filename_to_play, (const char*)context->new_song_to_play, sizeof(filename_to_play) - 1);
            filename_to_play[sizeof(filename_to_play) - 1] = '\0';
            context->new_song_to_play[0] = '\0';
        }
        taskEXIT_CRITICAL(&context->pcm_buffer_mutex);

        if (strlen(filename_to_play) > 0) {
            if (context->is_playing) {
                if (context->audioFile) context->audioFile.close();
                context->decoder.end();
                taskENTER_CRITICAL(&context->pcm_buffer_mutex);
                context->pcm_buffer_len = 0;
                taskEXIT_CRITICAL(&context->pcm_buffer_mutex);
            }

            context->audioFile = SD.open(filename_to_play);
            if (!context->audioFile) {
                Log::printf("Audio task: Failed to open file: %s\n", filename_to_play);
                context->is_playing = false;
            } else {
                Log::printf("Audio task: Starting playback for %s\n", filename_to_play);
                taskENTER_CRITICAL(&context->pcm_buffer_mutex);
                context->pcm_buffer_len = 0;
                taskEXIT_CRITICAL(&context->pcm_buffer_mutex);
                context->decoder.begin();
                context->decoder.setDataCallback(pcm_data_callback);
                context->a2dp.set_data_callback_in_frames(get_data_frames);
                esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_START);
                context->is_playing = true;
            }
        }

        if (context->is_playing && context->audioFile && context->audioFile.available()) {
            if (context->pcm_buffer_len < (sizeof(context->pcm_buffer) / 2)) {
                int bytes_read = context->audioFile.read(context->read_buffer, sizeof(context->read_buffer));
                if (bytes_read > 0) {
                    context->decoder.write(context->read_buffer, bytes_read);
                }
            }
        } else if (context->is_playing) {
            Log::printf("Audio task: End of file.\n");
            context->stop_requested = true;
        }

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

int32_t get_data_frames(Frame *frame, int32_t frame_count) {
    AppContext* context = g_appContext;
    int32_t frames_to_provide = 0;

    taskENTER_CRITICAL(&context->pcm_buffer_mutex);
    if (context->pcm_buffer_len > 0) {
        frames_to_provide = (context->pcm_buffer_len / 2 < frame_count) ? context->pcm_buffer_len / 2 : frame_count;
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
    taskEXIT_CRITICAL(&context->pcm_buffer_mutex);

    return frames_to_provide;
}

void play_file(AppContext& context, String filename, bool from_spiffs, unsigned long seek_position) {
    if (context.is_playing) {
        context.stop_requested = true;
    }
    strncpy((char*)context.new_song_to_play, filename.c_str(), sizeof(context.new_song_to_play) - 1);
}

void stop_audio_playback(AppContext& context) {
    if (context.is_playing) {
        Log::printf("Main loop: Requesting audio stop.\n");
        context.stop_requested = true;
    }
}

void pcm_data_callback(MP3FrameInfo &info, short *pcm_buffer_cb, size_t len, void *ref){
    AppContext* context = g_appContext;
    taskENTER_CRITICAL(&context->pcm_buffer_mutex);
    if (context->pcm_buffer_len + len < sizeof(context->pcm_buffer) / sizeof(int16_t)) {
        memcpy(context->pcm_buffer + context->pcm_buffer_len, pcm_buffer_cb, len * sizeof(int16_t));
        context->pcm_buffer_len += len;
    } else {
        Log::printf("PCM buffer overflow!\n");
    }
    taskEXIT_CRITICAL(&context->pcm_buffer_mutex);
}
