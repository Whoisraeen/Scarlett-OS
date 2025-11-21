/**
 * @file ac97.h
 * @brief AC'97 Audio Controller Driver
 *
 * User-space driver for AC'97 legacy audio controllers
 */

#ifndef DRIVERS_AUDIO_AC97_H
#define DRIVERS_AUDIO_AC97_H

#include <stdint.h>
#include <stdbool.h>

// AC'97 Registers (NAM - Native Audio Mixer)
#define AC97_RESET 0x00
#define AC97_MASTER_VOLUME 0x02
#define AC97_HEADPHONE_VOLUME 0x04
#define AC97_MASTER_VOLUME_MONO 0x06
#define AC97_MASTER_TONE 0x08
#define AC97_PC_BEEP_VOLUME 0x0A
#define AC97_PHONE_VOLUME 0x0C
#define AC97_MIC_VOLUME 0x0E
#define AC97_LINE_IN_VOLUME 0x10
#define AC97_CD_VOLUME 0x12
#define AC97_VIDEO_VOLUME 0x14
#define AC97_AUX_VOLUME 0x16
#define AC97_PCM_OUT_VOLUME 0x18
#define AC97_RECORD_SELECT 0x1A
#define AC97_RECORD_GAIN 0x1C
#define AC97_RECORD_GAIN_MIC 0x1E
#define AC97_GENERAL_PURPOSE 0x20
#define AC97_3D_CONTROL 0x22
#define AC97_POWERDOWN 0x26
#define AC97_EXTENDED_AUDIO_ID 0x28
#define AC97_EXTENDED_AUDIO_STATUS 0x2A

// AC'97 Bus Master Registers
#define AC97_NABMBAR_PICONTROL 0x0B  // PCM In Control
#define AC97_NABMBAR_PISTATUS 0x06   // PCM In Status
#define AC97_NABMBAR_POCONTROL 0x1B  // PCM Out Control
#define AC97_NABMBAR_POSTATUS 0x16   // PCM Out Status

// Audio format
typedef struct {
    uint32_t sample_rate;  // 8000, 11025, 16000, 22050, 32000, 44100, 48000
    uint8_t bits_per_sample;  // 8 or 16
    uint8_t channels;  // 1 (mono) or 2 (stereo)
} ac97_format_t;

// Buffer Descriptor
typedef struct {
    uint32_t buffer_ptr;
    uint16_t samples;
    uint16_t flags;
} __attribute__((packed)) ac97_buffer_desc_t;

// AC'97 controller
typedef struct {
    uint16_t nam_base;   // Native Audio Mixer base I/O port
    uint16_t nabm_base;  // Native Audio Bus Master base I/O port

    // Buffer descriptor lists
    ac97_buffer_desc_t* output_bdl;
    uint64_t output_bdl_phys;

    ac97_buffer_desc_t* input_bdl;
    uint64_t input_bdl_phys;

    // Audio buffers
    void* output_buffer;
    uint32_t output_buffer_size;
    void* input_buffer;
    uint32_t input_buffer_size;

    // State
    bool initialized;
    bool playing;
    bool recording;

    // Format
    ac97_format_t output_format;
    ac97_format_t input_format;
} ac97_controller_t;

// AC'97 operations
ac97_controller_t* ac97_init(uint16_t nam_base, uint16_t nabm_base);
void ac97_destroy(ac97_controller_t* ctrl);
bool ac97_reset(ac97_controller_t* ctrl);

// Mixer operations
uint16_t ac97_read_mixer(ac97_controller_t* ctrl, uint8_t reg);
void ac97_write_mixer(ac97_controller_t* ctrl, uint8_t reg, uint16_t value);
void ac97_set_master_volume(ac97_controller_t* ctrl, uint8_t left, uint8_t right);
void ac97_set_pcm_volume(ac97_controller_t* ctrl, uint8_t left, uint8_t right);

// Playback operations
bool ac97_setup_playback(ac97_controller_t* ctrl, void* buffer, uint32_t size, ac97_format_t* format);
bool ac97_start_playback(ac97_controller_t* ctrl);
void ac97_stop_playback(ac97_controller_t* ctrl);

// Recording operations
bool ac97_setup_recording(ac97_controller_t* ctrl, void* buffer, uint32_t size, ac97_format_t* format);
bool ac97_start_recording(ac97_controller_t* ctrl);
void ac97_stop_recording(ac97_controller_t* ctrl);

#endif // DRIVERS_AUDIO_AC97_H
