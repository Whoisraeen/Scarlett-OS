/**
 * @file hda.c
 * @brief Intel High Definition Audio Driver Implementation
 */

#include "hda.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// HDA verbs (commands)
#define VERB_GET_PARAMETER 0xF00
#define VERB_GET_CONNECTION_SELECT 0xF01
#define VERB_SET_CONNECTION_SELECT 0x701
#define VERB_GET_STREAM_FORMAT 0xA00
#define VERB_SET_STREAM_FORMAT 0x200
#define VERB_GET_AMP_GAIN_MUTE 0xB00
#define VERB_SET_AMP_GAIN_MUTE 0x300
#define VERB_GET_PIN_WIDGET_CONTROL 0xF07
#define VERB_SET_PIN_WIDGET_CONTROL 0x707
#define VERB_GET_POWER_STATE 0xF05
#define VERB_SET_POWER_STATE 0x705

// Parameters
#define PARAM_VENDOR_ID 0x00
#define PARAM_REVISION_ID 0x02
#define PARAM_NODE_COUNT 0x04
#define PARAM_FUNCTION_GROUP_TYPE 0x05
#define PARAM_AUDIO_WIDGET_CAPS 0x09
#define PARAM_PIN_CAPS 0x0C
#define PARAM_CONNECTION_LIST_LENGTH 0x0E

// Read 32-bit register
uint32_t hda_read32(hda_controller_t* ctrl, uint32_t offset) {
    if (!ctrl || !ctrl->mmio_base) return 0;
    return *(volatile uint32_t*)((uint8_t*)ctrl->mmio_base + offset);
}

// Write 32-bit register
void hda_write32(hda_controller_t* ctrl, uint32_t offset, uint32_t value) {
    if (!ctrl || !ctrl->mmio_base) return;
    *(volatile uint32_t*)((uint8_t*)ctrl->mmio_base + offset) = value;
}

// Read 16-bit register
uint16_t hda_read16(hda_controller_t* ctrl, uint32_t offset) {
    if (!ctrl || !ctrl->mmio_base) return 0;
    return *(volatile uint16_t*)((uint8_t*)ctrl->mmio_base + offset);
}

// Write 16-bit register
void hda_write16(hda_controller_t* ctrl, uint32_t offset, uint16_t value) {
    if (!ctrl || !ctrl->mmio_base) return;
    *(volatile uint16_t*)((uint8_t*)ctrl->mmio_base + offset) = value;
}

// Read 8-bit register
uint8_t hda_read8(hda_controller_t* ctrl, uint32_t offset) {
    if (!ctrl || !ctrl->mmio_base) return 0;
    return *(volatile uint8_t*)((uint8_t*)ctrl->mmio_base + offset);
}

// Write 8-bit register
void hda_write8(hda_controller_t* ctrl, uint32_t offset, uint8_t value) {
    if (!ctrl || !ctrl->mmio_base) return;
    *(volatile uint8_t*)((uint8_t*)ctrl->mmio_base + offset) = value;
}

// Create HDA verb command
uint32_t hda_make_verb(uint8_t codec, uint8_t nid, uint16_t verb, uint16_t payload) {
    return ((uint32_t)codec << 28) | ((uint32_t)nid << 20) | ((uint32_t)verb << 8) | payload;
}

// Initialize HDA controller
hda_controller_t* hda_init(uint8_t bus, uint8_t device, uint8_t function) {
    hda_controller_t* ctrl = (hda_controller_t*)malloc(sizeof(hda_controller_t));
    if (!ctrl) return NULL;

    memset(ctrl, 0, sizeof(hda_controller_t));

    ctrl->bus = bus;
    ctrl->device = device;
    ctrl->function = function;

    // TODO: Get vendor/device ID from PCI config space
    // TODO: Get MMIO base address from PCI BAR0
    // For now, use placeholder values
    ctrl->mmio_phys = 0;
    ctrl->mmio_size = 0x4000;  // 16KB typical size

    printf("HDA: Initializing controller at %02x:%02x.%x\n", bus, device, function);

    // TODO: Map MMIO region
    // ctrl->mmio_base = map_mmio(ctrl->mmio_phys, ctrl->mmio_size);

    return ctrl;
}

// Destroy HDA controller
void hda_destroy(hda_controller_t* ctrl) {
    if (!ctrl) return;

    hda_stop(ctrl);

    // TODO: Unmap MMIO
    // TODO: Free CORB/RIRB buffers
    // TODO: Free streams

    free(ctrl);
}

// Reset HDA controller
bool hda_reset(hda_controller_t* ctrl) {
    if (!ctrl || !ctrl->mmio_base) return false;

    printf("HDA: Resetting controller\n");

    // Reset controller
    uint32_t gctl = hda_read32(ctrl, HDA_GCTL);
    gctl &= ~0x01;  // Clear CRST (Controller Reset)
    hda_write32(ctrl, HDA_GCTL, gctl);

    // Wait for reset to complete
    for (int i = 0; i < 1000; i++) {
        if ((hda_read32(ctrl, HDA_GCTL) & 0x01) == 0) {
            break;
        }
        // TODO: usleep(100);
    }

    // Exit reset
    gctl |= 0x01;
    hda_write32(ctrl, HDA_GCTL, gctl);

    // Wait for controller to be ready
    for (int i = 0; i < 1000; i++) {
        if (hda_read32(ctrl, HDA_GCTL) & 0x01) {
            break;
        }
        // TODO: usleep(100);
    }

    // Read capabilities
    ctrl->gcap = hda_read16(ctrl, HDA_GCAP);
    ctrl->major_version = hda_read8(ctrl, HDA_VMAJ);
    ctrl->minor_version = hda_read8(ctrl, HDA_VMIN);

    ctrl->num_input_streams = (ctrl->gcap >> 8) & 0x0F;
    ctrl->num_output_streams = (ctrl->gcap >> 12) & 0x0F;
    ctrl->num_bidirectional_streams = (ctrl->gcap >> 3) & 0x1F;

    printf("HDA: Version %d.%d\n", ctrl->major_version, ctrl->minor_version);
    printf("HDA: %d input, %d output, %d bidirectional streams\n",
           ctrl->num_input_streams, ctrl->num_output_streams, ctrl->num_bidirectional_streams);

    return true;
}

// Initialize CORB/RIRB
bool hda_init_corb_rirb(hda_controller_t* ctrl) {
    if (!ctrl || !ctrl->mmio_base) return false;

    printf("HDA: Initializing CORB/RIRB\n");

    // Stop CORB/RIRB
    hda_write8(ctrl, HDA_CORBCTL, 0);
    hda_write8(ctrl, HDA_RIRBCTL, 0);

    // Allocate CORB (256 entries = 1KB)
    ctrl->corb_size = 256;
    ctrl->corb = (uint32_t*)malloc(ctrl->corb_size * sizeof(uint32_t));
    if (!ctrl->corb) return false;

    memset(ctrl->corb, 0, ctrl->corb_size * sizeof(uint32_t));
    // TODO: Get physical address
    ctrl->corb_phys = 0;  // Placeholder

    // Set CORB base address
    hda_write32(ctrl, HDA_CORBLBASE, (uint32_t)(ctrl->corb_phys & 0xFFFFFFFF));
    hda_write32(ctrl, HDA_CORBUBASE, (uint32_t)(ctrl->corb_phys >> 32));

    // Set CORB size (256 entries)
    hda_write8(ctrl, HDA_CORBSIZE, 0x02);

    // Reset CORB read pointer
    hda_write16(ctrl, HDA_CORBRP, 0x8000);  // Set reset bit
    hda_write16(ctrl, HDA_CORBRP, 0);       // Clear reset bit

    // Allocate RIRB (256 entries = 2KB)
    ctrl->rirb_size = 256;
    ctrl->rirb = (uint64_t*)malloc(ctrl->rirb_size * sizeof(uint64_t));
    if (!ctrl->rirb) {
        free(ctrl->corb);
        return false;
    }

    memset(ctrl->rirb, 0, ctrl->rirb_size * sizeof(uint64_t));
    ctrl->rirb_phys = 0;  // Placeholder

    // Set RIRB base address
    hda_write32(ctrl, HDA_RIRBLBASE, (uint32_t)(ctrl->rirb_phys & 0xFFFFFFFF));
    hda_write32(ctrl, HDA_RIRBUBASE, (uint32_t)(ctrl->rirb_phys >> 32));

    // Set RIRB size (256 entries)
    hda_write8(ctrl, HDA_RIRBSIZE, 0x02);

    // Reset RIRB write pointer
    hda_write16(ctrl, HDA_RIRBWP, 0x8000);

    // Enable CORB/RIRB
    hda_write8(ctrl, HDA_CORBCTL, 0x02);  // Run CORB
    hda_write8(ctrl, HDA_RIRBCTL, 0x02);  // Run RIRB

    return true;
}

// Send command via CORB
bool hda_send_command(hda_controller_t* ctrl, uint32_t command) {
    if (!ctrl || !ctrl->corb) return false;

    uint16_t wp = hda_read16(ctrl, HDA_CORBWP);
    uint16_t rp = hda_read16(ctrl, HDA_CORBRP);

    // Check if CORB is full
    uint16_t next_wp = (wp + 1) % ctrl->corb_size;
    if (next_wp == rp) {
        return false;  // CORB full
    }

    // Write command to CORB
    ctrl->corb[next_wp] = command;

    // Update write pointer
    hda_write16(ctrl, HDA_CORBWP, next_wp);

    return true;
}

// Get response from RIRB
bool hda_get_response(hda_controller_t* ctrl, uint64_t* response) {
    if (!ctrl || !ctrl->rirb || !response) return false;

    static uint16_t last_rp = 0;
    uint16_t wp = hda_read16(ctrl, HDA_RIRBWP) & 0xFF;

    // Check if there's a response
    if (last_rp == wp) {
        return false;  // No response
    }

    // Read response
    last_rp = (last_rp + 1) % ctrl->rirb_size;
    *response = ctrl->rirb[last_rp];

    return true;
}

// Detect codecs
bool hda_detect_codecs(hda_controller_t* ctrl) {
    if (!ctrl) return false;

    printf("HDA: Detecting codecs\n");

    uint16_t statests = hda_read16(ctrl, HDA_STATESTS);

    ctrl->codec_count = 0;
    for (uint8_t i = 0; i < 15; i++) {
        if (statests & (1 << i)) {
            printf("HDA: Found codec at address %d\n", i);
            ctrl->codecs[ctrl->codec_count].addr = i;
            hda_init_codec(ctrl, i);
            ctrl->codec_count++;
        }
    }

    return ctrl->codec_count > 0;
}

// Initialize codec
bool hda_init_codec(hda_controller_t* ctrl, uint8_t codec_addr) {
    if (!ctrl) return false;

    printf("HDA: Initializing codec %d\n", codec_addr);

    hda_codec_t* codec = NULL;
    for (uint32_t i = 0; i < ctrl->codec_count; i++) {
        if (ctrl->codecs[i].addr == codec_addr) {
            codec = &ctrl->codecs[i];
            break;
        }
    }

    if (!codec) return false;

    // Get vendor ID
    uint32_t cmd = hda_make_verb(codec_addr, 0, VERB_GET_PARAMETER, PARAM_VENDOR_ID);
    hda_send_command(ctrl, cmd);

    uint64_t response;
    for (int i = 0; i < 100; i++) {
        if (hda_get_response(ctrl, &response)) {
            codec->vendor_id = (uint32_t)response;
            printf("HDA: Codec vendor ID: 0x%08X\n", codec->vendor_id);
            break;
        }
        // TODO: usleep(100);
    }

    // Get revision ID
    cmd = hda_make_verb(codec_addr, 0, VERB_GET_PARAMETER, PARAM_REVISION_ID);
    hda_send_command(ctrl, cmd);

    for (int i = 0; i < 100; i++) {
        if (hda_get_response(ctrl, &response)) {
            codec->revision_id = (uint32_t)response;
            break;
        }
    }

    // Enumerate nodes
    hda_enumerate_nodes(ctrl, codec);

    return true;
}

// Enumerate codec nodes
bool hda_enumerate_nodes(hda_controller_t* ctrl, hda_codec_t* codec) {
    if (!ctrl || !codec) return false;

    // Get node count
    uint32_t cmd = hda_make_verb(codec->addr, 0, VERB_GET_PARAMETER, PARAM_NODE_COUNT);
    hda_send_command(ctrl, cmd);

    uint64_t response;
    if (!hda_get_response(ctrl, &response)) {
        return false;
    }

    uint8_t start_nid = (response >> 16) & 0xFF;
    uint8_t num_nodes = response & 0xFF;

    printf("HDA: Codec %d has %d nodes starting at NID %d\n", codec->addr, num_nodes, start_nid);

    codec->node_count = 0;
    for (uint8_t i = 0; i < num_nodes && codec->node_count < 128; i++) {
        uint8_t nid = start_nid + i;
        hda_node_t* node = &codec->nodes[codec->node_count];

        node->nid = nid;

        // Get widget capabilities
        cmd = hda_make_verb(codec->addr, nid, VERB_GET_PARAMETER, PARAM_AUDIO_WIDGET_CAPS);
        hda_send_command(ctrl, cmd);

        if (hda_get_response(ctrl, &response)) {
            node->wcaps = (uint32_t)response;

            // Check if it's an output or input
            uint8_t type = (node->wcaps >> 20) & 0x0F;
            node->is_output = (type == 0x00);  // Audio Output
            node->is_input = (type == 0x01);   // Audio Input

            if (node->is_output && codec->output_nid == 0) {
                codec->output_nid = nid;
                printf("HDA: Found output node at NID %d\n", nid);
            }

            if (node->is_input && codec->input_nid == 0) {
                codec->input_nid = nid;
                printf("HDA: Found input node at NID %d\n", nid);
            }

            codec->node_count++;
        }
    }

    return true;
}

// Start HDA controller
bool hda_start(hda_controller_t* ctrl) {
    if (!ctrl) return false;

    if (!hda_reset(ctrl)) {
        return false;
    }

    if (!hda_init_corb_rirb(ctrl)) {
        return false;
    }

    if (!hda_detect_codecs(ctrl)) {
        printf("HDA: Warning - No codecs detected\n");
    }

    ctrl->running = true;
    ctrl->initialized = true;

    printf("HDA: Controller initialized successfully\n");

    return true;
}

// Stop HDA controller
void hda_stop(hda_controller_t* ctrl) {
    if (!ctrl || !ctrl->running) return;

    // Stop all streams
    // TODO: Implement

    // Stop CORB/RIRB
    if (ctrl->mmio_base) {
        hda_write8(ctrl, HDA_CORBCTL, 0);
        hda_write8(ctrl, HDA_RIRBCTL, 0);
    }

    ctrl->running = false;
}

// Create stream
hda_stream_t* hda_create_stream(hda_controller_t* ctrl, bool is_input) {
    if (!ctrl) return NULL;

    hda_stream_t* stream = (hda_stream_t*)malloc(sizeof(hda_stream_t));
    if (!stream) return NULL;

    memset(stream, 0, sizeof(hda_stream_t));
    stream->is_input = is_input;

    // TODO: Allocate stream descriptor from available streams

    return stream;
}

// Destroy stream
void hda_destroy_stream(hda_stream_t* stream) {
    if (!stream) return;

    // TODO: Free buffers

    free(stream);
}

// Setup stream format
bool hda_setup_stream(hda_stream_t* stream, hda_format_t format, hda_rate_t rate, uint32_t channels) {
    if (!stream) return false;

    stream->format = format;
    stream->rate = rate;
    stream->channels = channels;

    // TODO: Calculate and write format register

    return true;
}

// Start stream
bool hda_start_stream(hda_controller_t* ctrl, hda_stream_t* stream) {
    if (!ctrl || !stream) return false;

    // TODO: Start DMA

    stream->is_running = true;

    return true;
}

// Stop stream
void hda_stop_stream(hda_controller_t* ctrl, hda_stream_t* stream) {
    if (!ctrl || !stream) return;

    // TODO: Stop DMA

    stream->is_running = false;
}

// Get stream position
uint32_t hda_get_stream_position(hda_controller_t* ctrl, hda_stream_t* stream) {
    if (!ctrl || !stream || !ctrl->mmio_base) return 0;

    // Read LPIB (Link Position in Buffer)
    uint32_t offset = stream->base_offset + HDA_SD_LPIB;
    return hda_read32(ctrl, offset);
}

// Setup buffer
bool hda_setup_buffer(hda_stream_t* stream, void* buffer, uint32_t size) {
    if (!stream || !buffer) return false;

    stream->buffer = buffer;
    stream->buffer_size = size;
    // TODO: Get physical address
    stream->buffer_phys = 0;

    // Setup BDL (Buffer Descriptor List)
    stream->bdl_entries = 2;  // Use 2 buffers for double buffering
    stream->bdl = (hda_bdl_entry_t*)malloc(stream->bdl_entries * sizeof(hda_bdl_entry_t));

    if (!stream->bdl) return false;

    uint32_t buffer_per_entry = size / stream->bdl_entries;

    for (uint32_t i = 0; i < stream->bdl_entries; i++) {
        stream->bdl[i].address = stream->buffer_phys + (i * buffer_per_entry);
        stream->bdl[i].length = buffer_per_entry;
        stream->bdl[i].ioc = 1;  // Interrupt on completion
    }

    // TODO: Get BDL physical address
    stream->bdl_phys = 0;

    return true;
}
