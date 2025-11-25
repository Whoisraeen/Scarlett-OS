/**
 * @file texteditor.h
 * @brief Advanced Text Editor for Scarlett OS
 *
 * Multi-tab text editor with syntax highlighting, search/replace, code folding,
 * and auto-completion support
 */

#ifndef APPS_TEXTEDITOR_H
#define APPS_TEXTEDITOR_H

#include <stdint.h>
#include <stdbool.h>
#include "../../gui/compositor/src/compositor.h"
#include "../../gui/widgets/src/widgets.h"

// Editor configuration
#define MAX_LINE_LENGTH 4096
#define MAX_LINES 100000
#define MAX_TABS 16
#define MAX_UNDO_LEVELS 1000
#define MAX_LANGUAGES 32

// Syntax highlighting token types
typedef enum {
    TOKEN_NORMAL = 0,
    TOKEN_KEYWORD,
    TOKEN_TYPE,
    TOKEN_STRING,
    TOKEN_COMMENT,
    TOKEN_NUMBER,
    TOKEN_OPERATOR,
    TOKEN_PREPROCESSOR,
    TOKEN_FUNCTION,
    TOKEN_VARIABLE,
} token_type_t;

// Syntax token
typedef struct {
    token_type_t type;
    uint32_t start;
    uint32_t length;
} syntax_token_t;

// Line structure
typedef struct {
    char* text;
    uint32_t length;
    uint32_t capacity;
    syntax_token_t* tokens;
    uint32_t token_count;
    bool folded;
    uint32_t fold_level;
} editor_line_t;

// Language definition
typedef struct {
    const char* name;
    const char** keywords;
    uint32_t keyword_count;
    const char** types;
    uint32_t type_count;
    const char* line_comment;
    const char* block_comment_start;
    const char* block_comment_end;
    const char** file_extensions;
    uint32_t extension_count;
} language_def_t;

// Undo/Redo action types
typedef enum {
    ACTION_INSERT_CHAR,
    ACTION_DELETE_CHAR,
    ACTION_INSERT_LINE,
    ACTION_DELETE_LINE,
    ACTION_REPLACE_TEXT,
} action_type_t;

// Undo/Redo action
typedef struct {
    action_type_t type;
    uint32_t line;
    uint32_t column;
    char* old_text; // For insert/delete, this is the text inserted/deleted. For replace, this is the old text.
    char* new_text; // For replace, this is the new text. NULL for other actions.
    uint32_t old_text_len;
    uint32_t new_text_len;
} editor_action_t;

// Text selection
typedef struct {
    uint32_t start_line;
    uint32_t start_column;
    uint32_t end_line;
    uint32_t end_column;
    bool active;
} text_selection_t;

// Auto-completion entry
typedef struct {
    char text[128];
    char description[256];
    token_type_t type;
} completion_entry_t;

// Search/Replace context
typedef struct {
    char search_text[256];
    char replace_text[256];
    bool case_sensitive;
    bool whole_word;
    bool use_regex;
    uint32_t match_count;
    uint32_t current_match;
} search_ctx_t;

// Editor buffer
typedef struct {
    editor_line_t* lines;
    uint32_t line_count;
    uint32_t line_capacity;
    
    uint32_t cursor_line;
    uint32_t cursor_column;
    uint32_t scroll_line;
    uint32_t scroll_column;
    
    text_selection_t selection;
    
    // Undo/Redo stacks
    editor_action_t undo_stack[MAX_UNDO_LEVELS];
    uint32_t undo_count;
    uint32_t undo_position;
    
    // File info
    char file_path[512];
    bool modified;
    bool read_only;
    
    // Syntax highlighting
    language_def_t* language;
    
    // Settings
    bool show_line_numbers;
    bool auto_indent;
    bool highlight_current_line;
    uint32_t tab_size;
    bool use_spaces_for_tabs;
} editor_buffer_t;

// Editor tab
typedef struct {
    uint32_t id;
    char title[128];
    editor_buffer_t buffer;
    widget_t* tab_button;
    bool split_view;
    editor_buffer_t* split_buffer;  // For split view
} editor_tab_t;

// Editor context
typedef struct {
    compositor_ctx_t* compositor;
    window_t* editor_window;
    
    editor_tab_t tabs[MAX_TABS];
    uint32_t tab_count;
    uint32_t active_tab;
    
    // Language definitions
    language_def_t languages[MAX_LANGUAGES];
    uint32_t language_count;
    
    // Search/Replace
    search_ctx_t search;
    widget_t* search_panel;
    
    // Auto-completion
    completion_entry_t completions[256];
    uint32_t completion_count;
    widget_t* completion_popup;
    bool completion_active;
    
    // Widgets
    widget_t* tab_bar;
    widget_t* toolbar;
    widget_t* editor_panel;
    widget_t* status_bar;
    widget_t* line_number_panel;
    
    // Toolbar buttons
    widget_t* btn_new;
    widget_t* btn_open;
    widget_t* btn_save;
    widget_t* btn_undo;
    widget_t* btn_redo;
    widget_t* btn_cut;
    widget_t* btn_copy;
    widget_t* btn_paste;
    widget_t* btn_find;
    widget_t* btn_replace;
    
    // Font settings
    const char* font_name;
    uint32_t font_size;
    uint32_t char_width;
    uint32_t char_height;
    
    // Theme colors
    uint32_t bg_color;
    uint32_t fg_color;
    uint32_t line_number_color;
    uint32_t current_line_color;
    uint32_t selection_color;
    uint32_t token_colors[16];
    
    bool running;
} texteditor_ctx_t;

// Editor initialization
texteditor_ctx_t* texteditor_create(compositor_ctx_t* compositor);
void texteditor_destroy(texteditor_ctx_t* ctx);
void texteditor_run(texteditor_ctx_t* ctx);

// Tab management
uint32_t texteditor_create_tab(texteditor_ctx_t* ctx, const char* title);
void texteditor_close_tab(texteditor_ctx_t* ctx, uint32_t tab_id);
void texteditor_switch_tab(texteditor_ctx_t* ctx, uint32_t tab_id);

// File operations
bool texteditor_open_file(texteditor_ctx_t* ctx, const char* path);
bool texteditor_save_file(texteditor_ctx_t* ctx);
bool texteditor_save_file_as(texteditor_ctx_t* ctx, const char* path);
void texteditor_new_file(texteditor_ctx_t* ctx);

// Buffer operations
void editor_buffer_init(editor_buffer_t* buf);
void editor_buffer_destroy(editor_buffer_t* buf);
void editor_insert_char(editor_buffer_t* buf, char c);
void editor_delete_char(editor_buffer_t* buf);
void editor_insert_line(editor_buffer_t* buf);
void editor_delete_line(editor_buffer_t* buf, uint32_t line);
void editor_insert_text(editor_buffer_t* buf, const char* text, uint32_t len);
void editor_delete_selection(editor_buffer_t* buf);

// Cursor movement
void editor_move_cursor(editor_buffer_t* buf, int32_t dx, int32_t dy);
void editor_move_cursor_to(editor_buffer_t* buf, uint32_t line, uint32_t column);
void editor_move_to_line_start(editor_buffer_t* buf);
void editor_move_to_line_end(editor_buffer_t* buf);
void editor_move_to_document_start(editor_buffer_t* buf);
void editor_move_to_document_end(editor_buffer_t* buf);
void editor_page_up(editor_buffer_t* buf);
void editor_page_down(editor_buffer_t* buf);

// Selection
void editor_start_selection(editor_buffer_t* buf);
void editor_end_selection(editor_buffer_t* buf);
void editor_select_all(editor_buffer_t* buf);
void editor_select_word(editor_buffer_t* buf);
void editor_select_line(editor_buffer_t* buf);
char* editor_get_selected_text(editor_buffer_t* buf);

// Clipboard operations
void texteditor_copy(texteditor_ctx_t* ctx);
void texteditor_cut(texteditor_ctx_t* ctx);
void texteditor_paste(texteditor_ctx_t* ctx);

// Undo/Redo
void editor_undo(editor_buffer_t* buf);
void editor_redo(editor_buffer_t* buf);
void editor_push_undo(editor_buffer_t* buf, action_type_t type, uint32_t line, uint32_t col, const char* text, uint32_t len);

// Search/Replace
void texteditor_start_search(texteditor_ctx_t* ctx, const char* query);
void texteditor_find_next(texteditor_ctx_t* ctx);
void texteditor_find_previous(texteditor_ctx_t* ctx);
void texteditor_replace_current(texteditor_ctx_t* ctx);
void texteditor_replace_all(texteditor_ctx_t* ctx);

// Syntax highlighting
void editor_highlight_line(editor_buffer_t* buf, uint32_t line);
void editor_highlight_all(editor_buffer_t* buf);
void editor_set_language(editor_buffer_t* buf, language_def_t* lang);
language_def_t* editor_detect_language(texteditor_ctx_t* ctx, const char* filename);

// Code folding
void editor_toggle_fold(editor_buffer_t* buf, uint32_t line);
void editor_fold_all(editor_buffer_t* buf);
void editor_unfold_all(editor_buffer_t* buf);
uint32_t editor_calculate_fold_level(const char* line);

// Auto-completion
void texteditor_trigger_completion(texteditor_ctx_t* ctx);
void texteditor_accept_completion(texteditor_ctx_t* ctx, uint32_t index);
void texteditor_cancel_completion(texteditor_ctx_t* ctx);
void texteditor_build_completions(texteditor_ctx_t* ctx, const char* prefix);

// Auto-indent
void editor_auto_indent(editor_buffer_t* buf);
uint32_t editor_calculate_indent_level(const char* line);

// Input handling
void texteditor_handle_key(texteditor_ctx_t* ctx, uint32_t keycode, uint32_t modifiers, bool pressed);
void texteditor_handle_char(texteditor_ctx_t* ctx, uint32_t codepoint);

// Rendering
void texteditor_render(texteditor_ctx_t* ctx);
void texteditor_render_buffer(texteditor_ctx_t* ctx, editor_buffer_t* buf, uint32_t x, uint32_t y, uint32_t width, uint32_t height);
void texteditor_render_line_numbers(texteditor_ctx_t* ctx, editor_buffer_t* buf, uint32_t x, uint32_t y, uint32_t height);
void texteditor_render_cursor(texteditor_ctx_t* ctx, editor_buffer_t* buf);

// Language definitions
void texteditor_load_languages(texteditor_ctx_t* ctx);
void texteditor_add_language(texteditor_ctx_t* ctx, const char* name, const char** keywords, uint32_t kw_count,
                             const char** types, uint32_t type_count, const char* line_comment,
                             const char* block_start, const char* block_end,
                             const char** extensions, uint32_t ext_count);

// Split view
void texteditor_split_horizontal(texteditor_ctx_t* ctx);
void texteditor_split_vertical(texteditor_ctx_t* ctx);
void texteditor_close_split(texteditor_ctx_t* ctx);

#endif // APPS_TEXTEDITOR_H
