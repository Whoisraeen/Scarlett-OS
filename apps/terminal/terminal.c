/**
 * @file terminal.c
 * @brief Terminal Emulator Implementation
 */

#include "terminal.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

// Default 16-color ANSI palette
static const uint32_t default_palette_16[16] = {
    0xFF000000, 0xFFAA0000, 0xFF00AA00, 0xFFAAAA00,
    0xFF0000AA, 0xFFAA00AA, 0xFF00AAAA, 0xFFAAAAAA,
    0xFF555555, 0xFFFF5555, 0xFF55FF55, 0xFFFFFF55,
    0xFF5555FF, 0xFFFF55FF, 0xFF55FFFF, 0xFFFFFFFF,
};

// Initialize buffer
static void init_buffer(term_buffer_t* buf, uint32_t cols, uint32_t rows) {
    buf->cols = cols;
    buf->rows = rows;
    buf->cursor_x = 0;
    buf->cursor_y = 0;
    buf->cursor_visible = true;
    buf->scrollback_count = 0;
    buf->scrollback_offset = 0;
    buf->current_fg = 0xFFAAAAAA;  // Light gray
    buf->current_bg = 0xFF000000;  // Black
    buf->current_flags = 0;

    // Clear screen
    terminal_clear_screen(buf);
}

// Initialize pane
static void init_pane(term_pane_t* pane, uint32_t cols, uint32_t rows) {
    pane->ansi_state = ANSI_STATE_NORMAL;
    pane->ansi_param_len = 0;
    pane->shell_pid = 0;

    init_buffer(&pane->buffer, cols, rows);
}

// Create terminal
terminal_ctx_t* terminal_create(compositor_ctx_t* compositor) {
    terminal_ctx_t* ctx = (terminal_ctx_t*)malloc(sizeof(terminal_ctx_t));
    if (!ctx) return NULL;

    memset(ctx, 0, sizeof(terminal_ctx_t));

    ctx->compositor = compositor;

    // Font settings
    ctx->font_name = "monospace";
    ctx->font_size = 12;
    ctx->char_width = 8;
    ctx->char_height = 16;

    // Create window
    uint32_t width = 800;
    uint32_t height = 600;
    int32_t x = (compositor->screen_width - width) / 2;
    int32_t y = (compositor->screen_height - height) / 2;

    ctx->term_window = window_create("Terminal", width, height);
    if (!ctx->term_window) {
        free(ctx);
        return NULL;
    }

    // Create root panel
    widget_t* root = panel_create();
    widget_set_size(root, width, height);
    ctx->term_window->root = root;

    // Create tab bar
    ctx->tab_bar = panel_create();
    widget_set_position(ctx->tab_bar, 0, 0);
    widget_set_size(ctx->tab_bar, width, 30);
    widget_set_colors(ctx->tab_bar, 0xFFFFFFFF, 0xFF2C3E50);
    widget_add_child(root, ctx->tab_bar);

    // Create terminal panel
    ctx->terminal_panel = panel_create();
    widget_set_position(ctx->terminal_panel, 0, 30);
    widget_set_size(ctx->terminal_panel, width, height - 30);
    widget_set_colors(ctx->terminal_panel, 0xFFAAAAAA, 0xFF000000);
    widget_add_child(root, ctx->terminal_panel);

    // Load color schemes
    terminal_load_color_schemes(ctx);
    ctx->current_scheme = &ctx->schemes[0];

    // Create first tab
    terminal_create_tab(ctx, "Terminal");

    ctx->running = true;

    return ctx;
}

// Destroy terminal
void terminal_destroy(terminal_ctx_t* ctx) {
    if (!ctx) return;

    if (ctx->clipboard_text) {
        free(ctx->clipboard_text);
    }

    if (ctx->term_window) {
        window_destroy(ctx->term_window);
    }

    free(ctx);
}

// Create new tab
uint32_t terminal_create_tab(terminal_ctx_t* ctx, const char* title) {
    if (!ctx || ctx->tab_count >= TERM_MAX_TABS) {
        return 0;
    }

    term_tab_t* tab = &ctx->tabs[ctx->tab_count];
    tab->id = ctx->tab_count + 1;

    if (title) {
        strncpy(tab->title, title, 63);
        tab->title[63] = '\0';
    } else {
        snprintf(tab->title, 64, "Tab %u", tab->id);
    }

    // Create first pane
    term_pane_t* pane = &tab->panes[0];
    pane->id = 1;
    init_pane(pane, 80, 24);  // Standard 80x24 terminal

    // Spawn shell in pane
    terminal_spawn_shell(pane);

    tab->pane_count = 1;
    tab->active_pane = 0;

    // Create tab button
    tab->tab_button = button_create(tab->title);
    widget_set_position(tab->tab_button, 10 + (ctx->tab_count * 120), 2);
    widget_set_size(tab->tab_button, 110, 26);
    widget_add_child(ctx->tab_bar, tab->tab_button);

    ctx->tab_count++;
    ctx->active_tab = ctx->tab_count - 1;

    return tab->id;
}

// Close tab
void terminal_close_tab(terminal_ctx_t* ctx, uint32_t tab_id) {
    if (!ctx || ctx->tab_count <= 1) return;  // Don't close last tab

    for (uint32_t i = 0; i < ctx->tab_count; i++) {
        if (ctx->tabs[i].id == tab_id) {
            // TODO: Kill shell processes

            // Remove tab button
            if (ctx->tabs[i].tab_button) {
                widget_remove_child(ctx->tab_bar, ctx->tabs[i].tab_button);
                widget_destroy(ctx->tabs[i].tab_button);
            }

            memmove(&ctx->tabs[i],
                    &ctx->tabs[i + 1],
                    (ctx->tab_count - i - 1) * sizeof(term_tab_t));
            ctx->tab_count--;

            if (ctx->active_tab >= ctx->tab_count) {
                ctx->active_tab = ctx->tab_count - 1;
            }

            break;
        }
    }
}

// Switch to tab
void terminal_switch_tab(terminal_ctx_t* ctx, uint32_t tab_id) {
    if (!ctx) return;

    for (uint32_t i = 0; i < ctx->tab_count; i++) {
        if (ctx->tabs[i].id == tab_id) {
            ctx->active_tab = i;
            break;
        }
    }
}

// Set tab title
void terminal_set_tab_title(terminal_ctx_t* ctx, uint32_t tab_id, const char* title) {
    if (!ctx || !title) return;

    for (uint32_t i = 0; i < ctx->tab_count; i++) {
        if (ctx->tabs[i].id == tab_id) {
            strncpy(ctx->tabs[i].title, title, 63);
            ctx->tabs[i].title[63] = '\0';

            if (ctx->tabs[i].tab_button) {
                button_set_text(ctx->tabs[i].tab_button, title);
            }

            break;
        }
    }
}

// Split pane horizontally
uint32_t terminal_split_pane_horizontal(terminal_ctx_t* ctx, uint32_t pane_id) {
    if (!ctx) return 0;

    term_tab_t* tab = &ctx->tabs[ctx->active_tab];
    if (tab->pane_count >= TERM_MAX_PANES) return 0;

    // TODO: Split existing pane and create new one
    return 0;
}

// Split pane vertically
uint32_t terminal_split_pane_vertical(terminal_ctx_t* ctx, uint32_t pane_id) {
    if (!ctx) return 0;

    term_tab_t* tab = &ctx->tabs[ctx->active_tab];
    if (tab->pane_count >= TERM_MAX_PANES) return 0;

    // TODO: Split existing pane and create new one
    return 0;
}

// Clear screen
void terminal_clear_screen(term_buffer_t* buf) {
    if (!buf) return;

    for (uint32_t y = 0; y < buf->rows; y++) {
        for (uint32_t x = 0; x < buf->cols; x++) {
            buf->cells[y][x].codepoint = ' ';
            buf->cells[y][x].fg_color = buf->current_fg;
            buf->cells[y][x].bg_color = buf->current_bg;
            buf->cells[y][x].flags = 0;
        }
    }
}

// Clear line
void terminal_clear_line(term_buffer_t* buf, uint32_t line) {
    if (!buf || line >= buf->rows) return;

    for (uint32_t x = 0; x < buf->cols; x++) {
        buf->cells[line][x].codepoint = ' ';
        buf->cells[line][x].fg_color = buf->current_fg;
        buf->cells[line][x].bg_color = buf->current_bg;
        buf->cells[line][x].flags = 0;
    }
}

// Scroll up (add line to scrollback)
void terminal_scroll_up(term_buffer_t* buf, uint32_t lines) {
    if (!buf) return;

    for (uint32_t i = 0; i < lines; i++) {
        // Save top line to scrollback
        if (buf->scrollback_count < TERM_SCROLLBACK_LINES) {
            memcpy(buf->scrollback[buf->scrollback_count],
                   buf->cells[0],
                   buf->cols * sizeof(term_cell_t));
            buf->scrollback_count++;
        } else {
            // Scrollback full, shift and overwrite oldest
            memmove(buf->scrollback[0],
                    buf->scrollback[1],
                    (TERM_SCROLLBACK_LINES - 1) * buf->cols * sizeof(term_cell_t));
            memcpy(buf->scrollback[TERM_SCROLLBACK_LINES - 1],
                   buf->cells[0],
                   buf->cols * sizeof(term_cell_t));
        }

        // Scroll screen up
        memmove(buf->cells[0],
                buf->cells[1],
                (buf->rows - 1) * buf->cols * sizeof(term_cell_t));

        // Clear bottom line
        terminal_clear_line(buf, buf->rows - 1);
    }
}

// Set cursor position
void terminal_set_cursor(term_buffer_t* buf, uint32_t x, uint32_t y) {
    if (!buf) return;

    if (x < buf->cols) buf->cursor_x = x;
    if (y < buf->rows) buf->cursor_y = y;
}

// Write character at cursor
void terminal_write_char(term_buffer_t* buf, uint32_t codepoint) {
    if (!buf) return;

    // Handle special characters
    if (codepoint == '\n') {
        buf->cursor_x = 0;
        buf->cursor_y++;
        if (buf->cursor_y >= buf->rows) {
            terminal_scroll_up(buf, 1);
            buf->cursor_y = buf->rows - 1;
        }
        return;
    } else if (codepoint == '\r') {
        buf->cursor_x = 0;
        return;
    } else if (codepoint == '\t') {
        buf->cursor_x = (buf->cursor_x + 8) & ~7;  // Tab to next 8-char boundary
        if (buf->cursor_x >= buf->cols) {
            buf->cursor_x = 0;
            buf->cursor_y++;
        }
        return;
    } else if (codepoint == '\b') {
        if (buf->cursor_x > 0) {
            buf->cursor_x--;
        }
        return;
    }

    // Write character
    if (buf->cursor_x < buf->cols && buf->cursor_y < buf->rows) {
        buf->cells[buf->cursor_y][buf->cursor_x].codepoint = codepoint;
        buf->cells[buf->cursor_y][buf->cursor_x].fg_color = buf->current_fg;
        buf->cells[buf->cursor_y][buf->cursor_x].bg_color = buf->current_bg;
        buf->cells[buf->cursor_y][buf->cursor_x].flags = buf->current_flags;

        buf->cursor_x++;

        if (buf->cursor_x >= buf->cols) {
            buf->cursor_x = 0;
            buf->cursor_y++;

            if (buf->cursor_y >= buf->rows) {
                terminal_scroll_up(buf, 1);
                buf->cursor_y = buf->rows - 1;
            }
        }
    }
}

// Write text
void terminal_write_text(term_buffer_t* buf, const char* text, uint32_t len) {
    if (!buf || !text) return;

    for (uint32_t i = 0; i < len; i++) {
        terminal_write_char(buf, (unsigned char)text[i]);
    }
}

// Process input with ANSI escape sequences
void terminal_process_input(term_pane_t* pane, const char* data, uint32_t len) {
    if (!pane || !data) return;

    term_buffer_t* buf = &pane->buffer;

    for (uint32_t i = 0; i < len; i++) {
        unsigned char ch = data[i];

        switch (pane->ansi_state) {
            case ANSI_STATE_NORMAL:
                if (ch == 0x1B) {  // ESC
                    pane->ansi_state = ANSI_STATE_ESCAPE;
                    pane->ansi_param_len = 0;
                } else {
                    terminal_write_char(buf, ch);
                }
                break;

            case ANSI_STATE_ESCAPE:
                if (ch == '[') {
                    pane->ansi_state = ANSI_STATE_CSI;
                } else if (ch == ']') {
                    pane->ansi_state = ANSI_STATE_OSC;
                } else {
                    // Unknown escape sequence, return to normal
                    pane->ansi_state = ANSI_STATE_NORMAL;
                }
                break;

            case ANSI_STATE_CSI:
                if (isalpha(ch)) {
                    // End of CSI sequence
                    pane->ansi_params[pane->ansi_param_len] = '\0';
                    terminal_handle_csi(buf, pane->ansi_params);
                    pane->ansi_state = ANSI_STATE_NORMAL;
                    pane->ansi_param_len = 0;
                } else if (pane->ansi_param_len < 255) {
                    pane->ansi_params[pane->ansi_param_len++] = ch;
                }
                break;

            case ANSI_STATE_OSC:
                // OSC sequences end with BEL or ESC \
                if (ch == 0x07 || ch == 0x1B) {
                    pane->ansi_state = ANSI_STATE_NORMAL;
                    pane->ansi_param_len = 0;
                }
                break;
        }
    }
}

// Handle CSI (Control Sequence Introducer)
void terminal_handle_csi(term_buffer_t* buf, const char* params) {
    if (!buf || !params) return;

    // Parse parameters
    int args[16] = {0};
    int arg_count = 0;
    const char* p = params;

    while (*p && arg_count < 16) {
        if (isdigit(*p)) {
            args[arg_count] = atoi(p);
            while (isdigit(*p)) p++;
            arg_count++;
        } else {
            p++;
        }
    }

    // Get command character (last char of params)
    char cmd = params[strlen(params) - 1];

    switch (cmd) {
        case 'H':  // Cursor position
        case 'f':
            terminal_set_cursor(buf,
                               (arg_count > 1 ? args[1] - 1 : 0),
                               (arg_count > 0 ? args[0] - 1 : 0));
            break;

        case 'A':  // Cursor up
            if (buf->cursor_y > 0)
                buf->cursor_y -= (args[0] > 0 ? args[0] : 1);
            break;

        case 'B':  // Cursor down
            buf->cursor_y += (args[0] > 0 ? args[0] : 1);
            if (buf->cursor_y >= buf->rows) buf->cursor_y = buf->rows - 1;
            break;

        case 'C':  // Cursor forward
            buf->cursor_x += (args[0] > 0 ? args[0] : 1);
            if (buf->cursor_x >= buf->cols) buf->cursor_x = buf->cols - 1;
            break;

        case 'D':  // Cursor back
            if (buf->cursor_x > 0)
                buf->cursor_x -= (args[0] > 0 ? args[0] : 1);
            break;

        case 'J':  // Erase in display
            if (args[0] == 2) {
                terminal_clear_screen(buf);
                terminal_set_cursor(buf, 0, 0);
            }
            break;

        case 'K':  // Erase in line
            terminal_clear_line(buf, buf->cursor_y);
            break;

        case 'm':  // SGR - Select Graphic Rendition
            terminal_handle_sgr(buf, params);
            break;

        case 's':  // Save cursor position
            buf->saved_x = buf->cursor_x;
            buf->saved_y = buf->cursor_y;
            break;

        case 'u':  // Restore cursor position
            buf->cursor_x = buf->saved_x;
            buf->cursor_y = buf->saved_y;
            break;
    }
}

// Handle SGR (text formatting)
void terminal_handle_sgr(term_buffer_t* buf, const char* params) {
    if (!buf) return;

    // Parse SGR parameters
    int args[16] = {0};
    int arg_count = 0;
    const char* p = params;

    while (*p && arg_count < 16) {
        if (isdigit(*p)) {
            args[arg_count] = atoi(p);
            while (isdigit(*p)) p++;
            arg_count++;
        } else if (*p == ';') {
            p++;
        } else {
            p++;
        }
    }

    if (arg_count == 0) {
        args[0] = 0;
        arg_count = 1;
    }

    for (int i = 0; i < arg_count; i++) {
        int code = args[i];

        if (code == 0) {
            // Reset all attributes
            buf->current_fg = 0xFFAAAAAA;
            buf->current_bg = 0xFF000000;
            buf->current_flags = 0;
        } else if (code == 1) {
            buf->current_flags |= TERM_BOLD;
        } else if (code == 4) {
            buf->current_flags |= TERM_UNDERLINE;
        } else if (code == 7) {
            buf->current_flags |= TERM_REVERSE;
        } else if (code >= 30 && code <= 37) {
            // Foreground color (standard 16 colors)
            buf->current_fg = default_palette_16[code - 30];
        } else if (code >= 40 && code <= 47) {
            // Background color (standard 16 colors)
            buf->current_bg = default_palette_16[code - 40];
        } else if (code == 38 || code == 48) {
            // 256-color or RGB color
            // TODO: Parse extended color codes
        }
    }
}

// Handle keyboard input
void terminal_handle_key(terminal_ctx_t* ctx, uint32_t keycode, uint32_t modifiers, bool pressed) {
    if (!ctx || !pressed) return;

    term_tab_t* tab = &ctx->tabs[ctx->active_tab];
    term_pane_t* pane = &tab->panes[tab->active_pane];

    char seq[16];
    uint32_t seq_len = 0;

    // TODO: Map keycode to appropriate sequence
    // Arrow keys, function keys, etc.

    if (seq_len > 0) {
        terminal_send_to_shell(pane, seq, seq_len);
    }
}

// Handle character input
void terminal_handle_char(terminal_ctx_t* ctx, uint32_t codepoint) {
    if (!ctx) return;

    term_tab_t* tab = &ctx->tabs[ctx->active_tab];
    term_pane_t* pane = &tab->panes[tab->active_pane];

    char utf8[4];
    uint32_t len = 0;

    // Convert codepoint to UTF-8
    if (codepoint < 0x80) {
        utf8[len++] = codepoint;
    } else if (codepoint < 0x800) {
        utf8[len++] = 0xC0 | (codepoint >> 6);
        utf8[len++] = 0x80 | (codepoint & 0x3F);
    } else if (codepoint < 0x10000) {
        utf8[len++] = 0xE0 | (codepoint >> 12);
        utf8[len++] = 0x80 | ((codepoint >> 6) & 0x3F);
        utf8[len++] = 0x80 | (codepoint & 0x3F);
    } else {
        utf8[len++] = 0xF0 | (codepoint >> 18);
        utf8[len++] = 0x80 | ((codepoint >> 12) & 0x3F);
        utf8[len++] = 0x80 | ((codepoint >> 6) & 0x3F);
        utf8[len++] = 0x80 | (codepoint & 0x3F);
    }

    terminal_send_to_shell(pane, utf8, len);

    // Echo character locally
    terminal_write_char(&pane->buffer, codepoint);
}

// Load color schemes
void terminal_load_color_schemes(terminal_ctx_t* ctx) {
    if (!ctx) return;

    // Default scheme
    color_scheme_t* scheme = &ctx->schemes[ctx->scheme_count++];
    scheme->name = "Default";
    scheme->fg_color = 0xFFAAAAAA;
    scheme->bg_color = 0xFF000000;
    scheme->cursor_color = 0xFFFFFFFF;
    memcpy(scheme->palette, default_palette_16, sizeof(default_palette_16));

    // Solarized Dark
    scheme = &ctx->schemes[ctx->scheme_count++];
    scheme->name = "Solarized Dark";
    scheme->fg_color = 0xFF839496;
    scheme->bg_color = 0xFF002B36;
    scheme->cursor_color = 0xFF93A1A1;
}

// Set color scheme
void terminal_set_color_scheme(terminal_ctx_t* ctx, uint32_t scheme_index) {
    if (!ctx || scheme_index >= ctx->scheme_count) return;

    ctx->current_scheme = &ctx->schemes[scheme_index];

    // Update all buffers with new colors
    for (uint32_t i = 0; i < ctx->tab_count; i++) {
        for (uint32_t j = 0; j < ctx->tabs[i].pane_count; j++) {
            term_buffer_t* buf = &ctx->tabs[i].panes[j].buffer;
            buf->current_fg = ctx->current_scheme->fg_color;
            buf->current_bg = ctx->current_scheme->bg_color;
        }
    }
}

// Spawn shell in pane
void terminal_spawn_shell(term_pane_t* pane) {
    if (!pane) return;

    // TODO: Fork and exec shell (e.g., /bin/sh)
    // Connect shell stdin/stdout/stderr to terminal
    pane->shell_pid = 0;  // Placeholder
}

// Send data to shell
void terminal_send_to_shell(term_pane_t* pane, const char* data, uint32_t len) {
    if (!pane || !data) return;

    // TODO: Write data to shell's stdin via pipe
}

// Render terminal
void terminal_render(terminal_ctx_t* ctx) {
    if (!ctx || !ctx->term_window) return;

    term_tab_t* tab = &ctx->tabs[ctx->active_tab];
    term_pane_t* pane = &tab->panes[tab->active_pane];

    terminal_render_pane(ctx, pane);

    window_render(ctx->term_window);
}

// Render pane
void terminal_render_pane(terminal_ctx_t* ctx, term_pane_t* pane) {
    if (!ctx || !pane) return;

    term_buffer_t* buf = &pane->buffer;

    // TODO: Render terminal buffer to framebuffer
    // - Draw each character cell
    // - Apply colors and text attributes
    // - Draw cursor
}

// Render cursor
void terminal_render_cursor(terminal_ctx_t* ctx, term_pane_t* pane) {
    if (!ctx || !pane || !pane->buffer.cursor_visible) return;

    // TODO: Draw cursor at current position
}

// Main terminal loop
void terminal_run(terminal_ctx_t* ctx) {
    if (!ctx) return;

    while (ctx->running) {
        // TODO: Read from shell processes
        // TODO: Process events

        terminal_render(ctx);

        // TODO: Sleep or yield CPU
    }
}
