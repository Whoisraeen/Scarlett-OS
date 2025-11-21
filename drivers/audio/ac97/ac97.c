/**
 * @file ac97.c
 * @brief AC'97 Audio Controller Driver Implementation
 */

#include "ac97.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// I/O port operations (TODO: implement via syscalls)
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}

// Initialize AC'97 controller
ac97_controller_t* ac97_init(uint16_t nam_base, uint16_t nabm_base) {
    ac97_controller_t* ctrl = (ac97_controller_t*)malloc(sizeof(ac97_controller_t));
    if (!ctrl) return NULL;

    memset(ctrl, 0, sizeof(ac97_controller_t));

    ctrl->nam_base = nam_base;
    ctrl->nabm_base = nabm_base;

    printf("AC'97: Initializing controller (NAM=0x%04X, NABM=0x%04X)\n", nam_base, nabm_base);

    // Reset codec
    if (!ac97_reset(ctrl)) {
        free(ctrl);
        return NULL;
    }

    // Allocate buffer descriptor lists
    ctrl->output_bdl = (ac97_buffer_desc_t*)malloc(32 * sizeof(ac97_buffer_desc_t));
    ctrl->input_bdl = (ac97_buffer_desc_t*)malloc(32 * sizeof(ac97_buffer_desc_t));

    if (!ctrl->output_bdl || !ctrl->input_bdl) {
        free(ctrl->output_bdl);
        free(ctrl->input_bdl);
        free(ctrl);
        return NULL;
    }

    memset(ctrl->output_bdl, 0, 32 * sizeof(ac97_buffer_desc_t));
    memset(ctrl->input_bdl, 0, 32 * sizeof(ac97_buffer_desc_t));

    // TODO: Get physical addresses
    ctrl->output_bdl_phys = 0;
    ctrl->input_bdl_phys = 0;

    // Set default volume (50%)
    ac97_set_master_volume(ctrl, 32, 32);
    ac97_set_pcm_volume(ctrl, 32, 32);

    ctrl->initialized = true;

    printf("AC'97: Controller initialized successfully\n");

    return ctrl;
}

// Destroy AC'97 controller
void ac97_destroy(ac97_controller_t* ctrl) {
    if (!ctrl) return;

    ac97_stop_playback(ctrl);
    ac97_stop_recording(ctrl);

    free(ctrl->output_bdl);
    free(ctrl->input_bdl);
    free(ctrl);
}

// Reset AC'97 codec
bool ac97_reset(ac97_controller_t* ctrl) {
    if (!ctrl) return false;

    printf("AC'97: Resetting codec\n");

    // Write reset to mixer
    ac97_write_mixer(ctrl, AC97_RESET, 0);

    // Wait for codec to be ready
    for (int i = 0; i < 1000; i++) {
        uint16_t status = ac97_read_mixer(ctrl, AC97_POWERDOWN);
        if ((status & 0x0F) == 0x0F) {
            printf("AC'97: Codec ready\n");
            return true;
        }
        // TODO: usleep(1000);
    }

    printf("AC'97: Codec reset timeout\n");
    return false;
}

// Read mixer register
uint16_t ac97_read_mixer(ac97_controller_t* ctrl, uint8_t reg) {
    if (!ctrl) return 0;
    return inw(ctrl->nam_base + reg);
}

// Write mixer register
void ac97_write_mixer(ac97_controller_t* ctrl, uint8_t reg, uint16_t value) {
    if (!ctrl) return;
    outw(ctrl->nam_base + reg, value);
}

// Set master volume
void ac97_set_master_volume(ac97_controller_t* ctrl, uint8_t left, uint8_t right) {
    if (!ctrl) return;

    // AC'97 uses 6-bit attenuation (0 = max, 63 = min)
    // Convert 0-63 range to attenuation
    left = 63 - (left & 0x3F);
    right = 63 - (right & 0x3F);

    uint16_t value = (left << 8) | right;
    ac97_write_mixer(ctrl, AC97_MASTER_VOLUME, value);
}

// Set PCM volume
void ac97_set_pcm_volume(ac97_controller_t* ctrl, uint8_t left, uint8_t right) {
    if (!ctrl) return;

    left = 63 - (left & 0x3F);
    right = 63 - (right & 0x3F);

    uint16_t value = (left << 8) | right;
    ac97_write_mixer(ctrl, AC97_PCM_OUT_VOLUME, value);
}

// Setup playback
bool ac97_setup_playback(ac97_controller_t* ctrl, void* buffer, uint32_t size, ac97_format_t* format) {
    if (!ctrl || !buffer || !format) return false;

    ctrl->output_buffer = buffer;
    ctrl->output_buffer_size = size;
    ctrl->output_format = *format;

    // Setup buffer descriptor list
    uint32_t buffer_size = size / 2;  // Use 2 buffers
    ctrl->output_bdl[0].buffer_ptr = 0;  // TODO: Physical address
    ctrl->output_bdl[0].samples = buffer_size / (format->bits_per_sample / 8);
    ctrl->output_bdl[0].flags = 0x8000;  // IOC (Interrupt on Completion)

    ctrl->output_bdl[1].buffer_ptr = buffer_size;
    ctrl->output_bdl[1].samples = buffer_size / (format->bits_per_sample / 8);
    ctrl->output_bdl[1].flags = 0x8000;

    // Write BDL address
    outl(ctrl->nabm_base + 0x10, (uint32_t)ctrl->output_bdl_phys);

    // Set Last Valid Index
    outb(ctrl->nabm_base + 0x15, 1);  // 2 buffers

    return true;
}

// Start playback
bool ac97_start_playback(ac97_controller_t* ctrl) {
    if (!ctrl) return false;

    printf("AC'97: Starting playback\n");

    // Enable PCM Out
    uint8_t control = inb(ctrl->nabm_base + AC97_NABMBAR_POCONTROL);
    control |= 0x01;  // RPBM (Run/Pause Bus Master)
    outb(ctrl->nabm_base + AC97_NABMBAR_POCONTROL, control);

    ctrl->playing = true;

    return true;
}

// Stop playback
void ac97_stop_playback(ac97_controller_t* ctrl) {
    if (!ctrl || !ctrl->playing) return;

    printf("AC'97: Stopping playback\n");

    // Disable PCM Out
    uint8_t control = inb(ctrl->nabm_base + AC97_NABMBAR_POCONTROL);
    control &= ~0x01;
    outb(ctrl->nabm_base + AC97_NABMBAR_POCONTROL, control);

    ctrl->playing = false;
}

// Setup recording
bool ac97_setup_recording(ac97_controller_t* ctrl, void* buffer, uint32_t size, ac97_format_t* format) {
    if (!ctrl || !buffer || !format) return false;

    ctrl->input_buffer = buffer;
    ctrl->input_buffer_size = size;
    ctrl->input_format = *format;

    // TODO: Setup input BDL similar to output

    return true;
}

// Start recording
bool ac97_start_recording(ac97_controller_t* ctrl) {
    if (!ctrl) return false;

    printf("AC'97: Starting recording\n");

    // Enable PCM In
    uint8_t control = inb(ctrl->nabm_base + AC97_NABMBAR_PICONTROL);
    control |= 0x01;
    outb(ctrl->nabm_base + AC97_NABMBAR_PICONTROL, control);

    ctrl->recording = true;

    return true;
}

// Stop recording
void ac97_stop_recording(ac97_controller_t* ctrl) {
    if (!ctrl || !ctrl->recording) return;

    printf("AC'97: Stopping recording\n");

    // Disable PCM In
    uint8_t control = inb(ctrl->nabm_base + AC97_NABMBAR_PICONTROL);
    control &= ~0x01;
    outb(ctrl->nabm_base + AC97_NABMBAR_PICONTROL, control);

    ctrl->recording = false;
}
