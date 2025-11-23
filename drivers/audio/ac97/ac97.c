/**
 * @file ac97.c
 * @brief AC'97 Audio Controller Driver Implementation
 */

#include "ac97.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Kernel includes
#include "../../../kernel/include/hal/hal.h"
#include "../../../kernel/include/mm/dma.h"
#include "../../../kernel/include/hal/timer.h"
#include "../../../kernel/include/kprintf.h"

// Initialize AC'97 controller
ac97_controller_t* ac97_init(uint16_t nam_base, uint16_t nabm_base) {
    ac97_controller_t* ctrl = (ac97_controller_t*)malloc(sizeof(ac97_controller_t));
    if (!ctrl) return NULL;

    memset(ctrl, 0, sizeof(ac97_controller_t));

    ctrl->nam_base = nam_base;
    ctrl->nabm_base = nabm_base;

    kprintf("AC'97: Initializing controller (NAM=0x%04X, NABM=0x%04X)\n", nam_base, nabm_base);

    // Reset codec
    if (!ac97_reset(ctrl)) {
        free(ctrl);
        return NULL;
    }

    // Allocate buffer descriptor lists using DMA allocator for physical contiguity
    // 32 entries * sizeof(desc)
    size_t bdl_size = 32 * sizeof(ac97_buffer_desc_t);
    
    ctrl->output_bdl = (ac97_buffer_desc_t*)dma_alloc(bdl_size, DMA_FLAG_UNCACHED);
    if (!ctrl->output_bdl) {
        free(ctrl);
        return NULL;
    }
    ctrl->output_bdl_phys = dma_get_physical(ctrl->output_bdl);

    ctrl->input_bdl = (ac97_buffer_desc_t*)dma_alloc(bdl_size, DMA_FLAG_UNCACHED);
    if (!ctrl->input_bdl) {
        dma_free(ctrl->output_bdl);
        free(ctrl);
        return NULL;
    }
    ctrl->input_bdl_phys = dma_get_physical(ctrl->input_bdl);

    memset(ctrl->output_bdl, 0, bdl_size);
    memset(ctrl->input_bdl, 0, bdl_size);

    // Set default volume (50%)
    ac97_set_master_volume(ctrl, 32, 32);
    ac97_set_pcm_volume(ctrl, 32, 32);

    ctrl->initialized = true;

    kprintf("AC'97: Controller initialized successfully\n");

    return ctrl;
}

// Destroy AC'97 controller
void ac97_destroy(ac97_controller_t* ctrl) {
    if (!ctrl) return;

    ac97_stop_playback(ctrl);
    ac97_stop_recording(ctrl);

    if (ctrl->output_bdl) dma_free(ctrl->output_bdl);
    if (ctrl->input_bdl) dma_free(ctrl->input_bdl);
    free(ctrl);
}

// Reset AC'97 codec
bool ac97_reset(ac97_controller_t* ctrl) {
    if (!ctrl) return false;

    kprintf("AC'97: Resetting codec\n");

    // Write reset to mixer
    ac97_write_mixer(ctrl, AC97_RESET, 0);

    // Wait for codec to be ready
    for (int i = 0; i < 100; i++) {
        uint16_t status = ac97_read_mixer(ctrl, AC97_POWERDOWN);
        if ((status & 0x0F) == 0x0F) {
            kprintf("AC'97: Codec ready\n");
            return true;
        }
        timer_sleep_ms(1);
    }

    kprintf("AC'97: Codec reset timeout\n");
    return false;
}

// Read mixer register
uint16_t ac97_read_mixer(ac97_controller_t* ctrl, uint8_t reg) {
    if (!ctrl) return 0;
    // Inw from NAM base + reg
    uint16_t val;
    __asm__ volatile ("inw %1, %0" : \"=a\"(val) : \"Nd\"(ctrl->nam_base + reg));
    return val;
}

// Write mixer register
void ac97_write_mixer(ac97_controller_t* ctrl, uint8_t reg, uint16_t value) {
    if (!ctrl) return;
    __asm__ volatile ("outw %0, %1" : : \"a\"(value), \"Nd\"(ctrl->nam_base + reg));
}

// Set master volume
void ac97_set_master_volume(ac97_controller_t* ctrl, uint8_t left, uint8_t right) {
    if (!ctrl) return;

    // AC'97 uses 6-bit attenuation (0 = max, 63 = min)
    left = 63 - (left & 0x3F);
    right = 63 - (right & 0x3F);

    uint16_t value = (left << 8) | right;
    // Unmute (bit 15 = 0)
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

// Helper: Local outb/outl for NABM access
static inline void outb_local(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : \"a\"(val), \"Nd\"(port));
}
static inline void outl_local(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %1" : : \"a\"(val), \"Nd\"(port));
}
static inline uint8_t inb_local(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : \"=a\"(ret) : \"Nd\"(port));
    return ret;
}

// Setup playback
bool ac97_setup_playback(ac97_controller_t* ctrl, void* buffer, uint32_t size, ac97_format_t* format) {
    if (!ctrl || !buffer || !format) return false;

    // We need a physically contiguous buffer for simplicity in this driver
    // If 'buffer' is virtual, we need its physical address.
    // Check if it was allocated via dma_alloc
    paddr_t phys_addr = dma_get_physical(buffer);
    if (!phys_addr) {
        kprintf("AC'97: Buffer not DMA capable\n");
        return false;
    }

    ctrl->output_buffer = buffer;
    ctrl->output_buffer_size = size;
    ctrl->output_format = *format;

    // Setup buffer descriptor list
    // Split buffer into 2 halves for double buffering logic in simplified driver
    uint32_t half_size = size / 2;
    
    // Max samples per entry is 0xFFFF (65535)
    // Entry length is in samples? No, AC'97 BD length is in samples.
    // 1 sample = bits_per_sample / 8 bytes.
    uint32_t bytes_per_sample = format->bits_per_sample / 8;
    uint32_t samples = half_size / bytes_per_sample;

    if (samples > 0xFFFF) samples = 0xFFFF;

    ctrl->output_bdl[0].buffer_ptr = (uint32_t)phys_addr;
    ctrl->output_bdl[0].samples = samples;
    ctrl->output_bdl[0].flags = 0x8000;  // IOC (Interrupt on Completion)

    ctrl->output_bdl[1].buffer_ptr = (uint32_t)(phys_addr + half_size);
    ctrl->output_bdl[1].samples = samples;
    ctrl->output_bdl[1].flags = 0x8000;  // IOC

    // Write BDL address to NABM (Global Control PO_BDBAR)
    outl_local(ctrl->nabm_base + AC97_NABMBAR_POBDBAR, (uint32_t)ctrl->output_bdl_phys);

    // Set Last Valid Index (LVI) to 1 (since we use 0 and 1)
    outb_local(ctrl->nabm_base + AC97_NABMBAR_POLVI, 1);

    return true;
}

// Start playback
bool ac97_start_playback(ac97_controller_t* ctrl) {
    if (!ctrl) return false;

    kprintf("AC'97: Starting playback\n");

    // Reset registers
    outb_local(ctrl->nabm_base + AC97_NABMBAR_POCONTROL, 0x02); // RR (Reset Registers)
    
    // Enable PCM Out
    // Run (Bit 0)
    uint8_t control = inb_local(ctrl->nabm_base + AC97_NABMBAR_POCONTROL);
    control |= 0x01;  // RPBM (Run/Pause Bus Master)
    outb_local(ctrl->nabm_base + AC97_NABMBAR_POCONTROL, control);

    ctrl->playing = true;

    return true;
}

// Stop playback
void ac97_stop_playback(ac97_controller_t* ctrl) {
    if (!ctrl || !ctrl->playing) return;

    kprintf("AC'97: Stopping playback\n");

    // Pause/Stop
    uint8_t control = inb_local(ctrl->nabm_base + AC97_NABMBAR_POCONTROL);
    control &= ~0x01;
    outb_local(ctrl->nabm_base + AC97_NABMBAR_POCONTROL, control);

    ctrl->playing = false;
}

// Setup recording
bool ac97_setup_recording(ac97_controller_t* ctrl, void* buffer, uint32_t size, ac97_format_t* format) {
    if (!ctrl || !buffer || !format) return false;

    paddr_t phys_addr = dma_get_physical(buffer);
    if (!phys_addr) return false;

    ctrl->input_buffer = buffer;
    ctrl->input_buffer_size = size;
    ctrl->input_format = *format;

    // Setup input BDL similar to output
    uint32_t half_size = size / 2;
    uint32_t bytes_per_sample = format->bits_per_sample / 8;
    uint32_t samples = half_size / bytes_per_sample;
    if (samples > 0xFFFF) samples = 0xFFFF;

    ctrl->input_bdl[0].buffer_ptr = (uint32_t)phys_addr;
    ctrl->input_bdl[0].samples = samples;
    ctrl->input_bdl[0].flags = 0x8000;

    ctrl->input_bdl[1].buffer_ptr = (uint32_t)(phys_addr + half_size);
    ctrl->input_bdl[1].samples = samples;
    ctrl->input_bdl[1].flags = 0x8000;

    // Write PI_BDBAR
    outl_local(ctrl->nabm_base + AC97_NABMBAR_PIBDBAR, (uint32_t)ctrl->input_bdl_phys);
    
    // Set LVI
    outb_local(ctrl->nabm_base + AC97_NABMBAR_PILVI, 1);

    return true;
}

// Start recording
bool ac97_start_recording(ac97_controller_t* ctrl) {
    if (!ctrl) return false;

    kprintf("AC'97: Starting recording\n");

    outb_local(ctrl->nabm_base + AC97_NABMBAR_PICONTROL, 0x02); // Reset

    uint8_t control = inb_local(ctrl->nabm_base + AC97_NABMBAR_PICONTROL);
    control |= 0x01;
    outb_local(ctrl->nabm_base + AC97_NABMBAR_PICONTROL, control);

    ctrl->recording = true;

    return true;
}

// Stop recording
void ac97_stop_recording(ac97_controller_t* ctrl) {
    if (!ctrl || !ctrl->recording) return;

    kprintf("AC'97: Stopping recording\n");

    uint8_t control = inb_local(ctrl->nabm_base + AC97_NABMBAR_PICONTROL);
    control &= ~0x01;
    outb_local(ctrl->nabm_base + AC97_NABMBAR_PICONTROL, control);

    ctrl->recording = false;
}