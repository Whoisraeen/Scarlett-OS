/**
 * @file terminal.h
 * @brief Terminal Emulator for Scarlett OS
 *
 * VT100/ANSI compatible terminal with tabs, split panes, and 256-color support
 */

#ifndef APPS_TERMINAL_H
#define APPS_TERMINAL_H

#include <stdint.h>
#include <stdbool.h>
#include "../../gui/compositor/src/compositor.h"
#include "../../gui/widgets/src/widgets.h"

// Terminal configuration
#define TERM_MAX_COLS 256
#define TERM_MAX_ROWS 128
#define TERM_SCROLLBACK_LINES 10000
#define TERM_MAX_TABS 16
#define TERM_MAX_PANES 4

// Character cell
typedef struct {
    uint32_t codepoint;
    uint32_t fg_color;      // RGBA
    uint32_t bg_color;      // RGBA
    uint8_t flags;          // Bold, underline, etc.
} term_cell_t;

// Cell flags
#define TERM_BOLD       0x01
#define TERM_UNDERLINE  0x02
#define TERM_REVERSE    0x04
#define TERM_BLINK      0x08
#define TERM_ITALIC     0x10

// Terminal buffer
typedef struct {
    term_cell_t cells[TERM_MAX_ROWS][TERM_MAX_COLS];
    term_cell_t scrollback[TERM_SCROLLBACK_LINES][TERM_MAX_COLS];
    uint32_t scrollback_count;
    uint32_t scrollback_offset;

    uint32_t cols;
    uint32_t rows;
    uint32_t cursor_x;
    uint32_t cursor_y;
    bool cursor_visible;

    // Current text attributes
    uint32_t current_fg;
    uint32_t current_bg;
    uint8_t current_flags;

    // Saved cursor position (for ANSI sequences)
    uint32_t saved_x;
    uint32_t saved_y;
} term_buffer_t;

// ANSI parser state
typedef enum {
    ANSI_STATE_NORMAL = 0,
    ANSI_STATE_ESCAPE = 1,
    ANSI_STATE_CSI = 2,
    ANSI_STATE_OSC = 3,
} ansi_state_t;

// Terminal pane
typedef struct {
    uint32_t id;
    term_buffer_t buffer;
    uint32_t shell_pid;  // Process ID of shell running in this pane

    // ANSI parser
    ansi_state_t ansi_state;
    char ansi_params[256];
    uint32_t ansi_param_len;

    // Display area (for split panes)
    uint32_t x, y;
    uint32_t width, height;
} term_pane_t;

// Terminal tab
typedef struct {
    uint32_t id;
    char title[64];
    term_pane_t panes[TERM_MAX_PANES];
    uint32_t pane_count;
    uint32_t active_pane;
    widget_t* tab_button;
} term_tab_t;

// Color scheme
typedef struct {
    const char* name;
    uint32_t fg_color;
    uint32_t bg_color;
    uint32_t cursor_color;
    uint32_t palette[256];  // 256-color palette
} color_scheme_t;

// Terminal context
typedef struct {
    compositor_ctx_t* compositor;
    window_t* term_window;

    term_tab_t tabs[TERM_MAX_TABS];
    uint32_t tab_count;
    uint32_t active_tab;

    // Font settings
    const char* font_name;
    uint32_t font_size;
    uint32_t char_width;
    uint32_t char_height;

    // Color scheme
    color_scheme_t* current_scheme;
    color_scheme_t schemes[8];
    uint32_t scheme_count;

    // Clipboard
    char* clipboard_text;
    uint32_t clipboard_len;

    // Search
    char search_query[256];
    bool search_active;
    uint32_t search_matches;

    // Widgets
    widget_t* tab_bar;
    widget_t* terminal_panel;

    bool running;
} terminal_ctx_t;

// Terminal initialization
terminal_ctx_t* terminal_create(compositor_ctx_t* compositor);
void terminal_destroy(terminal_ctx_t* ctx);
void terminal_run(terminal_ctx_t* ctx);

// Tab management
uint32_t terminal_create_tab(terminal_ctx_t* ctx, const char* title);
void terminal_close_tab(terminal_ctx_t* ctx, uint32_t tab_id);
void terminal_switch_tab(terminal_ctx_t* ctx, uint32_t tab_id);
void terminal_set_tab_title(terminal_ctx_t* ctx, uint32_t tab_id, const char* title);

// Pane management (split panes)
uint32_t terminal_split_pane_horizontal(terminal_ctx_t* ctx, uint32_t pane_id);
uint32_t terminal_split_pane_vertical(terminal_ctx_t* ctx, uint32_t pane_id);
void terminal_close_pane(terminal_ctx_t* ctx, uint32_t pane_id);
void terminal_switch_pane(terminal_ctx_t* ctx, uint32_t pane_id);

// Buffer operations
void terminal_clear_screen(term_buffer_t* buf);
void terminal_clear_line(term_buffer_t* buf, uint32_t line);
void terminal_scroll_up(term_buffer_t* buf, uint32_t lines);
void terminal_scroll_down(term_buffer_t* buf, uint32_t lines);
void terminal_set_cursor(term_buffer_t* buf, uint32_t x, uint32_t y);
void terminal_write_char(term_buffer_t* buf, uint32_t codepoint);
void terminal_write_text(term_buffer_t* buf, const char* text, uint32_t len);

// ANSI escape sequence handling
void terminal_process_input(term_pane_t* pane, const char* data, uint32_t len);
void terminal_handle_csi(term_buffer_t* buf, const char* params);
void terminal_handle_sgr(term_buffer_t* buf, const char* params);  // Select Graphic Rendition

// Input handling
void terminal_handle_key(terminal_ctx_t* ctx, uint32_t keycode, uint32_t modifiers, bool pressed);
void terminal_handle_char(terminal_ctx_t* ctx, uint32_t codepoint);
void terminal_paste_clipboard(terminal_ctx_t* ctx);

// Clipboard operations
void terminal_copy_selection(terminal_ctx_t* ctx);

// Search
void terminal_start_search(terminal_ctx_t* ctx, const char* query);
void terminal_stop_search(terminal_ctx_t* ctx);
void terminal_find_next(terminal_ctx_t* ctx);
void terminal_find_previous(terminal_ctx_t* ctx);

// Color schemes
void terminal_load_color_schemes(terminal_ctx_t* ctx);
void terminal_set_color_scheme(terminal_ctx_t* ctx, uint32_t scheme_index);
color_scheme_t* terminal_get_scheme_by_name(terminal_ctx_t* ctx, const char* name);

// Font management
void terminal_set_font(terminal_ctx_t* ctx, const char* font_name, uint32_t size);

// Rendering
void terminal_render(terminal_ctx_t* ctx);
void terminal_render_pane(terminal_ctx_t* ctx, term_pane_t* pane);
void terminal_render_cursor(terminal_ctx_t* ctx, term_pane_t* pane);

// Shell integration
void terminal_spawn_shell(term_pane_t* pane);
void terminal_send_to_shell(term_pane_t* pane, const char* data, uint32_t len);

#endif // APPS_TERMINAL_H
