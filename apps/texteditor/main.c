/**
 * @file main.c
 * @brief Text Editor Entry Point
 */

#include "texteditor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../libs/libc/include/syscall.h" // For syscall wrappers
#include "../../libs/libgui/include/compositor_ipc.h" // For compositor_connect/disconnect

// Syscall wrappers (copied from desktop/editor for consistency)
static int sys_open(const char* path, int flags) {
    return (int)syscall(SYS_OPEN, (uint64_t)path, (uint64_t)flags, 0, 0, 0);
}

static int sys_close(int fd) {
    return (int)syscall(SYS_CLOSE, (uint64_t)fd, 0, 0, 0, 0);
}

static long sys_read(int fd, void* buf, size_t count) {
    return (long)syscall(SYS_READ, (uint64_t)fd, (uint64_t)buf, (uint64_t)count, 0, 0);
}

static int sys_ipc_receive(uint64_t port, ipc_message_t* msg) {
    return (int)syscall(SYS_IPC_RECEIVE, port, (uint64_t)msg, 0, 0, 0);
}

static uint64_t sys_ipc_create_port(void) {
    return syscall(SYS_IPC_CREATE_PORT, 0, 0, 0, 0, 0);
}

static void sys_set_process_ipc_port(uint64_t port) {
    syscall(SYS_SET_PROCESS_IPC_PORT, port, 0, 0, 0, 0);
}

static void sys_yield(void) {
    syscall(SYS_YIELD, 0, 0, 0, 0, 0);
}

// O_RDONLY, O_WRONLY, O_CREAT, O_TRUNC (from fcntl.h typically)
#ifndef O_RDONLY
#define O_RDONLY 0
#endif
#ifndef O_WRONLY
#define O_WRONLY 1
#endif
#ifndef O_CREAT
#define O_CREAT 0x40 // Assuming standard-ish
#endif
#ifndef O_TRUNC
#define O_TRUNC 0x200
#endif


// ============================================================================ 
// Text Editor Context Management
// ============================================================================ 

texteditor_ctx_t* texteditor_create(compositor_ctx_t* compositor) {
    texteditor_ctx_t* ctx = (texteditor_ctx_t*)calloc(1, sizeof(texteditor_ctx_t));
    if (!ctx) return NULL;

    ctx->compositor = compositor;
    
    // Query screen dimensions from compositor
    uint32_t screen_width = 1920; // Default if compositor not ready
    uint32_t screen_height = 1080;
    compositor_get_screen_info(&screen_width, &screen_height);
    
    // Create window
    uint32_t width = 1000;
    uint32_t height = 700;
    int32_t x = (screen_width - width) / 2;
    int32_t y = (screen_height - height) / 2;

    ctx->editor_window = window_create("Text Editor", width, height);
    if (!ctx->editor_window) {
        free(ctx);
        return NULL;
    }
    
    // Initialize theme colors
    ctx->bg_color = 0xFF1E1E1E;  // Dark background
    ctx->fg_color = 0xFFD4D4D4;  // Light text
    ctx->line_number_color = 0xFF858585;
    ctx->current_line_color = 0xFF2A2A2A;
    ctx->selection_color = 0xFF264F78;
    
    // Token colors
    ctx->token_colors[TOKEN_NORMAL] = 0xFFD4D4D4;
    ctx->token_colors[TOKEN_KEYWORD] = 0xFF569CD6;  // Blue
    ctx->token_colors[TOKEN_TYPE] = 0xFF4EC9B0;     // Cyan
    ctx->token_colors[TOKEN_STRING] = 0xFFCE9178;   // Orange
    ctx->token_colors[TOKEN_COMMENT] = 0xFF6A9955;  // Green
    ctx->token_colors[TOKEN_NUMBER] = 0xFFB5CEA8;   // Light green
    ctx->token_colors[TOKEN_OPERATOR] = 0xFFD4D4D4;
    ctx->token_colors[TOKEN_PREPROCESSOR] = 0xFFC586C0;  // Pink
    ctx->token_colors[TOKEN_FUNCTION] = 0xFFDCDCAA;      // Yellow
    
    // Font settings
    ctx->font_name = "monospace";
    ctx->font_size = 14;
    ctx->char_width = 8;
    ctx->char_height = 16;
    
    // Load language definitions
    texteditor_load_languages(ctx);
    
    // Create first tab
    texteditor_create_tab(ctx, "Untitled");
    
    ctx->running = true;
    return ctx;
}

void texteditor_destroy(texteditor_ctx_t* ctx) {
    if (!ctx) return;
    for (uint32_t i = 0; i < ctx->tab_count; i++) {
        editor_buffer_destroy(&ctx->tabs[i].buffer);
        if (ctx->tabs[i].split_buffer) {
            editor_buffer_destroy(ctx->tabs[i].split_buffer);
            free(ctx->tabs[i].split_buffer);
        }
    }
    free(ctx);
}

// ============================================================================ 
// Tab Management
// ============================================================================ 

uint32_t texteditor_create_tab(texteditor_ctx_t* ctx, const char* title) {
    if (ctx->tab_count >= MAX_TABS) return 0xFFFFFFFF;
    
    editor_tab_t* tab = &ctx->tabs[ctx->tab_count];
    tab->id = ctx->tab_count;
    strncpy(tab->title, title, sizeof(tab->title) - 1);
    tab->title[sizeof(tab->title) - 1] = '\0';
    tab->split_view = false;
    tab->split_buffer = NULL;
    
    editor_buffer_init(&tab->buffer);
    
    ctx->active_tab = ctx->tab_count;
    ctx->tab_count++;
    
    return tab->id;
}

void texteditor_close_tab(texteditor_ctx_t* ctx, uint32_t tab_id) {
    if (tab_id >= ctx->tab_count) return;
    
    editor_buffer_destroy(&ctx->tabs[tab_id].buffer);
    
    // Shift tabs down
    for (uint32_t i = tab_id; i < ctx->tab_count - 1; i++) {
        ctx->tabs[i] = ctx->tabs[i + 1];
        ctx->tabs[i].id = i;
    }
    
    ctx->tab_count--;
    
    if (ctx->active_tab >= ctx->tab_count && ctx->tab_count > 0) {
        ctx->active_tab = ctx->tab_count - 1;
    }
}

void texteditor_switch_tab(texteditor_ctx_t* ctx, uint32_t tab_id) {
    if (tab_id < ctx->tab_count) {
        ctx->active_tab = tab_id;
    }
}

// ============================================================================ 
// File Operations
// ============================================================================ 

bool texteditor_open_file(texteditor_ctx_t* ctx, const char* path) {
    int fd = sys_open(path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "Failed to open file: %s\n", path);
        return false;
    }
    
    editor_buffer_t* buf = &ctx->tabs[ctx->active_tab].buffer;
    
    // Clear existing buffer
    editor_buffer_destroy(buf);
    editor_buffer_init(buf);
    
    // Read file
    char line_buf[MAX_LINE_LENGTH];
    long bytes_read;
    uint32_t line_len = 0;

    char file_chunk[1024];
    while ((bytes_read = sys_read(fd, file_chunk, sizeof(file_chunk))) > 0) {
        for (long i = 0; i < bytes_read; i++) {
            if (file_chunk[i] == '\n') {
                line_buf[line_len] = '\0';
                if (buf->line_count > 0 || line_len > 0) { // Don't add empty line if first line is empty
                    editor_insert_line(buf);
                }
                editor_insert_text(buf, line_buf, line_len);
                line_len = 0;
            } else {
                if (line_len < MAX_LINE_LENGTH - 1) {
                    line_buf[line_len++] = file_chunk[i];
                }
            }
        }
    }
    // Add last line
    line_buf[line_len] = '\0';
    if (buf->line_count > 0 || line_len > 0) { // Don't add empty line if first line is empty
        editor_insert_line(buf);
    }
    editor_insert_text(buf, line_buf, line_len);

    sys_close(fd);
    
    strncpy(buf->file_path, path, sizeof(buf->file_path) - 1);
    buf->file_path[sizeof(buf->file_path) - 1] = '\0';
    buf->modified = false;
    
    // Detect and set language
    buf->language = editor_detect_language(ctx, path);
    if (buf->language) {
        editor_highlight_all(buf);
    }
    
    // Update tab title
    const char* filename = strrchr(path, '/');
    if (!filename) filename = strrchr(path, '\\');
    if (filename) filename++;
    else filename = path;
    
    strncpy(ctx->tabs[ctx->active_tab].title, filename, sizeof(ctx->tabs[ctx->active_tab].title) - 1);
    ctx->tabs[ctx->active_tab].title[sizeof(ctx->tabs[ctx->active_tab].title) - 1] = '\0';
    
    return true;
}

bool texteditor_save_file(texteditor_ctx_t* ctx) {
    editor_buffer_t* buf = &ctx->tabs[ctx->active_tab].buffer;
    
    if (buf->file_path[0] == '\0') {
        // No file path, need to prompt for save as
        fprintf(stderr, "Cannot save: no file path specified. Use Save As.\n");
        return false;
    }
    
    int fd = sys_open(buf->file_path, O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0) {
        fprintf(stderr, "Failed to open file for writing: %s\n", buf->file_path);
        return false;
    }
    
    for (uint32_t i = 0; i < buf->line_count; i++) {
        sys_write(fd, buf->lines[i].text, buf->lines[i].length);
        if (i < buf->line_count - 1) {
            sys_write(fd, "\n", 1);
        }
    }
    
    sys_close(fd);
    buf->modified = false;
    
    return true;
}

bool texteditor_save_file_as(texteditor_ctx_t* ctx, const char* path) {
    editor_buffer_t* buf = &ctx->tabs[ctx->active_tab].buffer;
    strncpy(buf->file_path, path, sizeof(buf->file_path) - 1);
    buf->file_path[sizeof(buf->file_path) - 1] = '\0';
    return texteditor_save_file(ctx);
}

void texteditor_new_file(texteditor_ctx_t* ctx) {
    texteditor_create_tab(ctx, "Untitled");
}

// ============================================================================ 
// Clipboard Operations
// ============================================================================ 

void texteditor_copy(texteditor_ctx_t* ctx) {
    editor_buffer_t* buf = &ctx->tabs[ctx->active_tab].buffer;
    char* text = editor_get_selected_text(buf);
    if (text) {
        // Copy to system clipboard (placeholder for IPC to clipboard service)
        printf("TEXTEDITOR: Copied to clipboard: '%s'\n", text);
        free(text);
    }
}

void texteditor_cut(texteditor_ctx_t* ctx) {
    texteditor_copy(ctx);
    editor_buffer_t* buf = &ctx->tabs[ctx->active_tab].buffer;
    editor_delete_selection(buf);
}

void texteditor_paste(texteditor_ctx_t* ctx) {
    // Paste from system clipboard (placeholder for IPC from clipboard service)
    printf("TEXTEDITOR: Pasting from clipboard (not implemented)\n");
    // char* text = get_text_from_clipboard_service();
    // if (text) {
    //    editor_insert_text(buf, text, strlen(text));
    //    free(text);
    // }
}

// ============================================================================ 
// Language Detection
// ============================================================================ 

language_def_t* editor_detect_language(texteditor_ctx_t* ctx, const char* filename) {
    const char* ext = strrchr(filename, '.');
    if (!ext) return NULL;
    
    for (uint32_t i = 0; i < ctx->language_count; i++) {
        language_def_t* lang = &ctx->languages[i];
        for (uint32_t j = 0; j < lang->extension_count; j++) {
            if (strcmp(ext, lang->file_extensions[j]) == 0) {
                return lang;
            }
        }
    }
    
    return NULL;
}

void texteditor_load_languages(texteditor_ctx_t* ctx) {
    ctx->language_count = 0;
    
    // Add C language
    texteditor_add_language(ctx, "C",
                           c_keywords, sizeof(c_keywords) / sizeof(c_keywords[0]),
                           c_types, sizeof(c_types) / sizeof(c_types[0]),
                           "//", "/*", "*/",
                           c_extensions, sizeof(c_extensions) / sizeof(c_extensions[0]));
}

void texteditor_add_language(texteditor_ctx_t* ctx, const char* name,
                             const char** keywords, uint32_t kw_count,
                             const char** types, uint32_t type_count,
                             const char* line_comment,
                             const char* block_start, const char* block_end,
                             const char** extensions, uint32_t ext_count) {
    if (ctx->language_count >= MAX_LANGUAGES) return;
    
    language_def_t* lang = &ctx->languages[ctx->language_count++];
    lang->name = name;
    lang->keywords = keywords;
    lang->keyword_count = kw_count;
    lang->types = types;
    lang->type_count = type_count;
    lang->line_comment = line_comment;
    lang->block_comment_start = block_start;
    lang->block_comment_end = block_end;
    lang->file_extensions = extensions;
    lang->extension_count = ext_count;
}

// ============================================================================ 
// Undo/Redo Operations (Placeholder)
// ============================================================================ 

void texteditor_undo(texteditor_ctx_t* ctx) {
    printf("TEXTEDITOR: Undo (not implemented)\n");
}

void texteditor_redo(texteditor_ctx_t* ctx) {
    printf("TEXTEDITOR: Redo (not implemented)\n");
}


// ============================================================================ 
// Main Entry Point
// ============================================================================ 

int main(int argc, char** argv) {
    // Connect to compositor
    uint64_t compositor_port_id = compositor_connect();
    if (compositor_port_id == 0) {
        fprintf(stderr, "Failed to connect to compositor\n");
        return 1;
    }
    
    // Create text editor
    texteditor_ctx_t* editor = texteditor_create(NULL);
    if (!editor) {
        fprintf(stderr, "Failed to create text editor context\n");
        compositor_disconnect();
        return 1;
    }
    
    // Create and register IPC port for text editor service
    uint64_t editor_port_id = sys_ipc_create_port();
    if (editor_port_id == 0) {
        fprintf(stderr, "Failed to create editor IPC port\n");
        texteditor_destroy(editor);
        compositor_disconnect();
        return 1;
    }
    sys_set_process_ipc_port(editor_port_id);
    printf("Text Editor running on port %lu...\n", editor_port_id);

    // Show editor window
    window_show(editor->editor_window);

    // Open file if specified
    if (argc > 1) {
        texteditor_open_file(editor, argv[1]);
    }
    
    // Run editor
    printf("Text Editor initialized\n");
    printf("Tab count: %u\n", editor->tab_count);
    printf("Active tab: %u\n", editor->active_tab);

    ipc_message_t msg;
    while (editor->running) {
        // Process IPC messages (e.g., from compositor for input events)
        if (sys_ipc_receive(editor_port_id, &msg) == 0) {
            // Handle compositor input events
            if (msg.msg_id == 100) { // MOUSE_BUTTON_EVENT
                // int32_t x = *(int32_t*)&msg.inline_data[2];
                // int32_t y = *(int32_t*)&msg.inline_data[6];
                // uint32_t button = *(uint32_t*)&msg.inline_data[0];
                // bool pressed = (bool)msg.inline_data[4];
                // widget_handle_mouse_button(editor->editor_window->root, x, y, pressed);
            } else if (msg.msg_id == 101) { // KEYBOARD_EVENT
                // uint32_t keycode = *(uint32_t*)&msg.inline_data[0];
                // bool pressed = (bool)msg.inline_data[4];
                // editor_handle_key(editor, keycode, 0, pressed);
            }
        }
        // Render the editor window
        window_render(editor->editor_window);
        sys_yield(); // Yield CPU
    }
    
    // Cleanup
    texteditor_destroy(editor);
    compositor_disconnect();
    
    return 0;
}