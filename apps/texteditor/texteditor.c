/**
 * @file texteditor.c
 * @brief Advanced Text Editor Implementation
 */

#include "texteditor.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// C language keywords
static const char* c_keywords[] = {
    "auto", "break", "case", "char", "const", "continue", "default", "do",
    "double", "else", "enum", "extern", "float", "for", "goto", "if",
    "inline", "int", "long", "register", "restrict", "return", "short", "signed",
    "sizeof", "static", "struct", "switch", "typedef", "union", "unsigned", "void",
    "volatile", "while", "_Bool", "_Complex", "_Imaginary"
};

static const char* c_types[] = {
    "int8_t", "int16_t", "int32_t", "int64_t",
    "uint8_t", "uint16_t", "uint32_t", "uint64_t",
    "size_t", "ssize_t", "ptrdiff_t", "intptr_t", "uintptr_t",
    "bool", "true", "false", "NULL"
};

static const char* c_extensions[] = {".c", ".h"};

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
    buf->lines[0].tokens = NULL;
    buf->lines[0].token_count = 0;
    buf->lines[0].folded = false;
    buf->lines[0].fold_level = 0;
    
    buf->cursor_line = 0;
    buf->cursor_column = 0;
    buf->scroll_line = 0;
    buf->scroll_column = 0;
    
    buf->selection.active = false;
    buf->undo_count = 0;
    buf->undo_position = 0;
    
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
}

void editor_insert_char(editor_buffer_t* buf, char c) {
    if (buf->read_only) return;
    
    editor_line_t* line = &buf->lines[buf->cursor_line];
    
    // Expand line buffer if needed
    if (line->length + 1 >= line->capacity) {
        line->capacity *= 2;
        line->text = (char*)realloc(line->text, line->capacity);
    }
    
    // Insert character
    memmove(&line->text[buf->cursor_column + 1],
            &line->text[buf->cursor_column],
            line->length - buf->cursor_column);
    line->text[buf->cursor_column] = c;
    line->length++;
    line->text[line->length] = '\0';
    
    buf->cursor_column++;
    buf->modified = true;
    
    // Re-highlight line
    if (buf->language) {
        editor_highlight_line(buf, buf->cursor_line);
    }
}

void editor_delete_char(editor_buffer_t* buf) {
    if (buf->read_only) return;
    if (buf->cursor_column == 0 && buf->cursor_line == 0) return;
    
    editor_line_t* line = &buf->lines[buf->cursor_line];
    
    if (buf->cursor_column > 0) {
        // Delete character before cursor
        memmove(&line->text[buf->cursor_column - 1],
                &line->text[buf->cursor_column],
                line->length - buf->cursor_column + 1);
        line->length--;
        buf->cursor_column--;
        buf->modified = true;
        
        if (buf->language) {
            editor_highlight_line(buf, buf->cursor_line);
        }
    } else {
        // Merge with previous line
        if (buf->cursor_line > 0) {
            editor_line_t* prev_line = &buf->lines[buf->cursor_line - 1];
            uint32_t prev_len = prev_line->length;
            
            // Expand previous line if needed
            while (prev_line->length + line->length + 1 >= prev_line->capacity) {
                prev_line->capacity *= 2;
                prev_line->text = (char*)realloc(prev_line->text, prev_line->capacity);
            }
            
            // Append current line to previous
            memcpy(&prev_line->text[prev_line->length], line->text, line->length + 1);
            prev_line->length += line->length;
            
            // Delete current line
            editor_delete_line(buf, buf->cursor_line);
            
            buf->cursor_line--;
            buf->cursor_column = prev_len;
            buf->modified = true;
        }
    }
}

void editor_insert_line(editor_buffer_t* buf) {
    if (buf->read_only) return;
    
    // Expand line array if needed
    if (buf->line_count + 1 >= buf->line_capacity) {
        buf->line_capacity *= 2;
        buf->lines = (editor_line_t*)realloc(buf->lines, buf->line_capacity * sizeof(editor_line_t));
    }
    
    // Move lines down
    memmove(&buf->lines[buf->cursor_line + 2],
            &buf->lines[buf->cursor_line + 1],
            (buf->line_count - buf->cursor_line - 1) * sizeof(editor_line_t));
    
    // Split current line
    editor_line_t* current = &buf->lines[buf->cursor_line];
    editor_line_t* new_line = &buf->lines[buf->cursor_line + 1];
    
    new_line->capacity = 128;
    new_line->text = (char*)calloc(new_line->capacity, 1);
    new_line->length = current->length - buf->cursor_column;
    new_line->tokens = NULL;
    new_line->token_count = 0;
    new_line->folded = false;
    new_line->fold_level = current->fold_level;
    
    // Copy text after cursor to new line
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
    
    // Auto-indent
    if (buf->auto_indent) {
        editor_auto_indent(buf);
    }
}

void editor_delete_line(editor_buffer_t* buf, uint32_t line) {
    if (buf->read_only || line >= buf->line_count) return;
    
    free(buf->lines[line].text);
    free(buf->lines[line].tokens);
    
    memmove(&buf->lines[line],
            &buf->lines[line + 1],
            (buf->line_count - line - 1) * sizeof(editor_line_t));
    
    buf->line_count--;
    buf->modified = true;
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
    
    // Normalize selection (start before end)
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
    
    // Delete selection
    if (start_line == end_line) {
        // Single line selection
        editor_line_t* line = &buf->lines[start_line];
        memmove(&line->text[start_col],
                &line->text[end_col],
                line->length - end_col + 1);
        line->length -= (end_col - start_col);
    } else {
        // Multi-line selection
        // Keep start of first line and end of last line
        editor_line_t* start = &buf->lines[start_line];
        editor_line_t* end = &buf->lines[end_line];
        
        // Append end of last line to start line
        while (start->length + (end->length - end_col) + 1 >= start->capacity) {
            start->capacity *= 2;
            start->text = (char*)realloc(start->text, start->capacity);
        }
        
        memcpy(&start->text[start_col], &end->text[end_col], end->length - end_col + 1);
        start->length = start_col + (end->length - end_col);
        
        // Delete intermediate lines
        for (uint32_t i = start_line + 1; i <= end_line; i++) {
            editor_delete_line(buf, start_line + 1);
        }
    }
    
    buf->cursor_line = start_line;
    buf->cursor_column = start_col;
    buf->selection.active = false;
    buf->modified = true;
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
        
        // Clamp column to line length
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
    editor_move_cursor(buf, 0, -25);  // Move up 25 lines
}

void editor_page_down(editor_buffer_t* buf) {
    editor_move_cursor(buf, 0, 25);  // Move down 25 lines
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
    
    // Find word boundaries
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
    
    // Normalize
    if (start_line > end_line || (start_line == end_line && start_col > end_col)) {
        uint32_t temp = start_line;
        start_line = end_line;
        end_line = temp;
        temp = start_col;
        start_col = end_col;
        end_col = temp;
    }
    
    // Calculate total length
    uint32_t total_len = 0;
    if (start_line == end_line) {
        total_len = end_col - start_col;
    } else {
        total_len = buf->lines[start_line].length - start_col + 1;  // +1 for newline
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
        // First line
        uint32_t len = buf->lines[start_line].length - start_col;
        memcpy(text, &buf->lines[start_line].text[start_col], len);
        pos += len;
        text[pos++] = '\n';
        
        // Middle lines
        for (uint32_t i = start_line + 1; i < end_line; i++) {
            memcpy(&text[pos], buf->lines[i].text, buf->lines[i].length);
            pos += buf->lines[i].length;
            text[pos++] = '\n';
        }
        
        // Last line
        memcpy(&text[pos], buf->lines[end_line].text, end_col);
        pos += end_col;
    }
    
    text[pos] = '\0';
    return text;
}

// ============================================================================
// Undo/Redo
// ============================================================================

void editor_push_undo(editor_buffer_t* buf, action_type_t type, uint32_t line, uint32_t col, const char* text, uint32_t len) {
    if (buf->undo_position < MAX_UNDO_LEVELS - 1) {
        editor_action_t* action = &buf->undo_stack[buf->undo_position];
        action->type = type;
        action->line = line;
        action->column = col;
        action->text = (char*)malloc(len + 1);
        memcpy(action->text, text, len);
        action->text[len] = '\0';
        action->text_len = len;
        
        buf->undo_position++;
        buf->undo_count = buf->undo_position;
    }
}

void editor_undo(editor_buffer_t* buf) {
    if (buf->undo_position == 0) return;
    
    buf->undo_position--;
    editor_action_t* action = &buf->undo_stack[buf->undo_position];
    
    // Reverse the action
    editor_move_cursor_to(buf, action->line, action->column);
    
    switch (action->type) {
        case ACTION_INSERT_CHAR:
        case ACTION_INSERT_LINE:
            // Delete what was inserted
            for (uint32_t i = 0; i < action->text_len; i++) {
                editor_delete_char(buf);
            }
            break;
        case ACTION_DELETE_CHAR:
        case ACTION_DELETE_LINE:
            // Re-insert what was deleted
            editor_insert_text(buf, action->text, action->text_len);
            break;
        case ACTION_REPLACE_TEXT:
            // TODO: Implement replace undo
            break;
    }
}

void editor_redo(editor_buffer_t* buf) {
    if (buf->undo_position >= buf->undo_count) return;
    
    editor_action_t* action = &buf->undo_stack[buf->undo_position];
    editor_move_cursor_to(buf, action->line, action->column);
    
    switch (action->type) {
        case ACTION_INSERT_CHAR:
        case ACTION_INSERT_LINE:
            editor_insert_text(buf, action->text, action->text_len);
            break;
        case ACTION_DELETE_CHAR:
        case ACTION_DELETE_LINE:
            for (uint32_t i = 0; i < action->text_len; i++) {
                editor_delete_char(buf);
            }
            break;
        case ACTION_REPLACE_TEXT:
            // TODO: Implement replace redo
            break;
    }
    
    buf->undo_position++;
}

// ============================================================================
// Syntax Highlighting (Simplified)
// ============================================================================

static bool is_keyword(const char* word, uint32_t len, language_def_t* lang) {
    for (uint32_t i = 0; i < lang->keyword_count; i++) {
        if (strlen(lang->keywords[i]) == len && strncmp(word, lang->keywords[i], len) == 0) {
            return true;
        }
    }
    return false;
}

static bool is_type(const char* word, uint32_t len, language_def_t* lang) {
    for (uint32_t i = 0; i < lang->type_count; i++) {
        if (strlen(lang->types[i]) == len && strncmp(word, lang->types[i], len) == 0) {
            return true;
        }
    }
    return false;
}

void editor_highlight_line(editor_buffer_t* buf, uint32_t line_num) {
    if (!buf->language || line_num >= buf->line_count) return;
    
    editor_line_t* line = &buf->lines[line_num];
    free(line->tokens);
    line->tokens = (syntax_token_t*)malloc(256 * sizeof(syntax_token_t));
    line->token_count = 0;
    
    const char* text = line->text;
    uint32_t len = line->length;
    uint32_t i = 0;
    
    while (i < len) {
        // Skip whitespace
        while (i < len && isspace(text[i])) i++;
        if (i >= len) break;
        
        // Check for comments
        if (buf->language->line_comment && strncmp(&text[i], buf->language->line_comment, strlen(buf->language->line_comment)) == 0) {
            syntax_token_t token = {TOKEN_COMMENT, i, len - i};
            line->tokens[line->token_count++] = token;
            break;
        }
        
        // Check for strings
        if (text[i] == '"' || text[i] == '\'') {
            char quote = text[i];
            uint32_t start = i++;
            while (i < len && text[i] != quote) {
                if (text[i] == '\\') i++;  // Skip escaped characters
                i++;
            }
            if (i < len) i++;  // Include closing quote
            syntax_token_t token = {TOKEN_STRING, start, i - start};
            line->tokens[line->token_count++] = token;
            continue;
        }
        
        // Check for numbers
        if (isdigit(text[i])) {
            uint32_t start = i;
            while (i < len && (isdigit(text[i]) || text[i] == '.' || text[i] == 'x' || text[i] == 'X')) i++;
            syntax_token_t token = {TOKEN_NUMBER, start, i - start};
            line->tokens[line->token_count++] = token;
            continue;
        }
        
        // Check for identifiers/keywords
        if (isalpha(text[i]) || text[i] == '_') {
            uint32_t start = i;
            while (i < len && (isalnum(text[i]) || text[i] == '_')) i++;
            uint32_t word_len = i - start;
            
            token_type_t type = TOKEN_NORMAL;
            if (is_keyword(&text[start], word_len, buf->language)) {
                type = TOKEN_KEYWORD;
            } else if (is_type(&text[start], word_len, buf->language)) {
                type = TOKEN_TYPE;
            }
            
            syntax_token_t token = {type, start, word_len};
            line->tokens[line->token_count++] = token;
            continue;
        }
        
        // Operators and other characters
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
    
    // Get indent from previous line
    editor_line_t* prev = &buf->lines[buf->cursor_line - 1];
    uint32_t indent = editor_calculate_indent_level(prev->text);
    
    // Check if previous line ends with { or :
    if (prev->length > 0) {
        char last = prev->text[prev->length - 1];
        if (last == '{' || last == ':') {
            indent += buf->tab_size;
        }
    }
    
    // Insert spaces
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

// Continued in next part...
