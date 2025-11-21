/**
 * @file audio_framework.h
 * @brief Audio Driver Framework for Scarlett OS
 *
 * Provides common interface for all audio drivers (HDA, AC97, USB Audio)
 */

#ifndef DRIVERS_AUDIO_FRAMEWORK_H
#define DRIVERS_AUDIO_FRAMEWORK_H

#include <stdint.h>
#include <stdbool.h>

// Audio sample formats
typedef enum {
    AUDIO_FORMAT_S8 = 0,      // Signed 8-bit
    AUDIO_FORMAT_U8 = 1,      // Unsigned 8-bit
    AUDIO_FORMAT_S16_LE = 2,  // Signed 16-bit little-endian
    AUDIO_FORMAT_S16_BE = 3,  // Signed 16-bit big-endian
    AUDIO_FORMAT_S24_LE = 4,  // Signed 24-bit little-endian
    AUDIO_FORMAT_S32_LE = 5,  // Signed 32-bit little-endian
    AUDIO_FORMAT_FLOAT = 6,   // 32-bit float
} audio_format_t;

// Audio sample rates
typedef enum {
    AUDIO_RATE_8000 = 8000,
    AUDIO_RATE_11025 = 11025,
    AUDIO_RATE_16000 = 16000,
    AUDIO_RATE_22050 = 22050,
    AUDIO_RATE_32000 = 32000,
    AUDIO_RATE_44100 = 44100,
    AUDIO_RATE_48000 = 48000,
    AUDIO_RATE_88200 = 88200,
    AUDIO_RATE_96000 = 96000,
    AUDIO_RATE_176400 = 176400,
    AUDIO_RATE_192000 = 192000,
} audio_rate_t;

// Audio stream direction
typedef enum {
    AUDIO_DIR_PLAYBACK = 0,
    AUDIO_DIR_CAPTURE = 1,
} audio_direction_t;

// Audio buffer
typedef struct {
    void* data;
    uint32_t size;           // Total buffer size in bytes
    uint32_t frames;         // Number of frames
    uint32_t frame_size;     // Size of one frame in bytes
    uint32_t position;       // Current position in buffer
    bool dma_mapped;         // Is this buffer DMA-mapped?
    uint64_t dma_addr;       // DMA physical address
} audio_buffer_t;

// Audio stream parameters
typedef struct {
    audio_format_t format;
    audio_rate_t rate;
    uint32_t channels;
    uint32_t period_size;    // Frames per period
    uint32_t periods;        // Number of periods in buffer
    audio_direction_t direction;
} audio_params_t;

// Audio stream
typedef struct {
    uint32_t id;
    audio_params_t params;
    audio_buffer_t buffer;
    bool running;
    bool paused;
    uint64_t frames_played;
    uint64_t frames_captured;
    void* driver_data;       // Driver-specific data
} audio_stream_t;

// Audio device capabilities
typedef struct {
    uint32_t formats;        // Bitmask of supported formats
    uint32_t rates;          // Bitmask of supported rates
    uint32_t min_channels;
    uint32_t max_channels;
    uint32_t min_period_size;
    uint32_t max_period_size;
    uint32_t min_periods;
    uint32_t max_periods;
} audio_caps_t;

// Audio device
typedef struct {
    uint32_t id;
    char name[128];
    char vendor[64];
    char model[64];
    
    audio_caps_t playback_caps;
    audio_caps_t capture_caps;
    
    bool has_playback;
    bool has_capture;
    
    uint32_t playback_streams;
    uint32_t capture_streams;
    
    void* driver_data;
} audio_device_t;

// Audio driver operations
typedef struct {
    // Device management
    int (*probe)(audio_device_t* dev);
    int (*remove)(audio_device_t* dev);
    int (*suspend)(audio_device_t* dev);
    int (*resume)(audio_device_t* dev);
    
    // Stream management
    int (*stream_open)(audio_device_t* dev, audio_stream_t* stream, audio_params_t* params);
    int (*stream_close)(audio_device_t* dev, audio_stream_t* stream);
    int (*stream_start)(audio_device_t* dev, audio_stream_t* stream);
    int (*stream_stop)(audio_device_t* dev, audio_stream_t* stream);
    int (*stream_pause)(audio_device_t* dev, audio_stream_t* stream, bool pause);
    
    // Buffer management
    int (*buffer_alloc)(audio_device_t* dev, audio_stream_t* stream, uint32_t size);
    int (*buffer_free)(audio_device_t* dev, audio_stream_t* stream);
    int (*buffer_write)(audio_device_t* dev, audio_stream_t* stream, const void* data, uint32_t size);
    int (*buffer_read)(audio_device_t* dev, audio_stream_t* stream, void* data, uint32_t size);
    
    // Volume/Mixer control
    int (*set_volume)(audio_device_t* dev, uint32_t channel, uint32_t volume);
    int (*get_volume)(audio_device_t* dev, uint32_t channel, uint32_t* volume);
    int (*set_mute)(audio_device_t* dev, bool mute);
    int (*get_mute)(audio_device_t* dev, bool* mute);
} audio_driver_ops_t;

// Audio driver
typedef struct {
    const char* name;
    const audio_driver_ops_t* ops;
    void* private_data;
} audio_driver_t;

// Framework functions
int audio_framework_init(void);
void audio_framework_cleanup(void);

// Device registration
int audio_register_device(audio_device_t* dev, audio_driver_t* driver);
int audio_unregister_device(audio_device_t* dev);

// Device enumeration
uint32_t audio_get_device_count(void);
audio_device_t* audio_get_device(uint32_t index);
audio_device_t* audio_find_device_by_name(const char* name);

// Stream management
audio_stream_t* audio_stream_create(audio_device_t* dev, audio_params_t* params);
void audio_stream_destroy(audio_stream_t* stream);
int audio_stream_start(audio_stream_t* stream);
int audio_stream_stop(audio_stream_t* stream);
int audio_stream_pause(audio_stream_t* stream, bool pause);

// Buffer operations
int audio_stream_write(audio_stream_t* stream, const void* data, uint32_t size);
int audio_stream_read(audio_stream_t* stream, void* data, uint32_t size);
uint32_t audio_stream_get_position(audio_stream_t* stream);
uint32_t audio_stream_get_available(audio_stream_t* stream);

// Format conversion utilities
uint32_t audio_format_to_bytes(audio_format_t format);
const char* audio_format_to_string(audio_format_t format);
const char* audio_rate_to_string(audio_rate_t rate);

#endif // DRIVERS_AUDIO_FRAMEWORK_H
