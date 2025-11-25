/**
 * @file editor.c
 * @brief Text Editor Implementation
 */

#include "editor.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "../../libs/libc/include/syscall.h"
#include "../../libs/libgui/src/font8x8_basic.h"

// Syscall wrappers
static int sys_open(const char* path, int flags) {
    return (int)syscall(SYS_OPEN, (uint64_t)path, flags, 0, 0, 0);
}

static int sys_close(int fd) {
    return (int)syscall(SYS_CLOSE, (uint64_t)fd, 0, 0, 0, 0);
}

static long sys_read(int fd, void* buf, size_t count) {
    return (long)syscall(SYS_READ, (uint64_t)fd, (uint64_t)buf, count, 0, 0);
}

static long sys_write(int fd, const void* buf, size_t count) {
    return (long)syscall(SYS_WRITE, (uint64_t)fd, (uint64_t)buf, count, 0, 0);
}

static void sys_yield() {
    syscall(SYS_YIELD, 0, 0, 0, 0, 0);
}

// Graphics helpers
static void draw_pixel(uint32_t* buffer, int width, int x, int y, uint32_t color) {
    buffer[y * width + x] = color;
}

static void draw_rect(uint32_t* buffer, int width, int x, int y, int w, int h, uint32_t color) {
    for (int j = y; j < y + h; j++) {
        for (int i = x; i < x + w; i++) {
            draw_pixel(buffer, width, i, j, color);
        }
    }
}

static void draw_char(uint32_t* buffer, int width, int x, int y, char c, uint32_t color) {
    if (c < 0 || c > 127) return;
    for (int dy = 0; dy < 8; dy++) {
        for (int dx = 0; dx < 8; dx++) {
            if ((font8x8_basic[(int)c][dy] >> dx) & 1) {
                draw_pixel(buffer, width, x + dx, y + dy, color);
            }
        }
    }
}

static void draw_string(uint32_t* buffer, int width, int x, int y, const char* str, uint32_t color) {
    int cx = x;
    while (*str) {
        draw_char(buffer, width, cx, y, *str, color);
        cx += 8;
        str++;
    }
}

// Language keywords
static const char* c_keywords[] = {
    "auto", "break", "case", "char", "const", "continue", "default", "do",
    "double", "else", "enum", "extern", "float", "for", "goto", "if",
    "int", "long", "register", "return", "short", "signed", "sizeof", "static",
    "struct", "switch", "typedef", "union", "unsigned", "void", "volatile", "while",
    NULL
};

static const char* c_types[] = {
    "uint8_t", "uint16_t", "uint32_t", "uint64_t",
    "int8_t", "int16_t", "int32_t", "int64_t",
    "size_t", "bool", "true", "false", "NULL",
    NULL
};

// Helper: Check if character is a word boundary
static bool is_word_boundary(char ch) {
    return isspace(ch) || ch == '(' || ch == ')' || ch == '{' || ch == '}' ||
           ch == '[' || ch == ']' || ch == ';' || ch == ',' || ch == '.' ||
           ch == ':' || ch == '!' || ch == '&' || ch == '|' || ch == '=' ||
           ch == '+' || ch == '-' || ch == '*' || ch == '/' || ch == '<' || ch == '>';
}

// Helper: Check if word is a keyword
static bool is_keyword(const char* word, const char** keywords) {
    for (int i = 0; keywords[i]; i++) {
        if (strcmp(word, keywords[i]) == 0) {
            return true;
        }
    }
    return false;
}

// Initialize text line
static void init_line(text_line_t* line) {
    line->capacity = 128;
    line->length = 0;
    line->content = (char*)malloc(line->capacity);
    line->tokens = (token_type_t*)malloc(line->capacity * sizeof(token_type_t));
    line->content[0] = '\0';
    line->dirty = true;
}

// Free text line
static void free_line(text_line_t* line) {
    if (line->content) free(line->content);
    if (line->tokens) free(line->tokens);
}

// Ensure line has enough capacity
static void ensure_line_capacity(text_line_t* line, uint32_t required) {
    if (required >= line->capacity) {
        while (line->capacity < required) {
            line->capacity *= 2;
        }
        line->content = (char*)realloc(line->content, line->capacity);
        line->tokens = (token_type_t*)realloc(line->tokens, line->capacity * sizeof(token_type_t));
    }
}

// Create editor
editor_ctx_t* editor_create(compositor_ctx_t* compositor) {
    editor_ctx_t* ctx = (editor_ctx_t*)malloc(sizeof(editor_ctx_t));
    if (!ctx) return NULL;

    memset(ctx, 0, sizeof(editor_ctx_t));

    ctx->compositor = compositor;
    ctx->font_name = "monospace";
    ctx->font_size = 12;
    ctx->char_width = 8;
    ctx->char_height = 16;
    ctx->tab_width = 4;
    ctx->show_line_numbers = true;
    ctx->auto_indent = true;
    ctx->auto_save = false;

    // Create window
    uint32_t width = 1000;
    uint32_t height = 700;
    
    ctx->editor_window = window_create("Text Editor", width, height);
    if (!ctx->editor_window) {
        free(ctx);
        return NULL;
    }

    // Create root panel
    widget_t* root = panel_create();
    widget_set_size(root, width, height);
    ctx->editor_window->root = root;

    // Create menu bar
    ctx->menu_bar = panel_create();
    widget_set_position(ctx->menu_bar, 0, 0);
    widget_set_size(ctx->menu_bar, width, 25);
    widget_set_colors(ctx->menu_bar, 0xFFFFFFFF, 0xFF2C3E50);
    widget_add_child(root, ctx->menu_bar);

    // Create toolbar
    ctx->toolbar = panel_create();
    widget_set_position(ctx->toolbar, 0, 25);
    widget_set_size(ctx->toolbar, width, 35);
    widget_set_colors(ctx->toolbar, 0xFF000000, 0xFFBDC3C7);
    widget_add_child(root, ctx->toolbar);

    // Create tab bar
    ctx->tab_bar = panel_create();
    widget_set_position(ctx->tab_bar, 0, 60);
    widget_set_size(ctx->tab_bar, width, 30);
    widget_set_colors(ctx->tab_bar, 0xFFFFFFFF, 0xFF34495E);
    widget_add_child(root, ctx->tab_bar);

    // Create editor panel
    ctx->editor_panel = panel_create();
    widget_set_position(ctx->editor_panel, 0, 90);
    widget_set_size(ctx->editor_panel, width, height - 120);
    widget_set_colors(ctx->editor_panel, 0xFF000000, 0xFFFFFFFF);
    widget_add_child(root, ctx->editor_panel);

    // Create status bar
    ctx->status_bar = panel_create();
    widget_set_position(ctx->status_bar, 0, height - 30);
    widget_set_size(ctx->status_bar, width, 30);
    widget_set_colors(ctx->status_bar, 0xFFFFFFFF, 0xFF34495E);
    widget_add_child(root, ctx->status_bar);

    // Load themes
    editor_load_themes(ctx);
    ctx->current_theme = &ctx->themes[0];

    // Create first tab
    editor_create_tab(ctx, NULL);

    ctx->running = true;

    return ctx;
}

// Destroy editor
void editor_destroy(editor_ctx_t* ctx) {
    if (!ctx) return;

    // Free all tabs and buffers
    for (uint32_t i = 0; i < ctx->tab_count; i++) {
        if (ctx->tabs[i].buffer) {
            editor_buffer_t* buf = ctx->tabs[i].buffer;
            for (uint32_t j = 0; j < buf->line_count; j++) {
                free_line(&buf->lines[j]);
            }
            free(buf->lines);
            free(buf);
        }
    }

    if (ctx->editor_window) {
        window_destroy(ctx->editor_window);
    }

    free(ctx);
}

// Create new tab
uint32_t editor_create_tab(editor_ctx_t* ctx, const char* file_path) {
    if (!ctx || ctx->tab_count >= MAX_TABS) {
        return 0;
    }

    editor_tab_t* tab = &ctx->tabs[ctx->tab_count];
    tab->id = ctx->tab_count + 1;

    // Create buffer
    tab->buffer = (editor_buffer_t*)malloc(sizeof(editor_buffer_t));
    memset(tab->buffer, 0, sizeof(editor_buffer_t));

    editor_buffer_t* buf = tab->buffer;
    buf->line_capacity = 100;
    buf->lines = (text_line_t*)malloc(buf->line_capacity * sizeof(text_line_t));
    buf->line_count = 1;
    init_line(&buf->lines[0]);

    buf->language = LANG_NONE;
    buf->auto_detect_language = true;

    // Load file or create new
    if (file_path) {
        editor_load_file(buf, file_path);
        strncpy(tab->title, file_path, 63);
        tab->title[63] = '\0';
    } else {
        strcpy(tab->title, "Untitled");
    }

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
void editor_close_tab(editor_ctx_t* ctx, uint32_t tab_id) {
    if (!ctx || ctx->tab_count <= 1) return;

    for (uint32_t i = 0; i < ctx->tab_count; i++) {
        if (ctx->tabs[i].id == tab_id) {
            editor_buffer_t* buf = ctx->tabs[i].buffer;

            // Ask to save if modified
            if (buf->modified) {
                // In a full GUI we would show a dialog here
                // For now we just close
            }

            // Free buffer
            for (uint32_t j = 0; j < buf->line_count; j++) {
                free_line(&buf->lines[j]);
            }
            free(buf->lines);
            free(buf);

            // Remove tab button
            if (ctx->tabs[i].tab_button) {
                widget_remove_child(ctx->tab_bar, ctx->tabs[i].tab_button);
                widget_destroy(ctx->tabs[i].tab_button);
            }

            memmove(&ctx->tabs[i],
                    &ctx->tabs[i + 1],
                    (ctx->tab_count - i - 1) * sizeof(editor_tab_t));
            ctx->tab_count--;

            if (ctx->active_tab >= ctx->tab_count) {
                ctx->active_tab = ctx->tab_count - 1;
            }

            break;
        }
    }
}

// Switch to tab
void editor_switch_tab(editor_ctx_t* ctx, uint32_t tab_id) {
    if (!ctx) return;

    for (uint32_t i = 0; i < ctx->tab_count; i++) {
        if (ctx->tabs[i].id == tab_id) {
            ctx->active_tab = i;
            break;
        }
    }
}

// Load file
bool editor_load_file(editor_buffer_t* buf, const char* file_path) {
    if (!buf || !file_path) return false;

    strncpy(buf->file_path, file_path, 511);
    buf->file_path[511] = '\0';
    
    int fd = sys_open(file_path, 0); // O_RDONLY
    if (fd < 0) return false;

    // Clear existing lines
    for (uint32_t i = 0; i < buf->line_count; i++) {
        free_line(&buf->lines[i]);
    }
    free(buf->lines);
    
    buf->lines = (text_line_t*)malloc(sizeof(text_line_t) * 100);
    buf->line_capacity = 100;
    buf->line_count = 0;

    char buffer[1024];
    long bytes;
    text_line_t current_line;
    init_line(&current_line);

    while ((bytes = sys_read(fd, buffer, sizeof(buffer))) > 0) {
        for (long i = 0; i < bytes; i++) {
            if (buffer[i] == '\n') {
                if (buf->line_count >= buf->line_capacity) {
                    buf->line_capacity *= 2;
                    buf->lines = (text_line_t*)realloc(buf->lines, buf->line_capacity * sizeof(text_line_t));
                }
                buf->lines[buf->line_count++] = current_line;
                init_line(&current_line);
            } else {
                ensure_line_capacity(&current_line, current_line.length + 2);
                current_line.content[current_line.length++] = buffer[i];
                current_line.content[current_line.length] = '\0';
            }
        }
    }
    
    // Add last line if not empty or if file was empty
    if (buf->line_count >= buf->line_capacity) {
        buf->line_capacity *= 2;
        buf->lines = (text_line_t*)realloc(buf->lines, buf->line_capacity * sizeof(text_line_t));
    }
    buf->lines[buf->line_count++] = current_line;

    sys_close(fd);
    
    buf->modified = false;
    editor_detect_language(buf, file_path);

    return true;
}

// Save file
bool editor_save_file(editor_buffer_t* buf, const char* file_path) {
    if (!buf) return false;

    const char* path = file_path ? file_path : buf->file_path;
    if (!path || !path[0]) return false;

    int fd = sys_open(path, 1 | 2 | 0x20); // O_WRONLY | O_CREAT | O_TRUNC
    if (fd < 0) return false;

    for (uint32_t i = 0; i < buf->line_count; i++) {
        text_line_t* line = &buf->lines[i];
        sys_write(fd, line->content, line->length);
        if (i < buf->line_count - 1) {
            sys_write(fd, "\n", 1);
        }
    }

    sys_close(fd);
    buf->modified = false;

    return true;
}

// Insert character at cursor
void editor_insert_char(editor_buffer_t* buf, char ch) {
    if (!buf) return;

    text_line_t* line = &buf->lines[buf->cursor_line];

    ensure_line_capacity(line, line->length + 2);

    if (buf->cursor_column < line->length) {
        memmove(&line->content[buf->cursor_column + 1],
                &line->content[buf->cursor_column],
                line->length - buf->cursor_column);
    }

    line->content[buf->cursor_column] = ch;
    line->length++;
    line->content[line->length] = '\0';
    line->dirty = true;

    buf->cursor_column++;
    buf->modified = true;

    char str[2] = {ch, '\0'};
    editor_record_action(buf, ACTION_INSERT_CHAR, buf->cursor_line, buf->cursor_column - 1, str, 1);
}

// Delete character at cursor
void editor_delete_char(editor_buffer_t* buf) {
    if (!buf) return;

    text_line_t* line = &buf->lines[buf->cursor_line];

    if (buf->cursor_column > 0) {
        char deleted = line->content[buf->cursor_column - 1];

        memmove(&line->content[buf->cursor_column - 1],
                &line->content[buf->cursor_column],
                line->length - buf->cursor_column + 1);

        line->length--;
        line->dirty = true;
        buf->cursor_column--;
        buf->modified = true;

        char str[2] = {deleted, '\0'};
        editor_record_action(buf, ACTION_DELETE_CHAR, buf->cursor_line, buf->cursor_column, str, 1);
    } else if (buf->cursor_line > 0) {
        text_line_t* prev_line = &buf->lines[buf->cursor_line - 1];
        uint32_t prev_len = prev_line->length;

        ensure_line_capacity(prev_line, prev_line->length + line->length + 1);

        memcpy(&prev_line->content[prev_line->length], line->content, line->length + 1);
        prev_line->length += line->length;
        prev_line->dirty = true;

        free_line(line);

        memmove(&buf->lines[buf->cursor_line],
                &buf->lines[buf->cursor_line + 1],
                (buf->line_count - buf->cursor_line - 1) * sizeof(text_line_t));

        buf->line_count--;
        buf->cursor_line--;
        buf->cursor_column = prev_len;
        buf->modified = true;
    }
}

// Insert new line
void editor_insert_line(editor_buffer_t* buf) {
    if (!buf) return;

    if (buf->line_count >= buf->line_capacity) {
        buf->line_capacity *= 2;
        buf->lines = (text_line_t*)realloc(buf->lines, buf->line_capacity * sizeof(text_line_t));
    }

    text_line_t* cur_line = &buf->lines[buf->cursor_line];

    memmove(&buf->lines[buf->cursor_line + 2],
            &buf->lines[buf->cursor_line + 1],
            (buf->line_count - buf->cursor_line - 1) * sizeof(text_line_t));

    text_line_t* new_line = &buf->lines[buf->cursor_line + 1];
    init_line(new_line);

    if (buf->cursor_column < cur_line->length) {
        uint32_t remaining = cur_line->length - buf->cursor_column;
        ensure_line_capacity(new_line, remaining + 1);

        memcpy(new_line->content, &cur_line->content[buf->cursor_column], remaining);
        new_line->content[remaining] = '\0';
        new_line->length = remaining;

        cur_line->content[buf->cursor_column] = '\0';
        cur_line->length = buf->cursor_column;
        cur_line->dirty = true;
    }

    buf->line_count++;
    buf->cursor_line++;
    buf->cursor_column = 0;
    buf->modified = true;

    if (buf->auto_detect_language) {
        editor_auto_indent(buf);
    }
}

// Move cursor
void editor_move_cursor(editor_buffer_t* buf, int32_t dx, int32_t dy) {
    if (!buf) return;

    if (dy < 0 && buf->cursor_line > 0) {
        buf->cursor_line--;
        if (buf->cursor_column > buf->lines[buf->cursor_line].length) {
            buf->cursor_column = buf->lines[buf->cursor_line].length;
        }
    } else if (dy > 0 && buf->cursor_line < buf->line_count - 1) {
        buf->cursor_line++;
        if (buf->cursor_column > buf->lines[buf->cursor_line].length) {
            buf->cursor_column = buf->lines[buf->cursor_line].length;
        }
    }

    if (dx < 0 && buf->cursor_column > 0) {
        buf->cursor_column--;
    } else if (dx > 0 && buf->cursor_column < buf->lines[buf->cursor_line].length) {
        buf->cursor_column++;
    }
}

// Move to line start
void editor_move_to_line_start(editor_buffer_t* buf) {
    if (!buf) return;
    buf->cursor_column = 0;
}

// Move to line end
void editor_move_to_line_end(editor_buffer_t* buf) {
    if (!buf) return;
    buf->cursor_column = buf->lines[buf->cursor_line].length;
}

// Select all
void editor_select_all(editor_buffer_t* buf) {
    if (!buf) return;

    buf->has_selection = true;
    buf->sel_start_line = 0;
    buf->sel_start_column = 0;
    buf->sel_end_line = buf->line_count - 1;
    buf->sel_end_column = buf->lines[buf->line_count - 1].length;
}

// Get selected text
char* editor_get_selection(editor_buffer_t* buf) {
    if (!buf || !buf->has_selection) return NULL;

    uint32_t total_len = 0;
    for (uint32_t i = buf->sel_start_line; i <= buf->sel_end_line; i++) {
        if (i == buf->sel_start_line) {
            total_len += buf->lines[i].length - buf->sel_start_column;
        } else if (i == buf->sel_end_line) {
            total_len += buf->sel_end_column;
        } else {
            total_len += buf->lines[i].length;
        }
        total_len++;  // Newline
    }

    char* result = (char*)malloc(total_len + 1);
    uint32_t pos = 0;

    for (uint32_t i = buf->sel_start_line; i <= buf->sel_end_line; i++) {
        uint32_t start = (i == buf->sel_start_line) ? buf->sel_start_column : 0;
        uint32_t end = (i == buf->sel_end_line) ? buf->sel_end_column : buf->lines[i].length;

        memcpy(&result[pos], &buf->lines[i].content[start], end - start);
        pos += end - start;

        if (i < buf->sel_end_line) {
            result[pos++] = '\n';
        }
    }

    result[pos] = '\0';
    return result;
}

// Record undo action
void editor_record_action(editor_buffer_t* buf, action_type_t type, uint32_t line, uint32_t col, const char* data, uint32_t len) {
    if (!buf || buf->undo_count >= MAX_UNDO_LEVELS) return;

    if (buf->undo_index < buf->undo_count) {
        for (uint32_t i = buf->undo_index; i < buf->undo_count; i++) {
            if (buf->undo_stack[i].data) {
                free(buf->undo_stack[i].data);
            }
        }
        buf->undo_count = buf->undo_index;
    }

    undo_action_t* action = &buf->undo_stack[buf->undo_count];
    action->type = type;
    action->line = line;
    action->column = col;
    action->data = NULL;
    action->data_len = len;

    if (data && len > 0) {
        action->data = (char*)malloc(len + 1);
        memcpy(action->data, data, len);
        action->data[len] = '\0';
    }

    buf->undo_count++;
    buf->undo_index = buf->undo_count;
}

// Detect language from file extension
language_t editor_detect_from_extension(const char* file_path) {
    if (!file_path) return LANG_NONE;

    const char* ext = strrchr(file_path, '.');
    if (!ext) return LANG_NONE;

    ext++;

    if (strcmp(ext, "c") == 0 || strcmp(ext, "h") == 0) return LANG_C;
    if (strcmp(ext, "cpp") == 0 || strcmp(ext, "hpp") == 0 || strcmp(ext, "cc") == 0 || strcmp(ext, "hh") == 0) return LANG_CPP;
    if (strcmp(ext, "rs") == 0) return LANG_RUST;
    if (strcmp(ext, "py") == 0) return LANG_PYTHON;
    if (strcmp(ext, "js") == 0) return LANG_JAVASCRIPT;
    if (strcmp(ext, "html") == 0 || strcmp(ext, "htm") == 0) return LANG_HTML;
    if (strcmp(ext, "css") == 0) return LANG_CSS;
    if (strcmp(ext, "json") == 0) return LANG_JSON;
    if (strcmp(ext, "xml") == 0) return LANG_XML;
    if (strcmp(ext, "md") == 0) return LANG_MARKDOWN;
    if (strcmp(ext, "sh") == 0 || strcmp(ext, "bash") == 0) return LANG_SHELL;
    if (strcmp(ext, "asm") == 0 || strcmp(ext, "S") == 0) return LANG_ASSEMBLY;

    return LANG_NONE;
}

// Detect language
void editor_detect_language(editor_buffer_t* buf, const char* file_path) {
    if (!buf || !buf->auto_detect_language) return;

    buf->language = editor_detect_from_extension(file_path);
}

// Highlight line (C language example)
void editor_highlight_line(editor_buffer_t* buf, uint32_t line_idx) {
    if (!buf || line_idx >= buf->line_count) return;

    text_line_t* line = &buf->lines[line_idx];
    if (!line->dirty && buf->language == LANG_NONE) return;

    for (uint32_t i = 0; i < line->length; i++) {
        line->tokens[i] = TOKEN_NORMAL;
    }

    if (buf->language == LANG_C || buf->language == LANG_CPP) {
        char* content = line->content;
        uint32_t i = 0;

        while (i < line->length) {
            if (isspace(content[i])) {
                i++;
                continue;
            }

            if (content[i] == '/' && i + 1 < line->length) {
                if (content[i + 1] == '/') {
                    for (uint32_t j = i; j < line->length; j++) {
                        line->tokens[j] = TOKEN_COMMENT;
                    }
                    break;
                } else if (content[i + 1] == '*') {
                    line->tokens[i] = TOKEN_COMMENT;
                    line->tokens[i + 1] = TOKEN_COMMENT;
                    i += 2;
                    continue;
                }
            }

            if (content[i] == '#') {
                for (uint32_t j = i; j < line->length; j++) {
                    line->tokens[j] = TOKEN_PREPROCESSOR;
                }
                break;
            }

            if (content[i] == '"' || content[i] == '\'') {
                char quote = content[i];
                line->tokens[i++] = TOKEN_STRING;
                while (i < line->length && content[i] != quote) {
                    if (content[i] == '\\') {
                        line->tokens[i++] = TOKEN_STRING;
                    }
                    if (i < line->length) {
                        line->tokens[i++] = TOKEN_STRING;
                    }
                }
                if (i < line->length) {
                    line->tokens[i++] = TOKEN_STRING;
                }
                continue;
            }

            if (isdigit(content[i])) {
                while (i < line->length && (isdigit(content[i]) || content[i] == '.' || content[i] == 'x' || content[i] == 'X')) {
                    line->tokens[i++] = TOKEN_NUMBER;
                }
                continue;
            }

            if (isalpha(content[i]) || content[i] == '_') {
                uint32_t start = i;
                while (i < line->length && (isalnum(content[i]) || content[i] == '_')) {
                    i++;
                }

                char word[256];
                uint32_t len = i - start;
                if (len < 256) {
                    memcpy(word, &content[start], len);
                    word[len] = '\0';

                    token_type_t token = TOKEN_IDENTIFIER;
                    if (is_keyword(word, c_keywords)) {
                        token = TOKEN_KEYWORD;
                    } else if (is_keyword(word, c_types)) {
                        token = TOKEN_TYPE;
                    }

                    for (uint32_t j = start; j < i; j++) {
                        line->tokens[j] = token;
                    }
                }
                continue;
            }

            if (strchr("+-*/%=<>!&|^~", content[i])) {
                line->tokens[i] = TOKEN_OPERATOR;
            }

            i++;
        }
    }

    line->dirty = false;
}

// Auto-indent
void editor_auto_indent(editor_buffer_t* buf) {
    if (!buf || buf->cursor_line == 0) return;

    text_line_t* prev_line = &buf->lines[buf->cursor_line - 1];
    uint32_t indent = 0;

    while (indent < prev_line->length && prev_line->content[indent] == ' ') {
        indent++;
    }

    if (prev_line->length > 0 && prev_line->content[prev_line->length - 1] == '{') {
        indent += buf->tab_width;
    }

    text_line_t* cur_line = &buf->lines[buf->cursor_line];
    ensure_line_capacity(cur_line, indent + 1);

    memmove(&cur_line->content[indent], cur_line->content, cur_line->length + 1);
    for (uint32_t i = 0; i < indent; i++) {
        cur_line->content[i] = ' ';
    }
    cur_line->length += indent;
    buf->cursor_column = indent;
}

// Load themes
void editor_load_themes(editor_ctx_t* ctx) {
    if (!ctx) return;

    editor_theme_t* theme = &ctx->themes[ctx->theme_count++];
    theme->name = "Light";
    theme->background = 0xFFFFFFFF;
    theme->foreground = 0xFF000000;
    theme->line_number_bg = 0xFFF0F0F0;
    theme->line_number_fg = 0xFF808080;
    theme->cursor_line_bg = 0xFFFFFACD;
    theme->selection_bg = 0xFFB0E0FF;
    theme->keyword_color = 0xFF0000FF;
    theme->type_color = 0xFF008080;
    theme->string_color = 0xFFFF0000;
    theme->number_color = 0xFFFF00FF;
    theme->comment_color = 0xFF008000;
    theme->preprocessor_color = 0xFF800080;
    theme->operator_color = 0xFF000000;
    theme->function_color = 0xFF000080;

    theme = &ctx->themes[ctx->theme_count++];
    theme->name = "Dark";
    theme->background = 0xFF1E1E1E;
    theme->foreground = 0xFFD4D4D4;
    theme->line_number_bg = 0xFF252526;
    theme->line_number_fg = 0xFF858585;
    theme->cursor_line_bg = 0xFF2A2A2A;
    theme->selection_bg = 0xFF264F78;
    theme->keyword_color = 0xFF569CD6;
    theme->type_color = 0xFF4EC9B0;
    theme->string_color = 0xFFCE9178;
    theme->number_color = 0xFFB5CEA8;
    theme->comment_color = 0xFF6A9955;
    theme->preprocessor_color = 0xFFC586C0;
    theme->operator_color = 0xFFD4D4D4;
    theme->function_color = 0xFFDCDCAA;
}

// Set theme
void editor_set_theme(editor_ctx_t* ctx, uint32_t theme_index) {
    if (!ctx || theme_index >= ctx->theme_count) return;

    ctx->current_theme = &ctx->themes[theme_index];
}

// Render editor
void editor_render(editor_ctx_t* ctx) {
    if (!ctx || !ctx->editor_window) return;

    if (ctx->tab_count > 0) {
        editor_tab_t* tab = &ctx->tabs[ctx->active_tab];
        editor_render_buffer(ctx, tab->buffer, 60, 90, ctx->editor_window->width - 70, ctx->editor_window->height - 120);
    }

    window_render(ctx->editor_window);
}

// Render buffer
void editor_render_buffer(editor_ctx_t* ctx, editor_buffer_t* buf, int32_t x, int32_t y, uint32_t width, uint32_t height) {
    if (!ctx || !buf) return;

    uint32_t* fb = (uint32_t*)ctx->editor_window->framebuffer;
    int fb_width = ctx->editor_window->width;

    draw_rect(fb, fb_width, x, y, width, height, ctx->current_theme->background);

    int line_height = ctx->char_height;
    int char_width = ctx->char_width;
    int lines_visible = height / line_height;
    
    if (ctx->show_line_numbers) {
        draw_rect(fb, fb_width, x, y, 50, height, ctx->current_theme->line_number_bg);
    }

    for (int i = 0; i < lines_visible; i++) {
        uint32_t line_idx = buf->scroll_y + i;
        if (line_idx >= buf->line_count) break;

        int ly = y + i * line_height;
        
        if (line_idx == buf->cursor_line) {
            draw_rect(fb, fb_width, x + (ctx->show_line_numbers ? 50 : 0), ly, width - (ctx->show_line_numbers ? 50 : 0), line_height, ctx->current_theme->cursor_line_bg);
        }

        if (ctx->show_line_numbers) {
            char num[16];
            sprintf(num, "%d", line_idx + 1);
            draw_string(fb, fb_width, x + 5, ly + 2, num, ctx->current_theme->line_number_fg);
        }

        text_line_t* line = &buf->lines[line_idx];
        editor_highlight_line(buf, line_idx); 
        
        int lx = x + (ctx->show_line_numbers ? 60 : 5);
        for (uint32_t j = 0; j < line->length; j++) {
            if (j < buf->scroll_x) continue;
            
            uint32_t color = ctx->current_theme->foreground;
            switch (line->tokens[j]) {
                case TOKEN_KEYWORD: color = ctx->current_theme->keyword_color; break;
                case TOKEN_STRING: color = ctx->current_theme->string_color; break;
                case TOKEN_NUMBER: color = ctx->current_theme->number_color; break;
                case TOKEN_COMMENT: color = ctx->current_theme->comment_color; break;
                case TOKEN_TYPE: color = ctx->current_theme->type_color; break;
                case TOKEN_PREPROCESSOR: color = ctx->current_theme->preprocessor_color; break;
                default: break;
            }
            
            draw_char(fb, fb_width, lx + (j - buf->scroll_x) * char_width, ly + 2, line->content[j], color);
        }
    }
    
    int cy = y + (buf->cursor_line - buf->scroll_y) * line_height;
    int cx = x + (ctx->show_line_numbers ? 60 : 5) + (buf->cursor_column - buf->scroll_x) * char_width;
    
    if (cy >= y && cy < y + height) {
        draw_rect(fb, fb_width, cx, cy, 2, line_height, 0xFF000000); // Cursor
    }
}

// Main editor loop
void editor_run(editor_ctx_t* ctx) {
    if (!ctx) return;

    while (ctx->running) {
        // Process events
        // Wait for IPC (placeholder for full event loop)
        
        editor_render(ctx);
        
        sys_yield();
    }
}

// Stub implementations for remaining functions
void editor_delete_line(editor_buffer_t* buf, uint32_t line) {}
void editor_insert_text(editor_buffer_t* buf, const char* text, uint32_t len) {}
void editor_move_cursor_to(editor_buffer_t* buf, uint32_t line, uint32_t column) {}
void editor_move_to_file_start(editor_buffer_t* buf) { buf->cursor_line = 0; buf->cursor_column = 0; }
void editor_move_to_file_end(editor_buffer_t* buf) { buf->cursor_line = buf->line_count - 1; buf->cursor_column = buf->lines[buf->line_count - 1].length; }
void editor_page_up(editor_buffer_t* buf) {}
void editor_page_down(editor_buffer_t* buf) {}
void editor_select(editor_buffer_t* buf, uint32_t start_line, uint32_t start_col, uint32_t end_line, uint32_t end_col) {}
void editor_select_word(editor_buffer_t* buf) {}
void editor_select_line(editor_buffer_t* buf) {}
void editor_clear_selection(editor_buffer_t* buf) { buf->has_selection = false; }
void editor_copy(editor_ctx_t* ctx) {}
void editor_cut(editor_ctx_t* ctx) {}
void editor_paste(editor_ctx_t* ctx) {}
void editor_undo(editor_buffer_t* buf) {}
void editor_redo(editor_buffer_t* buf) {}
void editor_search(editor_ctx_t* ctx, const char* query, bool regex, bool case_sensitive) {}
void editor_find_next(editor_ctx_t* ctx) {}
void editor_find_previous(editor_ctx_t* ctx) {}
void editor_replace_current(editor_ctx_t* ctx, const char* replacement) {}
void editor_replace_all(editor_ctx_t* ctx, const char* replacement) {}
void editor_highlight_all(editor_buffer_t* buf) {}
uint32_t editor_get_indent_level(const char* line) { return 0; }
void editor_toggle_fold(editor_buffer_t* buf, uint32_t line) {}
void editor_toggle_split_view(editor_ctx_t* ctx, bool vertical) {}
void editor_close_split_view(editor_ctx_t* ctx) {}
void editor_render_line_numbers(editor_ctx_t* ctx, editor_buffer_t* buf, int32_t x, int32_t y, uint32_t height) {}
void editor_render_cursor(editor_ctx_t* ctx, editor_buffer_t* buf, int32_t x, int32_t y) {}
void editor_handle_key(editor_ctx_t* ctx, uint32_t keycode, uint32_t modifiers, bool pressed) {}
void editor_handle_char(editor_ctx_t* ctx, uint32_t codepoint) {}
bool editor_save_as(editor_buffer_t* buf, const char* file_path) { return editor_save_file(buf, file_path); }
void editor_new_file(editor_buffer_t* buf) {}