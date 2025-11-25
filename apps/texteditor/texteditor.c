/**
 * @file texteditor.c
 * @brief Advanced Text Editor Implementation
 */

#include "texteditor.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "../../libs/libc/include/syscall.h" // For syscall wrappers
#include "../../libs/libgui/src/font8x8_basic.h" // For font data

// Global text editor context for callbacks (if needed)
static texteditor_ctx_t* g_texteditor_ctx = NULL;

// C language keywords
static const char* c_keywords[] = {
    "auto", "break", "case", "char", "const", "continue", "default", "do",
    "double", "else", "enum", "extern", "float", "for", "goto", "if",
    "inline", "int", "long", "register", "restrict", "return", "short", "signed",
    "sizeof", "static", "struct", "switch", "typedef", "union", "unsigned", "void",
    "volatile", "while", "_Bool", "_Complex", "_Imaginary", NULL
};

static const char* c_types[] = {
    "int8_t", "int16_t", "int32_t", "int64_t",
    "uint8_t", "uint16_t", "uint32_t", "uint64_t",
    "size_t", "ssize_t", "ptrdiff_t", "intptr_t", "uintptr_t",
    "bool", "true", "false", "NULL", NULL
};

static const char* c_extensions[] = {".c", ".h", NULL};

// Syscall wrappers (copied for consistency)
static int sys_open(const char* path, int flags) {
    return (int)syscall(SYS_OPEN, (uint64_t)path, (uint64_t)flags, 0, 0, 0);
}

static int sys_close(int fd) {
    return (int)syscall(SYS_CLOSE, (uint64_t)fd, 0, 0, 0, 0);
}

static long sys_read(int fd, void* buf, size_t count) {
    return (long)syscall(SYS_READ, (uint64_t)fd, (uint64_t)buf, (uint64_t)count, 0, 0);
}

static long sys_write(int fd, const void* buf, size_t count) {
    return (long)syscall(SYS_WRITE, (uint64_t)fd, (uint64_t)buf, count, 0, 0);
}

static void sys_yield(void) {
    syscall(SYS_YIELD, 0, 0, 0, 0, 0);
}

static uint64_t sys_get_uptime_ms(void) {
    return syscall(SYS_GET_UPTIME_MS, 0, 0, 0, 0, 0);
}


// Graphics helpers (copied from editor for consistency)
static void draw_rect(uint32_t* buffer, int width, int x, int y, int w, int h, uint32_t color) {
    for (int j = y; j < y + h; j++) {
        for (int i = x; i < x + w; i++) {
            if (i >= 0 && i < width && j >= 0) { // Simple bounds check
                buffer[j * width + i] = color;
            }
        }
    }
}

static void draw_char_editor(uint32_t* buffer, int width, int x, int y, char c, uint32_t color) {
    if (c < 0 || c > 127) return;
    for (int dy = 0; dy < 8; dy++) {
        for (int dx = 0; dx < 8; dx++) {
            if ((font8x8_basic[(int)c][dy] >> dx) & 1) {
                if (x + dx >= 0 && x + dx < width && y + dy >= 0) { // Simple bounds check
                    buffer[(y + dy) * width + (x + dx)] = color;
                }
            }
        }
    }
}

static void draw_string_editor(uint32_t* buffer, int width, int x, int y, const char* str, uint32_t color) {
    int cx = x;
    while (*str) {
        draw_char_editor(buffer, width, cx, y, *str, color);
        cx += 8;
        str++;
    }
}


// ============================================================================ 
// Buffer Management
// ============================================================================ 

void editor_buffer_init(editor_buffer_t* buf) {
    buf->line_capacity = 1024;
    buf->lines = (editor_line_t*)calloc(buf->line_capacity, sizeof(editor_line_t));
    buf->line_count = 1;
    
    // Initialize first line
    buf->lines[0].capacity = 128;
    buf->lines[0].text = (char*)calloc(buf->lines[0].capacity, 1);
    buf->lines[0].length = 0;
    buf->lines[0].tokens = (syntax_token_t*)calloc(buf->lines[0].capacity, sizeof(syntax_token_t));
    buf->lines[0].token_count = 0;
    buf->lines[0].folded = false;
    buf->lines[0].fold_level = 0;
    
    buf->cursor_line = 0;
    buf->cursor_column = 0;
    buf->scroll_line = 0;
    buf->scroll_column = 0;
    
    buf->selection.active = false;
    buf->undo_count = 0;
    buf->undo_position = 0; // Where next undo/redo will happen
    
    buf->file_path[0] = '\0';
    buf->modified = false;
    buf->read_only = false;
    buf->language = NULL;
    
    buf->show_line_numbers = true;
    buf->auto_indent = true;
    buf->highlight_current_line = true;
    buf->tab_size = 4;
    buf->use_spaces_for_tabs = true;
}

void editor_buffer_destroy(editor_buffer_t* buf) {
    for (uint32_t i = 0; i < buf->line_count; i++) {
        free(buf->lines[i].text);
        free(buf->lines[i].tokens);
    }
    free(buf->lines);

    // Free undo/redo data
    for (uint32_t i = 0; i < buf->undo_count; i++) {
        if (buf->undo_stack[i].old_text) {
            free(buf->undo_stack[i].old_text);
        }
        if (buf->undo_stack[i].new_text) {
            free(buf->undo_stack[i].new_text);
        }
    }
}

void editor_insert_char(editor_buffer_t* buf, char c) {
    if (buf->read_only) return;
    
    editor_line_t* line = &buf->lines[buf->cursor_line];
    
    if (line->length + 1 >= line->capacity) {
        line->capacity *= 2;
        line->text = (char*)realloc(line->text, line->capacity);
        line->tokens = (syntax_token_t*)realloc(line->tokens, line->capacity * sizeof(syntax_token_t));
    }
    
    memmove(&line->text[buf->cursor_column + 1],
            &line->text[buf->cursor_column],
            line->length - buf->cursor_column);
    line->text[buf->cursor_column] = c;
    line->length++;
    line->text[line->length] = '\0';
    
    buf->cursor_column++;
    buf->modified = true;
    
    editor_push_undo(buf, ACTION_INSERT_CHAR, buf->cursor_line, buf->cursor_column - 1, (char*)&c, 1, NULL, 0);
    
    if (buf->language) {
        editor_highlight_line(buf, buf->cursor_line);
    }
}

void editor_delete_char(editor_buffer_t* buf) {
    if (buf->read_only) return;
    if (buf->cursor_column == 0 && buf->cursor_line == 0) return;
    
    editor_line_t* line = &buf->lines[buf->cursor_line];
    
    if (buf->cursor_column > 0) {
        char deleted_char = line->text[buf->cursor_column - 1];
        memmove(&line->text[buf->cursor_column - 1],
                &line->text[buf->cursor_column],
                line->length - buf->cursor_column + 1);
        line->length--;
        buf->cursor_column--;
        buf->modified = true;
        
        editor_push_undo(buf, ACTION_DELETE_CHAR, buf->cursor_line, buf->cursor_column, (char*)&deleted_char, 1, NULL, 0);

        if (buf->language) {
            editor_highlight_line(buf, buf->cursor_line);
        }
    } else { // At start of line, merge with previous
        if (buf->cursor_line > 0) {
            editor_line_t* prev_line = &buf->lines[buf->cursor_line - 1];
            uint32_t prev_len = prev_line->length;
            
            // Store content of current line (which is about to be merged) for undo
            char* deleted_line_content = strdup(line->text);
            uint32_t deleted_line_len = line->length;

            while (prev_line->length + line->length + 1 >= prev_line->capacity) {
                prev_line->capacity *= 2;
                prev_line->text = (char*)realloc(prev_line->text, prev_line->capacity);
                prev_line->tokens = (syntax_token_t*)realloc(prev_line->tokens, prev_line->capacity * sizeof(syntax_token_t));
            }
            
            memcpy(&prev_line->text[prev_line->length], line->text, line->length + 1);
            prev_line->length += line->length;
            
            editor_delete_line(buf, buf->cursor_line); // This will shift lines up
            
            buf->cursor_line--;
            buf->cursor_column = prev_len;
            buf->modified = true;
            
            editor_push_undo(buf, ACTION_DELETE_LINE, buf->cursor_line + 1, 0, deleted_line_content, deleted_line_len, NULL, 0);
            free(deleted_line_content);
        }
    }
}

void editor_insert_line(editor_buffer_t* buf) {
    if (buf->read_only) return;
    
    if (buf->line_count + 1 >= buf->line_capacity) {
        buf->line_capacity *= 2;
        buf->lines = (editor_line_t*)realloc(buf->lines, buf->line_capacity * sizeof(editor_line_t));
    }
    
    memmove(&buf->lines[buf->cursor_line + 2],
            &buf->lines[buf->cursor_line + 1],
            (buf->line_count - buf->cursor_line - 1) * sizeof(editor_line_t));
    
    editor_line_t* current = &buf->lines[buf->cursor_line];
    editor_line_t* new_line = &buf->lines[buf->cursor_line + 1];
    
    new_line->capacity = 128;
    new_line->text = (char*)calloc(new_line->capacity, 1);
    new_line->tokens = (syntax_token_t*)calloc(new_line->capacity, sizeof(syntax_token_t));
    new_line->length = current->length - buf->cursor_column;
    new_line->folded = false;
    new_line->fold_level = current->fold_level;
    
    if (new_line->length > 0) {
        memcpy(new_line->text, &current->text[buf->cursor_column], new_line->length);
        new_line->text[new_line->length] = '\0';
        current->text[buf->cursor_column] = '\0';
        current->length = buf->cursor_column;
    }
    
    buf->line_count++;
    buf->cursor_line++;
    buf->cursor_column = 0;
    buf->modified = true;
    
    editor_push_undo(buf, ACTION_INSERT_LINE, buf->cursor_line - 1, buf->cursor_column, new_line->text, new_line->length, NULL, 0);

    if (buf->auto_indent) {
        editor_auto_indent(buf);
    }
}

void editor_delete_line(editor_buffer_t* buf, uint32_t line_num) {
    if (buf->read_only || line_num >= buf->line_count) return;
    
    char* deleted_line_content = strdup(buf->lines[line_num].text); // Copy content for undo
    uint32_t deleted_line_len = buf->lines[line_num].length;

    free(buf->lines[line_num].text);
    free(buf->lines[line_num].tokens);
    
    memmove(&buf->lines[line_num],
            &buf->lines[line_num + 1],
            (buf->line_count - line_num - 1) * sizeof(editor_line_t));
    
    buf->line_count--;
    buf->modified = true;

    editor_push_undo(buf, ACTION_DELETE_LINE, line_num, 0, deleted_line_content, deleted_line_len, NULL, 0);
    free(deleted_line_content);
}

void editor_insert_text(editor_buffer_t* buf, const char* text, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        if (text[i] == '\n') {
            editor_insert_line(buf);
        } else {
            editor_insert_char(buf, text[i]);
        }
    }
}

void editor_delete_selection(editor_buffer_t* buf) {
    if (!buf->selection.active) return;
    
    // Store selected text for undo/redo
    char* deleted_text = editor_get_selected_text(buf);
    uint32_t deleted_text_len = strlen(deleted_text);

    uint32_t start_line = buf->selection.start_line;
    uint32_t start_col = buf->selection.start_column;
    uint32_t end_line = buf->selection.end_line;
    uint32_t end_col = buf->selection.end_column;
    
    if (start_line > end_line || (start_line == end_line && start_col > end_col)) {
        uint32_t temp = start_line;
        start_line = end_line;
        end_line = temp;
        temp = start_col;
        start_col = end_col;
        end_col = temp;
    }
    
    if (start_line == end_line) {
        editor_line_t* line = &buf->lines[start_line];
        memmove(&line->text[start_col],
                &line->text[end_col],
                line->length - end_col + 1);
        line->length -= (end_col - start_col);
    } else {
        editor_line_t* start = &buf->lines[start_line];
        editor_line_t* end = &buf->lines[end_line];
        
        while (start->length + (end->length - end_col) + 1 >= start->capacity) {
            start->capacity *= 2;
            start->text = (char*)realloc(start->text, start->capacity);
        }
        
        memcpy(&start->text[start_col], &end->text[end_col], end->length - end_col + 1);
        start->length = start_col + (end->length - end_col);
        
        for (uint32_t i = start_line + 1; i <= end_line; i++) {
            editor_delete_line(buf, start_line + 1);
        }
    }
    
    buf->cursor_line = start_line;
    buf->cursor_column = start_col;
    buf->selection.active = false;
    buf->modified = true;

    editor_push_undo(buf, ACTION_DELETE_SELECTION, start_line, start_col, deleted_text, deleted_text_len, NULL, 0);
    free(deleted_text);
}

// ============================================================================ 
// Cursor Movement
// ============================================================================ 

void editor_move_cursor(editor_buffer_t* buf, int32_t dx, int32_t dy) {
    if (dy != 0) {
        int32_t new_line = (int32_t)buf->cursor_line + dy;
        if (new_line < 0) new_line = 0;
        if (new_line >= (int32_t)buf->line_count) new_line = buf->line_count - 1;
        buf->cursor_line = new_line;
        
        if (buf->cursor_column > buf->lines[buf->cursor_line].length) {
            buf->cursor_column = buf->lines[buf->cursor_line].length;
        }
    }
    
    if (dx != 0) {
        int32_t new_col = (int32_t)buf->cursor_column + dx;
        if (new_col < 0) new_col = 0;
        if (new_col > (int32_t)buf->lines[buf->cursor_line].length) {
            new_col = buf->lines[buf->cursor_line].length;
        }
        buf->cursor_column = new_col;
    }
}

void editor_move_cursor_to(editor_buffer_t* buf, uint32_t line, uint32_t column) {
    if (line >= buf->line_count) line = buf->line_count - 1;
    buf->cursor_line = line;
    
    if (column > buf->lines[line].length) column = buf->lines[line].length;
    buf->cursor_column = column;
}

void editor_move_to_line_start(editor_buffer_t* buf) {
    buf->cursor_column = 0;
}

void editor_move_to_line_end(editor_buffer_t* buf) {
    buf->cursor_column = buf->lines[buf->cursor_line].length;
}

void editor_move_to_document_start(editor_buffer_t* buf) {
    buf->cursor_line = 0;
    buf->cursor_column = 0;
}

void editor_move_to_document_end(editor_buffer_t* buf) {
    buf->cursor_line = buf->line_count - 1;
    buf->cursor_column = buf->lines[buf->cursor_line].length;
}

void editor_page_up(editor_buffer_t* buf) {
    editor_move_cursor(buf, 0, -25);
}

void editor_page_down(editor_buffer_t* buf) {
    editor_move_cursor(buf, 0, 25);
}

// ============================================================================ 
// Selection
// ============================================================================ 

void editor_start_selection(editor_buffer_t* buf) {
    buf->selection.active = true;
    buf->selection.start_line = buf->cursor_line;
    buf->selection.start_column = buf->cursor_column;
}

void editor_end_selection(editor_buffer_t* buf) {
    if (buf->selection.active) {
        buf->selection.end_line = buf->cursor_line;
        buf->selection.end_column = buf->cursor_column;
    }
}

void editor_select_all(editor_buffer_t* buf) {
    buf->selection.active = true;
    buf->selection.start_line = 0;
    buf->selection.start_column = 0;
    buf->selection.end_line = buf->line_count - 1;
    buf->selection.end_column = buf->lines[buf->line_count - 1].length;
}

void editor_select_word(editor_buffer_t* buf) {
    editor_line_t* line = &buf->lines[buf->cursor_line];
    uint32_t start = buf->cursor_column;
    uint32_t end = buf->cursor_column;
    
    while (start > 0 && (isalnum(line->text[start - 1]) || line->text[start - 1] == '_')) {
        start--;
    }
    while (end < line->length && (isalnum(line->text[end]) || line->text[end] == '_')) {
        end++;
    }
    
    buf->selection.active = true;
    buf->selection.start_line = buf->cursor_line;
    buf->selection.start_column = start;
    buf->selection.end_line = buf->cursor_line;
    buf->selection.end_column = end;
}

void editor_select_line(editor_buffer_t* buf) {
    buf->selection.active = true;
    buf->selection.start_line = buf->cursor_line;
    buf->selection.start_column = 0;
    buf->selection.end_line = buf->cursor_line;
    buf->selection.end_column = buf->lines[buf->cursor_line].length;
}

char* editor_get_selected_text(editor_buffer_t* buf) {
    if (!buf->selection.active) return NULL;
    
    uint32_t start_line = buf->selection.start_line;
    uint32_t start_col = buf->selection.start_column;
    uint32_t end_line = buf->selection.end_line;
    uint32_t end_col = buf->selection.end_column;
    
    if (start_line > end_line || (start_line == end_line && start_col > end_col)) {
        uint32_t temp = start_line;
        start_line = end_line;
        end_line = temp;
        temp = start_col;
        start_col = end_col;
        end_col = temp;
    }
    
    uint32_t total_len = 0;
    if (start_line == end_line) {
        total_len = end_col - start_col;
    } else {
        total_len = buf->lines[start_line].length - start_col + 1;
        for (uint32_t i = start_line + 1; i < end_line; i++) {
            total_len += buf->lines[i].length + 1;
        }
        total_len += end_col;
    }
    
    char* text = (char*)malloc(total_len + 1);
    uint32_t pos = 0;
    
    if (start_line == end_line) {
        memcpy(text, &buf->lines[start_line].text[start_col], end_col - start_col);
        pos = end_col - start_col;
    } else {
        uint32_t len = buf->lines[start_line].length - start_col;
        memcpy(text, &buf->lines[start_line].text[start_col], len);
        pos += len;
        text[pos++] = '\n';
        
        for (uint32_t i = start_line + 1; i < end_line; i++) {
            memcpy(&text[pos], buf->lines[i].text, buf->lines[i].length);
            pos += buf->lines[i].length;
            text[pos++] = '\n';
        }
        
        memcpy(&text[pos], buf->lines[end_line].text, end_col);
        pos += end_col;
    }
    
    text[pos] = '\0';
    return text;
}

void editor_clear_selection(editor_buffer_t* buf) {
    buf->selection.active = false;
}

// ============================================================================ 
// Undo/Redo
// ============================================================================ 

void editor_push_undo(editor_buffer_t* buf, action_type_t type, uint32_t line, uint32_t col, const char* old_text, uint32_t old_text_len, const char* new_text, uint32_t new_text_len) {
    if (buf->undo_position < MAX_UNDO_LEVELS - 1) {
        editor_action_t* action = &buf->undo_stack[buf->undo_position];
        action->type = type;
        action->line = line;
        action->column = col;
        
        if (old_text && old_text_len > 0) {
            action->old_text = (char*)malloc(old_text_len + 1);
            memcpy(action->old_text, old_text, old_text_len);
            action->old_text[old_text_len] = '\0';
            action->old_text_len = old_text_len;
        } else {
            action->old_text = NULL;
            action->old_text_len = 0;
        }

        if (new_text && new_text_len > 0) {
            action->new_text = (char*)malloc(new_text_len + 1);
            memcpy(action->new_text, new_text, new_text_len);
            action->new_text[new_text_len] = '\0';
            action->new_text_len = new_text_len;
        } else {
            action->new_text = NULL;
            action->new_text_len = 0;
        }
        
        buf->undo_position++;
        buf->undo_count = buf->undo_position; // Clear redo stack
    }
}

void texteditor_undo(texteditor_ctx_t* ctx) {
    if (!ctx) return;
    editor_buffer_t* buf = &ctx->tabs[ctx->active_tab].buffer;
    
    if (buf->undo_position == 0) return;
    
    buf->undo_position--;
    editor_action_t* action = &buf->undo_stack[buf->undo_position];
    
    // Reverse the action
    editor_move_cursor_to(buf, action->line, action->column);
    
    switch (action->type) {
        case ACTION_INSERT_CHAR:
            editor_delete_char(buf); // Delete the inserted char
            break;
        case ACTION_INSERT_LINE:
            editor_delete_line(buf, action->line + 1); // Delete the inserted line
            break;
        case ACTION_DELETE_CHAR:
            editor_insert_char(buf, action->old_text[0]); // Re-insert the deleted char
            break;
        case ACTION_DELETE_LINE:
            // Re-insert the deleted line
            editor_move_cursor_to(buf, action->line, action->column);
            editor_insert_line(buf); // This will split the current line, then paste.
            editor_insert_text(buf, action->old_text, action->old_text_len);
            break;
        case ACTION_DELETE_SELECTION: // Re-insert deleted selection
            editor_insert_text(buf, action->old_text, action->old_text_len);
            break;
        case ACTION_REPLACE_TEXT:
            // Delete new text, insert old text
            // Move cursor to end of new text
            editor_move_cursor_to(buf, action->line, action->column + action->new_text_len); 
            // Delete new text
            for (uint32_t i = 0; i < action->new_text_len; i++) editor_delete_char(buf); 
            // Insert old text
            editor_insert_text(buf, action->old_text, action->old_text_len); 
            break;
    }
    buf->modified = true;
}

void texteditor_redo(texteditor_ctx_t* ctx) {
    if (!ctx) return;
    editor_buffer_t* buf = &ctx->tabs[ctx->active_tab].buffer;

    if (buf->undo_position >= buf->undo_count) return; 
    
    editor_action_t* action = &buf->undo_stack[buf->undo_position];
    
    // Redo the action
    editor_move_cursor_to(buf, action->line, action->column);
    
    switch (action->type) {
        case ACTION_INSERT_CHAR:
            editor_insert_char(buf, action->old_text[0]);
            break;
        case ACTION_INSERT_LINE:
            editor_insert_line(buf);
            editor_insert_text(buf, action->old_text, action->old_text_len);
            break;
        case ACTION_DELETE_CHAR:
            editor_delete_char(buf);
            break;
        case ACTION_DELETE_LINE:
            editor_delete_line(buf, action->line);
            break;
        case ACTION_DELETE_SELECTION: // Re-do deleting selection
            editor_delete_selection(buf);
            break;
        case ACTION_REPLACE_TEXT:
            // Delete old text, insert new text
            // Move cursor to end of old text
            editor_move_cursor_to(buf, action->line, action->column + action->old_text_len); 
            // Delete old text
            for (uint32_t i = 0; i < action->old_text_len; i++) editor_delete_char(buf); 
            // Insert new text
            editor_insert_text(buf, action->new_text, action->new_text_len); 
            break;
    }
    buf->modified = true;
    buf->undo_position++;
}

// ============================================================================ 
// Syntax Highlighting (Simplified)
// ============================================================================ 

static bool is_keyword_func(const char* word, uint32_t len, const char** keywords_list) {
    for (uint32_t i = 0; keywords_list[i]; i++) {
        if (strlen(keywords_list[i]) == len && strncmp(word, keywords_list[i], len) == 0) {
            return true;
        }
    }
    return false;
}

void editor_highlight_line(editor_buffer_t* buf, uint32_t line_num) {
    if (!buf->language || line_num >= buf->line_count) return; 
    
    editor_line_t* line = &buf->lines[line_num];
    free(line->tokens); // Free old tokens
    line->tokens = (syntax_token_t*)calloc(line->capacity, sizeof(syntax_token_t)); // Allocate new
    line->token_count = 0;
    
    const char* text = line->text;
    uint32_t len = line->length;
    uint32_t i = 0;
    
    while (i < len) {
        if (isspace(text[i])) { i++; continue; } 
        
        // Comments
        if (buf->language->line_comment && strncmp(&text[i], buf->language->line_comment, strlen(buf->language->line_comment)) == 0) {
            line->tokens[line->token_count++] = (syntax_token_t){TOKEN_COMMENT, i, len - i};
            break;
        }
        
        // Strings
        if (text[i] == '"' || text[i] == '\'') {
            char quote = text[i];
            uint32_t start = i++;
            while (i < len && text[i] != quote) {
                if (text[i] == '\\') i++;  // Skip escaped characters
                i++;
            }
            if (i < len) i++;  // Include closing quote
            line->tokens[line->token_count++] = (syntax_token_t){TOKEN_STRING, start, i - start};
            continue;
        }
        
        // Numbers
        if (isdigit(text[i])) {
            uint32_t start = i;
            while (i < len && (isdigit(text[i]) || text[i] == '.' || text[i] == 'x' || text[i] == 'X')) i++;
            line->tokens[line->token_count++] = (syntax_token_t){TOKEN_NUMBER, start, i - start};
            continue;
        }
        
        // Identifiers/Keywords/Types
        if (isalpha(text[i]) || text[i] == '_') {
            uint32_t start = i;
            while (i < len && (isalnum(text[i]) || text[i] == '_')) i++;
            uint32_t word_len = i - start;
            
            token_type_t type = TOKEN_NORMAL;
            if (is_keyword_func(&text[start], word_len, buf->language->keywords)) {
                type = TOKEN_KEYWORD;
            } else if (is_keyword_func(&text[start], word_len, buf->language->types)) {
                type = TOKEN_TYPE;
            }
            
            line->tokens[line->token_count++] = (syntax_token_t){type, start, word_len};
            continue;
        }
        
        i++;
    }
}

void editor_highlight_all(editor_buffer_t* buf) {
    for (uint32_t i = 0; i < buf->line_count; i++) {
        editor_highlight_line(buf, i);
    }
}

void editor_set_language(editor_buffer_t* buf, language_def_t* lang) {
    buf->language = lang;
    editor_highlight_all(buf);
}

// ============================================================================ 
// Auto-indent
// ============================================================================ 

uint32_t editor_calculate_indent_level(const char* line) {
    uint32_t indent = 0;
    while (*line == ' ' || *line == '\t') {
        if (*line == '\t') indent += 4;
        else indent++;
        line++;
    }
    return indent;
}

void editor_auto_indent(editor_buffer_t* buf) {
    if (buf->cursor_line == 0) return;
    
    editor_line_t* prev = &buf->lines[buf->cursor_line - 1];
    uint32_t indent = editor_calculate_indent_level(prev->text);
    
    if (prev->length > 0) {
        char last = prev->text[prev->length - 1];
        if (last == '{' || last == ':') {
            indent += buf->tab_size;
        }
    }
    
    for (uint32_t i = 0; i < indent; i++) {
        editor_insert_char(buf, ' ');
    }
}

// ============================================================================ 
// Code Folding
// ============================================================================ 

uint32_t editor_calculate_fold_level(const char* line) {
    uint32_t level = 0;
    while (*line) {
        if (*line == '{') level++;
        else if (*line == '}' && level > 0) level--;
        line++;
    }
    return level;
}

void editor_toggle_fold(editor_buffer_t* buf, uint32_t line) {
    if (line >= buf->line_count) return;
    buf->lines[line].folded = !buf->lines[line].folded;
}

void editor_fold_all(editor_buffer_t* buf) {
    for (uint32_t i = 0; i < buf->line_count; i++) {
        if (buf->lines[i].fold_level > 0) {
            buf->lines[i].folded = true;
        }
    }
}

void editor_unfold_all(editor_buffer_t* buf) {
    for (uint32_t i = 0; i < buf->line_count; i++) {
        buf->lines[i].folded = false;
    }
}

// ============================================================================ 
// Search & Replace
// ============================================================================ 

void editor_search(texteditor_ctx_t* ctx, const char* query, bool regex, bool case_sensitive) {
    printf("TEXTEDITOR: Search (not implemented)\n");
}

void editor_find_next(texteditor_ctx_t* ctx) {
    printf("TEXTEDITOR: Find Next (not implemented)\n");
}

void editor_find_previous(texteditor_ctx_t* ctx) {
    printf("TEXTEDITOR: Find Previous (not implemented)\n");
}

void editor_replace_current(texteditor_ctx_t* ctx, const char* replacement) {
    printf("TEXTEDITOR: Replace Current (not implemented)\n");
}

void editor_replace_all(texteditor_ctx_t* ctx, const char* replacement) {
    printf("TEXTEDITOR: Replace All (not implemented)\n");
}

// ============================================================================ 
// Split View
// ============================================================================ 

void editor_toggle_split_view(texteditor_ctx_t* ctx, bool vertical) {
    printf("TEXTEDITOR: Toggle Split View (not implemented)\n");
}

void editor_close_split_view(texteditor_ctx_t* ctx) {
    printf("TEXTEDITOR: Close Split View (not implemented)\n");
}

// ============================================================================ 
// Rendering
// ============================================================================ 

void texteditor_render_buffer(texteditor_ctx_t* ctx, editor_buffer_t* buf, int32_t x, int32_t y, uint32_t width, uint32_t height) {
    if (!ctx || !buf || !ctx->editor_window || !ctx->editor_window->framebuffer) return;

    uint32_t* fb = (uint32_t*)ctx->editor_window->framebuffer;
    int fb_width = ctx->editor_window->width;
    int char_w = ctx->char_width;
    int char_h = ctx->char_height;

    // Clear background
    draw_rect(fb, fb_width, x, y, width, height, ctx->bg_color);

    int line_num_col_width = 4 * char_w; // For line numbers
    int text_start_x = x + (buf->show_line_numbers ? line_num_col_width : 0);

    for (uint32_t i = 0; i < height / char_h; i++) {
        uint32_t line_idx = buf->scroll_line + i;
        if (line_idx >= buf->line_count) break;

        editor_line_t* line = &buf->lines[line_idx];
        int draw_y = y + i * char_h;

        // Highlight current line
        if (buf->highlight_current_line && line_idx == buf->cursor_line) {
            draw_rect(fb, fb_width, x, draw_y, width, char_h, ctx->current_line_color);
        }

        // Draw line numbers
        if (buf->show_line_numbers) {
            char num_str[16];
            snprintf(num_str, sizeof(num_str), "%u", line_idx + 1);
            draw_string_editor(fb, fb_width, x, draw_y, num_str, ctx->line_number_color);
        }

        // Draw text content
        uint32_t current_x = text_start_x;
        for (uint32_t j = 0; j < line->length; j++) {
            char c = line->text[j];
            uint32_t fg_color = ctx->fg_color;

            // Apply syntax highlighting colors
            for (uint32_t t = 0; t < line->token_count; t++) {
                if (j >= line->tokens[t].start && j < line->tokens[t].start + line->tokens[t].length) {
                    switch (line->tokens[t].type) {
                        case TOKEN_KEYWORD: fg_color = ctx->token_colors[TOKEN_KEYWORD]; break;
                        case TOKEN_TYPE: fg_color = ctx->token_colors[TOKEN_TYPE]; break;
                        case TOKEN_STRING: fg_color = ctx->token_colors[TOKEN_STRING]; break;
                        case TOKEN_COMMENT: fg_color = ctx->token_colors[TOKEN_COMMENT]; break;
                        case TOKEN_NUMBER: fg_color = ctx->token_colors[TOKEN_NUMBER]; break;
                        case TOKEN_OPERATOR: fg_color = ctx->token_colors[TOKEN_OPERATOR]; break;
                        case TOKEN_PREPROCESSOR: fg_color = ctx->token_colors[TOKEN_PREPROCESSOR]; break;
                        case TOKEN_FUNCTION: fg_color = ctx->token_colors[TOKEN_FUNCTION]; break;
                        default: break;
                    }
                    break;
                }
            }
            draw_char_editor(fb, fb_width, current_x, draw_y, c, fg_color);
            current_x += char_w;
        }

        // Draw cursor
        if (line_idx == buf->cursor_line) {
            int cursor_x_pos = text_start_x + (buf->cursor_column - buf->scroll_column) * char_w;
            draw_rect(fb, fb_width, cursor_x_pos, draw_y, 2, char_h, 0xFFFFFFFF);
        }
    }
}

// ============================================================================ 
// Input Handling
// ============================================================================ 

void texteditor_handle_key(texteditor_ctx_t* ctx, uint32_t keycode, uint32_t modifiers, bool pressed) {
    if (!pressed) return; // Only handle key presses

    editor_buffer_t* buf = &ctx->tabs[ctx->active_tab].buffer;

    switch (keycode) {
        case 0x1B: // Escape
            break;
        case 0x08: // Backspace
            editor_delete_char(buf);
            break;
        case 0x0D: // Enter
            editor_insert_line(buf);
            break;
        case 0x25: // Left Arrow
            editor_move_cursor(buf, -1, 0);
            break;
        case 0x26: // Up Arrow
            editor_move_cursor(buf, 0, -1);
            break;
        case 0x27: // Right Arrow
            editor_move_cursor(buf, 1, 0);
            break;
        case 0x28: // Down Arrow
            editor_move_cursor(buf, 0, 1);
            break;
        // Basic character input
        default:
            if (keycode >= ' ' && keycode <= '~') { // Printable ASCII
                editor_insert_char(buf, (char)keycode);
            }
            break;
    }
}

void texteditor_handle_char(texteditor_ctx_t* ctx, uint32_t codepoint) {
    // Already handled by key handler for now, if it's printable ASCII
}

// ============================================================================ 
// Main Entry Point
// ============================================================================