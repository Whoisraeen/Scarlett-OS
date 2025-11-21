/**
 * @file hda.h
 * @brief Intel High Definition Audio (HDA) Driver
 *
 * User-space driver for Intel HDA audio controllers
 */

#ifndef DRIVERS_AUDIO_HDA_H
#define DRIVERS_AUDIO_HDA_H

#include <stdint.h>
#include <stdbool.h>

// HDA Register Offsets
#define HDA_GCAP 0x00    // Global Capabilities
#define HDA_VMIN 0x02    // Minor Version
#define HDA_VMAJ 0x03    // Major Version
#define HDA_OUTPAY 0x04  // Output Payload Capability
#define HDA_INPAY 0x06   // Input Payload Capability
#define HDA_GCTL 0x08    // Global Control
#define HDA_WAKEEN 0x0C  // Wake Enable
#define HDA_STATESTS 0x0E // State Change Status
#define HDA_INTCTL 0x20  // Interrupt Control
#define HDA_INTSTS 0x24  // Interrupt Status
#define HDA_WALCLK 0x30  // Wall Clock Counter
#define HDA_SSYNC 0x38   // Stream Synchronization
#define HDA_CORBLBASE 0x40 // CORB Lower Base Address
#define HDA_CORBUBASE 0x44 // CORB Upper Base Address
#define HDA_CORBWP 0x48  // CORB Write Pointer
#define HDA_CORBRP 0x4A  // CORB Read Pointer
#define HDA_CORBCTL 0x4C // CORB Control
#define HDA_CORBSTS 0x4D // CORB Status
#define HDA_CORBSIZE 0x4E // CORB Size
#define HDA_RIRBLBASE 0x50 // RIRB Lower Base Address
#define HDA_RIRBUBASE 0x54 // RIRB Upper Base Address
#define HDA_RIRBWP 0x58  // RIRB Write Pointer
#define HDA_RINTCNT 0x5A // Response Interrupt Count
#define HDA_RIRBCTL 0x5C // RIRB Control
#define HDA_RIRBSTS 0x5D // RIRB Status
#define HDA_RIRBSIZE 0x5E // RIRB Size

// Stream Descriptor Registers (per stream)
#define HDA_SD_CTL 0x00      // Control
#define HDA_SD_STS 0x03      // Status
#define HDA_SD_LPIB 0x04     // Link Position in Buffer
#define HDA_SD_CBL 0x08      // Cyclic Buffer Length
#define HDA_SD_LVI 0x0C      // Last Valid Index
#define HDA_SD_FIFOS 0x10    // FIFO Size
#define HDA_SD_FMT 0x12      // Format
#define HDA_SD_BDPL 0x18     // Buffer Descriptor List Pointer Lower
#define HDA_SD_BDPU 0x1C     // Buffer Descriptor List Pointer Upper

// Audio formats
typedef enum {
    HDA_FMT_PCM8 = 0,
    HDA_FMT_PCM16 = 1,
    HDA_FMT_PCM20 = 2,
    HDA_FMT_PCM24 = 3,
    HDA_FMT_PCM32 = 4,
    HDA_FMT_FLOAT32 = 5,
} hda_format_t;

// Sample rates
typedef enum {
    HDA_RATE_8000 = 0,
    HDA_RATE_11025 = 1,
    HDA_RATE_16000 = 2,
    HDA_RATE_22050 = 3,
    HDA_RATE_32000 = 4,
    HDA_RATE_44100 = 5,
    HDA_RATE_48000 = 6,
    HDA_RATE_88200 = 7,
    HDA_RATE_96000 = 8,
    HDA_RATE_176400 = 9,
    HDA_RATE_192000 = 10,
} hda_rate_t;

// Buffer Descriptor List Entry
typedef struct {
    uint64_t address;
    uint32_t length;
    uint32_t ioc;  // Interrupt on completion
} __attribute__((packed)) hda_bdl_entry_t;

// Stream descriptor
typedef struct {
    uint32_t id;
    uint32_t base_offset;
    bool is_input;
    bool is_running;

    // Buffer
    void* buffer;
    uint64_t buffer_phys;
    uint32_t buffer_size;

    // BDL
    hda_bdl_entry_t* bdl;
    uint64_t bdl_phys;
    uint32_t bdl_entries;

    // Format
    hda_format_t format;
    hda_rate_t rate;
    uint32_t channels;
} hda_stream_t;

// Codec node
typedef struct {
    uint8_t nid;  // Node ID
    uint32_t wcaps;  // Widget capabilities
    uint32_t pin_caps;
    uint32_t pin_cfg;
    bool is_output;
    bool is_input;
} hda_node_t;

// Audio codec
typedef struct {
    uint8_t addr;  // Codec address
    uint32_t vendor_id;
    uint32_t revision_id;

    hda_node_t nodes[128];
    uint32_t node_count;

    uint8_t output_nid;  // Default output node
    uint8_t input_nid;   // Default input node
} hda_codec_t;

// HDA controller
typedef struct {
    // PCI device info
    uint32_t vendor_id;
    uint32_t device_id;
    uint8_t bus;
    uint8_t device;
    uint8_t function;

    // Memory-mapped I/O
    void* mmio_base;
    uint64_t mmio_phys;
    uint32_t mmio_size;

    // Capabilities
    uint16_t gcap;
    uint8_t major_version;
    uint8_t minor_version;
    uint8_t num_input_streams;
    uint8_t num_output_streams;
    uint8_t num_bidirectional_streams;

    // CORB/RIRB (Command/Response buffers)
    uint32_t* corb;
    uint64_t corb_phys;
    uint32_t corb_size;

    uint64_t* rirb;
    uint64_t rirb_phys;
    uint32_t rirb_size;

    // Streams
    hda_stream_t* streams;
    uint32_t stream_count;

    // Codecs
    hda_codec_t codecs[15];  // Max 15 codecs
    uint32_t codec_count;

    // State
    bool initialized;
    bool running;
} hda_controller_t;

// HDA controller operations
hda_controller_t* hda_init(uint8_t bus, uint8_t device, uint8_t function);
void hda_destroy(hda_controller_t* ctrl);
bool hda_reset(hda_controller_t* ctrl);
bool hda_start(hda_controller_t* ctrl);
void hda_stop(hda_controller_t* ctrl);

// CORB/RIRB operations
bool hda_init_corb_rirb(hda_controller_t* ctrl);
bool hda_send_command(hda_controller_t* ctrl, uint32_t command);
bool hda_get_response(hda_controller_t* ctrl, uint64_t* response);
uint32_t hda_make_verb(uint8_t codec, uint8_t nid, uint16_t verb, uint16_t payload);

// Codec operations
bool hda_detect_codecs(hda_controller_t* ctrl);
bool hda_init_codec(hda_controller_t* ctrl, uint8_t codec_addr);
bool hda_get_codec_info(hda_controller_t* ctrl, uint8_t codec_addr, hda_codec_t* codec);
bool hda_enumerate_nodes(hda_controller_t* ctrl, hda_codec_t* codec);

// Stream operations
hda_stream_t* hda_create_stream(hda_controller_t* ctrl, bool is_input);
void hda_destroy_stream(hda_stream_t* stream);
bool hda_setup_stream(hda_stream_t* stream, hda_format_t format, hda_rate_t rate, uint32_t channels);
bool hda_start_stream(hda_controller_t* ctrl, hda_stream_t* stream);
void hda_stop_stream(hda_controller_t* ctrl, hda_stream_t* stream);
uint32_t hda_get_stream_position(hda_controller_t* ctrl, hda_stream_t* stream);

// Buffer operations
bool hda_setup_buffer(hda_stream_t* stream, void* buffer, uint32_t size);

// Utility functions
uint32_t hda_read32(hda_controller_t* ctrl, uint32_t offset);
void hda_write32(hda_controller_t* ctrl, uint32_t offset, uint32_t value);
uint16_t hda_read16(hda_controller_t* ctrl, uint32_t offset);
void hda_write16(hda_controller_t* ctrl, uint32_t offset, uint16_t value);
uint8_t hda_read8(hda_controller_t* ctrl, uint32_t offset);
void hda_write8(hda_controller_t* ctrl, uint32_t offset, uint8_t value);

#endif // DRIVERS_AUDIO_HDA_H
