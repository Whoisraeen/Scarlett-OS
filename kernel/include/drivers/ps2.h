/**
 * @file ps2.h
 * @brief PS/2 controller and device interface
 */

#ifndef KERNEL_DRIVERS_PS2_H
#define KERNEL_DRIVERS_PS2_H

#include "../types.h"
#include "../errors.h"

// PS/2 I/O ports
#define PS2_DATA_PORT        0x60
#define PS2_STATUS_PORT      0x64
#define PS2_COMMAND_PORT     0x64

// PS/2 status register bits
#define PS2_STATUS_OUTPUT    0x01  // Output buffer full
#define PS2_STATUS_INPUT     0x02  // Input buffer full
#define PS2_STATUS_SYSTEM    0x04  // System flag
#define PS2_STATUS_CMD_DATA  0x08  // Command/data
#define PS2_STATUS_TIMEOUT   0x40  // Timeout error
#define PS2_STATUS_PARITY    0x80  // Parity error

// PS/2 commands
#define PS2_CMD_READ_CONFIG  0x20
#define PS2_CMD_WRITE_CONFIG 0x60
#define PS2_CMD_DISABLE_PORT2 0xA7
#define PS2_CMD_ENABLE_PORT2  0xA8
#define PS2_CMD_TEST_PORT2    0xA9
#define PS2_CMD_TEST_CONTROLLER 0xAA
#define PS2_CMD_TEST_PORT1    0xAB
#define PS2_CMD_DISABLE_PORT1 0xAD
#define PS2_CMD_ENABLE_PORT1  0xAE

// PS/2 responses
#define PS2_RESP_ACK         0xFA
#define PS2_RESP_RESEND      0xFE
#define PS2_RESP_ERROR       0xFC
#define PS2_RESP_TEST_OK     0x55
#define PS2_RESP_SELF_TEST_OK 0xAA

// PS/2 device types
#define PS2_TYPE_KEYBOARD    0x01
#define PS2_TYPE_MOUSE       0x02
#define PS2_TYPE_MOUSE_SCROLL 0x03
#define PS2_TYPE_MOUSE_5BTN  0x04

// Keyboard scancode sets
#define KEYBOARD_SCANCODE_SET1 1
#define KEYBOARD_SCANCODE_SET2 2
#define KEYBOARD_SCANCODE_SET3 3

// Keyboard function keys
#define KEY_ESCAPE           0x01
#define KEY_ENTER            0x1C
#define KEY_BACKSPACE        0x0E
#define KEY_TAB              0x0F
#define KEY_LEFT_SHIFT       0x2A
#define KEY_RIGHT_SHIFT      0x36
#define KEY_LEFT_CTRL        0x1D
#define KEY_RIGHT_CTRL       0x1D
#define KEY_LEFT_ALT         0x38
#define KEY_RIGHT_ALT        0x38
#define KEY_CAPS_LOCK        0x3A
#define KEY_NUM_LOCK         0x45
#define KEY_SCROLL_LOCK      0x46
#define KEY_F1               0x3B
#define KEY_F12              0x58

// Mouse buttons
#define MOUSE_BUTTON_LEFT    0x01
#define MOUSE_BUTTON_RIGHT   0x02
#define MOUSE_BUTTON_MIDDLE  0x04

// PS/2 controller functions
error_code_t ps2_init(void);
uint8_t ps2_read_data(void);
void ps2_write_data(uint8_t data);
uint8_t ps2_read_status(void);
void ps2_write_command(uint8_t cmd);
error_code_t ps2_wait_output(void);
error_code_t ps2_wait_input(void);
error_code_t ps2_send_byte(uint8_t data);

#endif // KERNEL_DRIVERS_PS2_H

