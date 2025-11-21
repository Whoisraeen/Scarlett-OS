/**
 * @file main.c
 * @brief Text Editor Entry Point
 */

#include "texteditor.h"
#include <stdio.h>
#include <stdlib.h>

// ============================================================================
// Text Editor Context Management
// ============================================================================

texteditor_ctx_t* texteditor_create(compositor_ctx_t* compositor) {
    texteditor_ctx_t* ctx = (texteditor_ctx_t*)calloc(1, sizeof(texteditor_ctx_t));
    ctx->compositor = compositor;
    
    // Create window
    ctx->editor_window = compositor_create_window(compositor, "Text Editor", 100, 100, 1000, 700);
    
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
    FILE* file = fopen(path, "r");
    if (!file) return false;
    
    editor_buffer_t* buf = &ctx->tabs[ctx->active_tab].buffer;
    
    // Clear existing buffer
    editor_buffer_destroy(buf);
    editor_buffer_init(buf);
    
    // Read file
    char line_buf[MAX_LINE_LENGTH];
    while (fgets(line_buf, sizeof(line_buf), file)) {
        uint32_t len = strlen(line_buf);
        if (len > 0 && line_buf[len - 1] == '\n') {
            line_buf[len - 1] = '\0';
            len--;
        }
        
        if (buf->line_count > 1 || buf->lines[0].length > 0) {
            editor_insert_line(buf);
        }
        
        editor_insert_text(buf, line_buf, len);
    }
    
    fclose(file);
    
    strncpy(buf->file_path, path, sizeof(buf->file_path) - 1);
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
    
    return true;
}

bool texteditor_save_file(texteditor_ctx_t* ctx) {
    editor_buffer_t* buf = &ctx->tabs[ctx->active_tab].buffer;
    
    if (buf->file_path[0] == '\0') {
        // No file path, need to prompt for save as
        return false;
    }
    
    FILE* file = fopen(buf->file_path, "w");
    if (!file) return false;
    
    for (uint32_t i = 0; i < buf->line_count; i++) {
        fprintf(file, "%s\n", buf->lines[i].text);
    }
    
    fclose(file);
    buf->modified = false;
    
    return true;
}

bool texteditor_save_file_as(texteditor_ctx_t* ctx, const char* path) {
    editor_buffer_t* buf = &ctx->tabs[ctx->active_tab].buffer;
    strncpy(buf->file_path, path, sizeof(buf->file_path) - 1);
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
        // TODO: Copy to system clipboard
        free(text);
    }
}

void texteditor_cut(texteditor_ctx_t* ctx) {
    texteditor_copy(ctx);
    editor_buffer_t* buf = &ctx->tabs[ctx->active_tab].buffer;
    editor_delete_selection(buf);
}

void texteditor_paste(texteditor_ctx_t* ctx) {
    // TODO: Paste from system clipboard
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
// Main Entry Point
// ============================================================================

int main(int argc, char** argv) {
    // Connect to compositor
    compositor_ctx_t* compositor = compositor_connect();
    if (!compositor) {
        fprintf(stderr, "Failed to connect to compositor\n");
        return 1;
    }
    
    // Create text editor
    texteditor_ctx_t* editor = texteditor_create(compositor);
    
    // Open file if specified
    if (argc > 1) {
        texteditor_open_file(editor, argv[1]);
    }
    
    // Run editor (simplified - actual implementation would have event loop)
    printf("Text Editor initialized\n");
    printf("Tab count: %u\n", editor->tab_count);
    printf("Active tab: %u\n", editor->active_tab);
    
    // Cleanup
    texteditor_destroy(editor);
    compositor_disconnect(compositor);
    
    return 0;
}
