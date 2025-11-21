/**
 * @file libaudio.h
 * @brief Audio Client Library for Scarlett OS Applications
 *
 * Simple API for applications to play and record audio
 */

#ifndef LIBAUDIO_H
#define LIBAUDIO_H

#include <stdint.h>
#include <stdbool.h>

// Audio handle
typedef struct audio_handle audio_handle_t;

// Audio parameters
typedef struct {
    uint32_t sample_rate;    // 8000, 11025, 16000, 22050, 32000, 44100, 48000, etc.
    uint32_t channels;       // 1 = mono, 2 = stereo
    uint32_t bits_per_sample; // 8, 16, 24, 32
} audio_params_t;

// Initialize audio library
int audio_init(void);
void audio_cleanup(void);

// Playback API
audio_handle_t* audio_open_playback(const char* app_name, audio_params_t* params);
void audio_close_playback(audio_handle_t* handle);
int audio_write(audio_handle_t* handle, const void* data, uint32_t size);
int audio_drain(audio_handle_t* handle);  // Wait for playback to complete

// Recording API
audio_handle_t* audio_open_capture(const char* app_name, audio_params_t* params);
void audio_close_capture(audio_handle_t* handle);
int audio_read(audio_handle_t* handle, void* data, uint32_t size);

// Volume control
int audio_set_volume(audio_handle_t* handle, uint32_t volume);  // 0-100
int audio_get_volume(audio_handle_t* handle, uint32_t* volume);
int audio_set_mute(audio_handle_t* handle, bool mute);
int audio_get_mute(audio_handle_t* handle, bool* mute);

// Device enumeration
uint32_t audio_get_device_count(void);
int audio_get_device_name(uint32_t index, char* name, uint32_t size);
int audio_set_device(audio_handle_t* handle, uint32_t device_index);

// Utility functions
const char* audio_get_error_string(int error_code);

// Error codes
#define AUDIO_SUCCESS 0
#define AUDIO_ERROR_INIT -1
#define AUDIO_ERROR_NO_DEVICE -2
#define AUDIO_ERROR_INVALID_PARAMS -3
#define AUDIO_ERROR_BUFFER_FULL -4
#define AUDIO_ERROR_BUFFER_EMPTY -5
#define AUDIO_ERROR_DEVICE_BUSY -6

#endif // LIBAUDIO_H
