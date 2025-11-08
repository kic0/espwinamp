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
        // Wait indefinitely for a signal to do something
        if (xSemaphoreTake(context->audio_task_semaphore, portMAX_DELAY) == pdTRUE) {
            bool should_play_new_song = false;
            char filename_to_play[256] = "";

            xSemaphoreTake(context->pcm_buffer_mutex, portMAX_DELAY);
            if (context->new_song_to_play[0] != '\0') {
                should_play_new_song = true;
                strncpy(filename_to_play, (const char*)context->new_song_to_play, sizeof(filename_to_play) - 1);
                filename_to_play[sizeof(filename_to_play) - 1] = '\0';
                context->new_song_to_play[0] = '\0';
            }
            bool stop_requested = context->stop_requested;
            context->stop_requested = false;
            xSemaphoreGive(context->pcm_buffer_mutex);

            if (stop_requested || should_play_new_song) {
                if (context->is_playing) {
                    esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_STOP);
                    if (context->audioFile) context->audioFile.close();
                    context->is_playing = false;
                    context->decoder.end();
                    xSemaphoreTake(context->pcm_buffer_mutex, portMAX_DELAY);
                    context->pcm_buffer_head = 0;
                    context->pcm_buffer_tail = 0;
                    context->pcm_buffer_count = 0;
                    xSemaphoreGive(context->pcm_buffer_mutex);
                }
            }

            if (should_play_new_song) {
                File audioFile = context->new_song_from_spiffs ? SPIFFS.open(filename_to_play) : SD.open(filename_to_play);
                if (audioFile) {
                    context->audioFile = audioFile;
                    context->decoder.begin();
                    context->decoder.setDataCallback(pcm_data_callback);
                    context->a2dp.set_data_callback_in_frames(get_data_frames);
                    esp_err_t start_err = esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_START);
                    Log::printf("AudioTask: esp_a2d_media_ctrl(START) returned 0x%x\n", start_err);
                    context->is_playing = true;
                }
            }
        }

        while (context->is_playing) {
            size_t pcm_buffer_capacity = sizeof(context->pcm_buffer) / sizeof(int16_t);
            if (context->pcm_buffer_count >= pcm_buffer_capacity) {
                vTaskDelay(10 / portTICK_PERIOD_MS);
                continue;
            }

            if (context->audioFile && context->audioFile.available()) {
                int bytes_read = context->audioFile.read(context->read_buffer, sizeof(context->read_buffer));
                if (bytes_read > 0) {
                    context->decoder.write(context->read_buffer, bytes_read);
                } else {
                    context->is_playing = false;
                }
            } else {
                context->is_playing = false;
            }
        }
    }
}

int32_t get_data_frames(Frame *frame, int32_t frame_count) {
    AppContext* context = g_appContext;
    int32_t frames_to_provide = 0;

    xSemaphoreTake(context->pcm_buffer_mutex, portMAX_DELAY);
    size_t pcm_buffer_capacity = sizeof(context->pcm_buffer) / sizeof(int16_t);
    size_t frames_in_buffer = context->pcm_buffer_count / 2;
    frames_to_provide = (frames_in_buffer < frame_count) ? frames_in_buffer : frame_count;

    for (int i = 0; i < frames_to_provide; i++) {
        frame[i].channel1 = context->pcm_buffer[context->pcm_buffer_tail];
        context->pcm_buffer_tail = (context->pcm_buffer_tail + 1) % pcm_buffer_capacity;
        frame[i].channel2 = context->pcm_buffer[context->pcm_buffer_tail];
        context->pcm_buffer_tail = (context->pcm_buffer_tail + 1) % pcm_buffer_capacity;
    }
    context->pcm_buffer_count -= frames_to_provide * 2;
    xSemaphoreGive(context->pcm_buffer_mutex);

    return frames_to_provide;
}

void play_file(AppContext& context, String filename, bool from_spiffs, unsigned long seek_position) {
    xSemaphoreTake(context.pcm_buffer_mutex, portMAX_DELAY);
    context.new_song_from_spiffs = from_spiffs;
    strncpy((char*)context.new_song_to_play, filename.c_str(), sizeof(context.new_song_to_play) - 1);
    context.new_song_to_play[sizeof(context.new_song_to_play) - 1] = '\0';
    xSemaphoreGive(context.pcm_buffer_mutex);
    xSemaphoreGive(context.audio_task_semaphore);
}

void stop_audio_playback(AppContext& context) {
    if (context.is_playing) {
        Log::printf("Main loop: Requesting audio stop.\n");
        xSemaphoreTake(context.pcm_buffer_mutex, portMAX_DELAY);
        context.stop_requested = true;
        xSemaphoreGive(context.pcm_buffer_mutex);
        xSemaphoreGive(context.audio_task_semaphore);
    }
}

void pcm_data_callback(MP3FrameInfo &info, short *pcm_buffer_cb, size_t len, void *ref){
    AppContext* context = g_appContext;

    if (context->diag_sample_rate != info.samprate) {
        context->diag_sample_rate = info.samprate;
        context->diag_bits_per_sample = info.bitsPerSample;
        context->diag_channels = info.nChans;
        esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY);
    }

    xSemaphoreTake(context->pcm_buffer_mutex, portMAX_DELAY);
    size_t pcm_buffer_capacity = sizeof(context->pcm_buffer) / sizeof(int16_t);
    for (size_t i = 0; i < len; i++) {
        if (context->pcm_buffer_count < pcm_buffer_capacity) {
            context->pcm_buffer[context->pcm_buffer_head] = pcm_buffer_cb[i];
            context->pcm_buffer_head = (context->pcm_buffer_head + 1) % pcm_buffer_capacity;
            context->pcm_buffer_count++;
        }
    }
    xSemaphoreGive(context->pcm_buffer_mutex);
}
