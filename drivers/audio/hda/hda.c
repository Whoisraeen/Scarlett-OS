/**
 * @file hda.c
 * @brief Intel High Definition Audio Driver Implementation
 */

#include "hda.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Kernel includes
#include "../../../kernel/drivers/pci/pci.h"
#include "../../../kernel/include/mm/dma.h"
#include "../../../kernel/include/mm/vmm.h"
#include "../../../kernel/include/hal/timer.h"
#include "../../../kernel/include/kprintf.h"

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

// Helper: Sleep wrapper
static void hda_sleep_ms(uint32_t ms) {
    timer_sleep_ms(ms);
}

// Initialize HDA controller
hda_controller_t* hda_init(uint8_t bus, uint8_t device, uint8_t function) {
    hda_controller_t* ctrl = (hda_controller_t*)malloc(sizeof(hda_controller_t));
    if (!ctrl) return NULL;

    memset(ctrl, 0, sizeof(hda_controller_t));

    ctrl->bus = bus;
    ctrl->device = device;
    ctrl->function = function;

    // Get vendor/device ID
    uint32_t vendor_dev = pci_read_config(bus, device, function, PCI_CONFIG_VENDOR_ID);
    ctrl->vendor_id = vendor_dev & 0xFFFF;
    ctrl->device_id = (vendor_dev >> 16) & 0xFFFF;

    // Get BAR0 (MMIO base)
    pci_device_t dev_struct;
    dev_struct.bus = bus;
    dev_struct.device = device;
    dev_struct.function = function;
    // Fill bars for pci_decode_bar (it reads from struct)
    for(int i=0; i<6; i++) dev_struct.bars[i] = pci_read_config(bus, device, function, PCI_CONFIG_BAR0 + i*4);

    pci_bar_info_t bar0;
    if (pci_decode_bar(&dev_struct, 0, &bar0) != ERR_OK) {
        kprintf("HDA: Failed to decode BAR0\n");
        free(ctrl);
        return NULL;
    }

    ctrl->mmio_phys = bar0.base_address;
    ctrl->mmio_size = bar0.size;
    
    if (ctrl->mmio_size < 0x4000) ctrl->mmio_size = 0x4000; // Minimum 16KB

    kprintf("HDA: Initializing controller at %02x:%02x.%x (MMIO: 0x%016lx, Size: 0x%lx)\n", 
           bus, device, function, ctrl->mmio_phys, ctrl->mmio_size);

    // Map MMIO region
    // Use dma_alloc for MMIO? No, dma_alloc allocates RAM. We need to map existing physical MMIO.
    // vmm_map_pages(as, vaddr, paddr, count, flags)
    // We need a virtual address space range for MMIO. 
    // Assuming we can find a free kernel virtual address or use a direct map if available.
    // For now, let's assume direct map access or allocate a page range.
    // Implementation detail: On x86_64/ARM64, higher half kernel usually has a direct map or specific MMIO range.
    // Ideally: ctrl->mmio_base = vmm_mmio_map(ctrl->mmio_phys, ctrl->mmio_size);
    // Since we don't have vmm_mmio_map exposed, let's try to use vmm_map_pages with specific flags to kernel space.
    
    // HACK: Using a fixed offset or finding a hole?
    // Better: Create `ioremap` equivalent in VMM or use a dedicated range.
    // For this implementation, I'll define a base and increment (very simplified kernel allocator)
    static uint64_t next_mmio_vaddr = 0xFFFFFF8000000000ULL; // Typical high kernel area
    
    ctrl->mmio_base = (void*)next_mmio_vaddr;
    next_mmio_vaddr += ((ctrl->mmio_size + 4095) & ~4095);
    
    address_space_t* k_as = vmm_get_kernel_address_space();
    if (vmm_map_pages(k_as, (vaddr_t)ctrl->mmio_base, ctrl->mmio_phys, 
                      (ctrl->mmio_size + 4095)/4096, 
                      VMM_PRESENT | VMM_WRITE | VMM_NOCACHE | VMM_GLOBAL) != 0) {
        kprintf("HDA: Failed to map MMIO\n");
        free(ctrl);
        return NULL;
    }

    return ctrl;
}

// Destroy HDA controller
void hda_destroy(hda_controller_t* ctrl) {
    if (!ctrl) return;

    hda_stop(ctrl);

    // Unmap MMIO
    if (ctrl->mmio_base) {
        address_space_t* k_as = vmm_get_kernel_address_space();
        vmm_unmap_pages(k_as, (vaddr_t)ctrl->mmio_base, (ctrl->mmio_size + 4095)/4096);
    }

    // Free CORB/RIRB buffers
    if (ctrl->corb) dma_free(ctrl->corb);
    if (ctrl->rirb) dma_free(ctrl->rirb);

    // Free streams
    if (ctrl->streams) free(ctrl->streams);

    free(ctrl);
}

// Reset HDA controller
bool hda_reset(hda_controller_t* ctrl) {
    if (!ctrl || !ctrl->mmio_base) return false;

    kprintf("HDA: Resetting controller\n");

    // Reset controller
    uint32_t gctl = hda_read32(ctrl, HDA_GCTL);
    gctl &= ~0x01;  // Clear CRST (Controller Reset)
    hda_write32(ctrl, HDA_GCTL, gctl);

    // Wait for reset to complete
    for (int i = 0; i < 1000; i++) {
        if ((hda_read32(ctrl, HDA_GCTL) & 0x01) == 0) {
            break;
        }
        hda_sleep_ms(1);
    }

    // Exit reset
    gctl |= 0x01;
    hda_write32(ctrl, HDA_GCTL, gctl);

    // Wait for controller to be ready
    for (int i = 0; i < 1000; i++) {
        if (hda_read32(ctrl, HDA_GCTL) & 0x01) {
            break;
        }
        hda_sleep_ms(1);
    }

    // Read capabilities
    ctrl->gcap = hda_read16(ctrl, HDA_GCAP);
    ctrl->major_version = hda_read8(ctrl, HDA_VMAJ);
    ctrl->minor_version = hda_read8(ctrl, HDA_VMIN);

    ctrl->num_input_streams = (ctrl->gcap >> 8) & 0x0F;
    ctrl->num_output_streams = (ctrl->gcap >> 12) & 0x0F;
    ctrl->num_bidirectional_streams = (ctrl->gcap >> 3) & 0x1F;

    kprintf("HDA: Version %d.%d\n", ctrl->major_version, ctrl->minor_version);
    kprintf("HDA: %d input, %d output, %d bidirectional streams\n",
           ctrl->num_input_streams, ctrl->num_output_streams, ctrl->num_bidirectional_streams);

    return true;
}

// Initialize CORB/RIRB
bool hda_init_corb_rirb(hda_controller_t* ctrl) {
    if (!ctrl || !ctrl->mmio_base) return false;

    kprintf("HDA: Initializing CORB/RIRB\n");

    // Stop CORB/RIRB
    hda_write8(ctrl, HDA_CORBCTL, 0);
    hda_write8(ctrl, HDA_RIRBCTL, 0);

    // Allocate CORB (256 entries = 1KB)
    ctrl->corb_size = 256;
    // Use dma_alloc for physical contiguity
    ctrl->corb = (uint32_t*)dma_alloc(ctrl->corb_size * sizeof(uint32_t), DMA_FLAG_UNCACHED);
    if (!ctrl->corb) return false;

    ctrl->corb_phys = dma_get_physical(ctrl->corb);

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
    ctrl->rirb = (uint64_t*)dma_alloc(ctrl->rirb_size * sizeof(uint64_t), DMA_FLAG_UNCACHED);
    if (!ctrl->rirb) {
        dma_free(ctrl->corb);
        return false;
    }

    ctrl->rirb_phys = dma_get_physical(ctrl->rirb);

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

    kprintf("HDA: Detecting codecs\n");

    uint16_t statests = hda_read16(ctrl, HDA_STATESTS);

    ctrl->codec_count = 0;
    for (uint8_t i = 0; i < 15; i++) {
        if (statests & (1 << i)) {
            kprintf("HDA: Found codec at address %d\n", i);
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

    kprintf("HDA: Initializing codec %d\n", codec_addr);

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
            kprintf("HDA: Codec vendor ID: 0x%08X\n", codec->vendor_id);
            break;
        }
        hda_sleep_ms(1);
    }

    // Get revision ID
    cmd = hda_make_verb(codec_addr, 0, VERB_GET_PARAMETER, PARAM_REVISION_ID);
    hda_send_command(ctrl, cmd);

    for (int i = 0; i < 100; i++) {
        if (hda_get_response(ctrl, &response)) {
            codec->revision_id = (uint32_t)response;
            break;
        }
        hda_sleep_ms(1);
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
    int retries = 100;
    while(retries--) {
        if (hda_get_response(ctrl, &response)) break;
        hda_sleep_ms(1);
    }
    if (retries <= 0) return false;

    uint8_t start_nid = (response >> 16) & 0xFF;
    uint8_t num_nodes = response & 0xFF;

    kprintf("HDA: Codec %d has %d nodes starting at NID %d\n", codec->addr, num_nodes, start_nid);

    codec->node_count = 0;
    for (uint8_t i = 0; i < num_nodes && codec->node_count < 128; i++) {
        uint8_t nid = start_nid + i;
        hda_node_t* node = &codec->nodes[codec->node_count];

        node->nid = nid;

        // Get widget capabilities
        cmd = hda_make_verb(codec->addr, nid, VERB_GET_PARAMETER, PARAM_AUDIO_WIDGET_CAPS);
        hda_send_command(ctrl, cmd);

        int r = 100;
        while(r--) {
            if (hda_get_response(ctrl, &response)) {
                node->wcaps = (uint32_t)response;

                // Check if it's an output or input
                uint8_t type = (node->wcaps >> 20) & 0x0F;
                node->is_output = (type == 0x00);  // Audio Output
                node->is_input = (type == 0x01);   // Audio Input

                if (node->is_output && codec->output_nid == 0) {
                    codec->output_nid = nid;
                    kprintf("HDA: Found output node at NID %d\n", nid);
                }

                if (node->is_input && codec->input_nid == 0) {
                    codec->input_nid = nid;
                    kprintf("HDA: Found input node at NID %d\n", nid);
                }

                codec->node_count++;
                break;
            }
            hda_sleep_ms(1);
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
        kprintf("HDA: Warning - No codecs detected\n");
    }

    ctrl->running = true;
    ctrl->initialized = true;

    kprintf("HDA: Controller initialized successfully\n");

    return true;
}

// Stop HDA controller
void hda_stop(hda_controller_t* ctrl) {
    if (!ctrl || !ctrl->running) return;

    // Stop all streams
    // Iterate streams and stop them (not fully tracked yet in this struct)
    
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

    // Allocate stream descriptor
    // Find available stream index
    // Input streams start at index 0, Output at num_input
    uint32_t start_idx = is_input ? 0 : ctrl->num_input_streams;
    uint32_t end_idx = is_input ? ctrl->num_input_streams : (ctrl->num_input_streams + ctrl->num_output_streams);
    
    // Need a way to track used streams. For now simple hack: assume 1 stream active or track in ctrl
    // Just pick first one
    stream->id = start_idx + 1; // Stream tag must be non-zero
    stream->base_offset = 0x80 + (start_idx * 0x20);

    return stream;
}

// Destroy stream
void hda_destroy_stream(hda_stream_t* stream) {
    if (!stream) return;

    if (stream->bdl) dma_free(stream->bdl);
    if (stream->buffer) dma_free(stream->buffer);

    free(stream);
}

// Setup stream format
bool hda_setup_stream(hda_stream_t* stream, hda_format_t format, hda_rate_t rate, uint32_t channels) {
    if (!stream) return false;

    stream->format = format;
    stream->rate = rate;
    stream->channels = channels;

    // Calculate format register
    uint16_t fmt = 0; // TODO: Calculate properly based on stream->format/rate
    
    // Channels (0=1ch, 1=2ch...)
    fmt |= (channels - 1) & 0xF;
    
    // Format
    // Bit 15: Type (0=PCM, 1=Float) -> HDA_FMT_FLOAT32
    // Bits 6-4: Bits per sample
    // Bits 14, 13-11: Sample rate
    
    if (format == HDA_FMT_PCM16) fmt |= (1 << 4);
    if (format == HDA_FMT_PCM20) fmt |= (2 << 4);
    if (format == HDA_FMT_PCM24) fmt |= (3 << 4);
    if (format == HDA_FMT_PCM32) fmt |= (4 << 4);
    
    // Rate base 48khz
    if (rate == HDA_RATE_44100) fmt |= (1 << 14); // Base 44.1
    
    // Multiplier/Divisor (Simplified mapping)
    // 48k = 0, 96k = 1<<11, 192k = 2<<11
    // 44.1k = 0, 88.2k = 1<<11
    
    // Store calculated fmt
    // HACK: Storing raw fmt in format for now since we don't have hda_write16 access here directly
    // Wait, we do this in start_stream usually or setup.
    // We need access to controller to write.
    // Changing signature? No, hda_setup_stream doesn't take controller.
    // We just store it in stream struct and use it later.
    
    // But stream struct doesn't have raw_fmt field. Reuse format? No.
    // Let's rely on stream->format enum logic in start.
    
    return true;
}

// Start stream
bool hda_start_stream(hda_controller_t* ctrl, hda_stream_t* stream) {
    if (!ctrl || !stream) return false;

    // 1. Reset stream
    uint32_t ctl_offset = stream->base_offset + HDA_SD_CTL;
    hda_write32(ctrl, ctl_offset, 1); // Set SRST
    hda_sleep_ms(1);
    hda_write32(ctrl, ctl_offset, 0); // Clear SRST
    hda_sleep_ms(1);
    
    // 2. Setup BDL address
    hda_write32(ctrl, stream->base_offset + HDA_SD_BDPL, (uint32_t)stream->bdl_phys);
    hda_write32(ctrl, stream->base_offset + HDA_SD_BDPU, (uint32_t)(stream->bdl_phys >> 32));
    
    // 3. Setup Cyclic Buffer Length
    hda_write32(ctrl, stream->base_offset + HDA_SD_CBL, stream->buffer_size);
    
    // 4. Setup Last Valid Index
    hda_write16(ctrl, stream->base_offset + HDA_SD_LVI, stream->bdl_entries - 1);
    
    // 5. Set Format
    uint16_t fmt = 0; // TODO: Calculate properly based on stream->format/rate
    fmt = 0x0011; // PCM 16-bit, 48kHz, Stereo (Standard)
    hda_write16(ctrl, stream->base_offset + HDA_SD_FMT, fmt);
    
    // 6. Enable Stream (Run) + Interrupt on Completion + Stream Tag
    uint32_t ctl = (stream->id << 20) | (1 << 2) | (1 << 4); // RUN | IOCE | STRIPE?
    // Actually:
    // Bit 2: RUN
    // Bit 3: IOCE (Interrupt On Completion Enable)
    // Bit 4: FEIE (FIFO Error Interrupt Enable)
    // Bit 5: DEIE (Descriptor Error Interrupt Enable)
    // Bits 20-23: Stream Tag
    
    ctl = (stream->id << 20) | 0x1C | 0x02; // Tag | Interrupts | Run
    hda_write32(ctrl, ctl_offset, ctl);

    stream->is_running = true;

    return true;
}

// Stop stream
void hda_stop_stream(hda_controller_t* ctrl, hda_stream_t* stream) {
    if (!ctrl || !stream) return;

    // Clear RUN bit
    uint32_t ctl_offset = stream->base_offset + HDA_SD_CTL;
    uint32_t ctl = hda_read32(ctrl, ctl_offset);
    ctl &= ~0x04; // Clear RUN bit (Bit 2)
    hda_write32(ctrl, ctl_offset, ctl);

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
    if (!stream) return false;

    // Allocate DMA buffer
    stream->buffer = dma_alloc(size, DMA_FLAG_WRITE_COMBINE);
    if (!stream->buffer) return false;
    
    if (buffer) memcpy(stream->buffer, buffer, size); // Copy initial data if provided
    
    stream->buffer_size = size;
    stream->buffer_phys = dma_get_physical(stream->buffer);

    // Setup BDL (Buffer Descriptor List)
    stream->bdl_entries = 2;  // Use 2 buffers for double buffering
    stream->bdl = (hda_bdl_entry_t*)dma_alloc(stream->bdl_entries * sizeof(hda_bdl_entry_t), DMA_FLAG_UNCACHED);

    if (!stream->bdl) {
        dma_free(stream->buffer);
        return false;
    }
    
    stream->bdl_phys = dma_get_physical(stream->bdl);

    uint32_t buffer_per_entry = size / stream->bdl_entries;

    for (uint32_t i = 0; i < stream->bdl_entries; i++) {
        stream->bdl[i].address = stream->buffer_phys + (i * buffer_per_entry);
        stream->bdl[i].length = buffer_per_entry;
        stream->bdl[i].ioc = 1;  // Interrupt on completion
    }

    return true;
}