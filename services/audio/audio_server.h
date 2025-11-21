/**
 * @file audio_server.h
 * @brief Audio Server for Scarlett OS
 *
 * User-space audio server with mixing, per-app volume control, and routing
 */

#ifndef SERVICES_AUDIO_SERVER_H
#define SERVICES_AUDIO_SERVER_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_AUDIO_CLIENTS 64
#define MAX_AUDIO_DEVICES 16
#define MIXER_BUFFER_SIZE 4096

// Audio client
typedef struct {
    uint32_t client_id;
    uint32_t process_id;
    char name[64];
    uint32_t volume;        // 0-100
    bool muted;
    bool active;
    uint32_t stream_id;
} audio_client_t;

// Audio mixer
typedef struct {
    int16_t buffer[MIXER_BUFFER_SIZE * 2];  // Stereo
    uint32_t sample_rate;
    uint32_t channels;
    uint32_t master_volume;  // 0-100
    bool master_muted;
} audio_mixer_t;

// Audio server context
typedef struct {
    audio_client_t clients[MAX_AUDIO_CLIENTS];
    uint32_t client_count;
    
    audio_mixer_t mixer;
    
    // Device management
    void* devices[MAX_AUDIO_DEVICES];
    uint32_t device_count;
    uint32_t active_device;
    
    bool running;
} audio_server_ctx_t;

// Server functions
audio_server_ctx_t* audio_server_create(void);
void audio_server_destroy(audio_server_ctx_t* ctx);
void audio_server_run(audio_server_ctx_t* ctx);

// Client management
uint32_t audio_server_register_client(audio_server_ctx_t* ctx, uint32_t pid, const char* name);
void audio_server_unregister_client(audio_server_ctx_t* ctx, uint32_t client_id);

// Playback
int audio_server_play(audio_server_ctx_t* ctx, uint32_t client_id, const void* data, uint32_t size);
int audio_server_stop(audio_server_ctx_t* ctx, uint32_t client_id);

// Volume control
void audio_server_set_master_volume(audio_server_ctx_t* ctx, uint32_t volume);
void audio_server_set_client_volume(audio_server_ctx_t* ctx, uint32_t client_id, uint32_t volume);
void audio_server_set_master_mute(audio_server_ctx_t* ctx, bool mute);
void audio_server_set_client_mute(audio_server_ctx_t* ctx, uint32_t client_id, bool mute);

// Mixing
void audio_server_mix(audio_server_ctx_t* ctx);

#endif // SERVICES_AUDIO_SERVER_H
