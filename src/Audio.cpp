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
        // Handle playback control signals first
        if (context->stop_requested) {
            if (context->is_playing) {
                if (context->audioFile) context->audioFile.close();
                context->decoder.end();
                context->is_playing = false;
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
            }
            if (context->new_song_from_spiffs) {
                context->audioFile = SPIFFS.open(filename_to_play);
            } else {
                context->audioFile = SD.open(filename_to_play);
            }
            if (context->audioFile) {
                context->decoder.begin();
                context->decoder.setDataCallback(pcm_data_callback);
                context->a2dp.set_data_callback_in_frames(get_data_frames);
                esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_START);
                context->is_playing = true;
                xSemaphoreGive(context->audio_task_semaphore); // Start the data pump
            }
        }

        // Wait for a signal from the consumer, with a timeout
        if (xSemaphoreTake(context->audio_task_semaphore, 10 / portTICK_PERIOD_MS) == pdTRUE) {
            if (context->is_playing && context->audioFile && context->audioFile.available()) {
                int bytes_read = context->audioFile.read(context->read_buffer, sizeof(context->read_buffer));
                if (bytes_read > 0) {
                    context->decoder.write(context->read_buffer, bytes_read);
                } else {
                    vTaskDelay(100 / portTICK_PERIOD_MS); // Allow buffer to drain
                    context->is_playing = false; // End of file
                }
            }
        }
    }
}

int32_t get_data_frames(Frame *frame, int32_t frame_count) {
    AppContext* context = g_appContext;
    int32_t frames_to_provide = 0;

    taskENTER_CRITICAL(&context->pcm_buffer_mutex);
    size_t pcm_buffer_capacity = sizeof(context->pcm_buffer) / sizeof(int16_t);
    size_t low_water_mark = pcm_buffer_capacity * 0.25;

    if (context->pcm_buffer_count < low_water_mark) {
        xSemaphoreGive(context->audio_task_semaphore);
    }

    size_t frames_in_buffer = context->pcm_buffer_count / 2;
    frames_to_provide = (frames_in_buffer < frame_count) ? frames_in_buffer : frame_count;
    for (int i = 0; i < frames_to_provide; i++) {
        frame[i].channel1 = context->pcm_buffer[context->pcm_buffer_tail];
        context->pcm_buffer_tail = (context->pcm_buffer_tail + 1) % pcm_buffer_capacity;
        frame[i].channel2 = context->pcm_buffer[context->pcm_buffer_tail];
        context->pcm_buffer_tail = (context->pcm_buffer_tail + 1) % pcm_buffer_capacity;
    }
    context->pcm_buffer_count -= frames_to_provide * 2;
    taskEXIT_CRITICAL(&context->pcm_buffer_mutex);

    return frames_to_provide;
}

void play_file(AppContext& context, String filename, bool from_spiffs, unsigned long seek_position) {
    if (context.is_playing) {
        context.stop_requested = true;
    }
    taskENTER_CRITICAL(&context.pcm_buffer_mutex);
    context.new_song_from_spiffs = from_spiffs;
    strncpy((char*)context.new_song_to_play, filename.c_str(), sizeof(context.new_song_to_play) - 1);
    taskEXIT_CRITICAL(&context.pcm_buffer_mutex);
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
    size_t pcm_buffer_capacity = sizeof(context->pcm_buffer) / sizeof(int16_t);
    for (size_t i = 0; i < len; i++) {
        if (context->pcm_buffer_count < pcm_buffer_capacity) {
            context->pcm_buffer[context->pcm_buffer_head] = pcm_buffer_cb[i];
            context->pcm_buffer_head = (context->pcm_buffer_head + 1) % pcm_buffer_capacity;
            context->pcm_buffer_count++;
        }
    }
    taskEXIT_CRITICAL(&context->pcm_buffer_mutex);
}
