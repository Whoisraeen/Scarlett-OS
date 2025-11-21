/**
 * @file editor.h
 * @brief Text Editor for Scarlett OS
 *
 * Full-featured text editor with syntax highlighting, search/replace, and code folding
 */

#ifndef APPS_EDITOR_H
#define APPS_EDITOR_H

#include <stdint.h>
#include <stdbool.h>
#include "../../gui/compositor/src/compositor.h"
#include "../../gui/widgets/src/widgets.h"

// Editor configuration
#define MAX_LINES 100000
#define MAX_LINE_LENGTH 4096
#define MAX_TABS 16
#define MAX_UNDO_LEVELS 1000
#define MAX_SEARCH_RESULTS 1000

// Language types for syntax highlighting
typedef enum {
    LANG_NONE = 0,
    LANG_C,
    LANG_CPP,
    LANG_RUST,
    LANG_PYTHON,
    LANG_JAVASCRIPT,
    LANG_HTML,
    LANG_CSS,
    LANG_JSON,
    LANG_XML,
    LANG_MARKDOWN,
    LANG_SHELL,
    LANG_ASSEMBLY,
    LANG_GO,
    LANG_JAVA,
    LANG_SQL,
} language_t;

// Token types for syntax highlighting
typedef enum {
    TOKEN_NORMAL = 0,
    TOKEN_KEYWORD,
    TOKEN_TYPE,
    TOKEN_STRING,
    TOKEN_NUMBER,
    TOKEN_COMMENT,
    TOKEN_PREPROCESSOR,
    TOKEN_OPERATOR,
    TOKEN_IDENTIFIER,
    TOKEN_FUNCTION,
} token_type_t;

// Text line
typedef struct {
    char* content;
    uint32_t length;
    uint32_t capacity;
    token_type_t* tokens;  // Token type for each character
    bool dirty;            // Needs re-highlighting
} text_line_t;

// Undo/redo action
typedef enum {
    ACTION_INSERT_CHAR,
    ACTION_DELETE_CHAR,
    ACTION_INSERT_LINE,
    ACTION_DELETE_LINE,
    ACTION_REPLACE,
} action_type_t;

typedef struct {
    action_type_t type;
    uint32_t line;
    uint32_t column;
    char* data;
    uint32_t data_len;
} undo_action_t;

// Search result
typedef struct {
    uint32_t line;
    uint32_t column;
    uint32_t length;
} search_result_t;

// Editor buffer (one per tab)
typedef struct {
    char file_path[512];
    bool modified;
    bool read_only;

    text_line_t* lines;
    uint32_t line_count;
    uint32_t line_capacity;

    uint32_t cursor_line;
    uint32_t cursor_column;

    // Selection
    bool has_selection;
    uint32_t sel_start_line;
    uint32_t sel_start_column;
    uint32_t sel_end_line;
    uint32_t sel_end_column;

    // Undo/redo
    undo_action_t undo_stack[MAX_UNDO_LEVELS];
    uint32_t undo_count;
    uint32_t undo_index;

    // Language and highlighting
    language_t language;
    bool auto_detect_language;

    // View state
    uint32_t scroll_line;
    uint32_t scroll_column;

} editor_buffer_t;

// Editor tab
typedef struct {
    uint32_t id;
    char title[64];
    editor_buffer_t* buffer;
    widget_t* tab_button;
} editor_tab_t;

// Editor theme
typedef struct {
    const char* name;
    uint32_t background;
    uint32_t foreground;
    uint32_t line_number_bg;
    uint32_t line_number_fg;
    uint32_t cursor_line_bg;
    uint32_t selection_bg;
    uint32_t keyword_color;
    uint32_t type_color;
    uint32_t string_color;
    uint32_t number_color;
    uint32_t comment_color;
    uint32_t preprocessor_color;
    uint32_t operator_color;
    uint32_t function_color;
} editor_theme_t;

// Editor context
typedef struct {
    compositor_ctx_t* compositor;
    window_t* editor_window;

    editor_tab_t tabs[MAX_TABS];
    uint32_t tab_count;
    uint32_t active_tab;

    // UI components
    widget_t* menu_bar;
    widget_t* toolbar;
    widget_t* tab_bar;
    widget_t* editor_panel;
    widget_t* line_numbers;
    widget_t* status_bar;
    widget_t* search_bar;

    // Font settings
    const char* font_name;
    uint32_t font_size;
    uint32_t char_width;
    uint32_t char_height;
    bool show_line_numbers;
    uint32_t tab_width;  // Number of spaces for tab

    // Theme
    editor_theme_t* current_theme;
    editor_theme_t themes[8];
    uint32_t theme_count;

    // Search state
    bool search_visible;
    char search_query[256];
    bool search_regex;
    bool search_case_sensitive;
    search_result_t search_results[MAX_SEARCH_RESULTS];
    uint32_t search_result_count;
    uint32_t current_search_result;

    // Replace state
    bool replace_visible;
    char replace_text[256];

    // Settings
    bool auto_indent;
    bool show_whitespace;
    bool word_wrap;
    bool auto_save;
    uint32_t auto_save_interval;  // seconds

    // Split view
    bool split_view_enabled;
    bool split_vertical;  // true = vertical split, false = horizontal
    uint32_t split_tab_id;

    bool running;
} editor_ctx_t;

// Editor initialization
editor_ctx_t* editor_create(compositor_ctx_t* compositor);
void editor_destroy(editor_ctx_t* ctx);
void editor_run(editor_ctx_t* ctx);

// Tab management
uint32_t editor_create_tab(editor_ctx_t* ctx, const char* file_path);
void editor_close_tab(editor_ctx_t* ctx, uint32_t tab_id);
void editor_switch_tab(editor_ctx_t* ctx, uint32_t tab_id);

// File operations
bool editor_load_file(editor_buffer_t* buf, const char* file_path);
bool editor_save_file(editor_buffer_t* buf, const char* file_path);
bool editor_save_as(editor_buffer_t* buf, const char* file_path);
void editor_new_file(editor_buffer_t* buf);

// Buffer operations
void editor_insert_char(editor_buffer_t* buf, char ch);
void editor_delete_char(editor_buffer_t* buf);
void editor_insert_line(editor_buffer_t* buf);
void editor_delete_line(editor_buffer_t* buf, uint32_t line);
void editor_insert_text(editor_buffer_t* buf, const char* text, uint32_t len);

// Cursor movement
void editor_move_cursor(editor_buffer_t* buf, int32_t dx, int32_t dy);
void editor_move_cursor_to(editor_buffer_t* buf, uint32_t line, uint32_t column);
void editor_move_to_line_start(editor_buffer_t* buf);
void editor_move_to_line_end(editor_buffer_t* buf);
void editor_move_to_file_start(editor_buffer_t* buf);
void editor_move_to_file_end(editor_buffer_t* buf);
void editor_page_up(editor_buffer_t* buf);
void editor_page_down(editor_buffer_t* buf);

// Selection
void editor_select(editor_buffer_t* buf, uint32_t start_line, uint32_t start_col, uint32_t end_line, uint32_t end_col);
void editor_select_all(editor_buffer_t* buf);
void editor_select_word(editor_buffer_t* buf);
void editor_select_line(editor_buffer_t* buf);
void editor_clear_selection(editor_buffer_t* buf);
char* editor_get_selection(editor_buffer_t* buf);

// Clipboard operations
void editor_copy(editor_ctx_t* ctx);
void editor_cut(editor_ctx_t* ctx);
void editor_paste(editor_ctx_t* ctx);

// Undo/redo
void editor_undo(editor_buffer_t* buf);
void editor_redo(editor_buffer_t* buf);
void editor_record_action(editor_buffer_t* buf, action_type_t type, uint32_t line, uint32_t col, const char* data, uint32_t len);

// Search and replace
void editor_search(editor_ctx_t* ctx, const char* query, bool regex, bool case_sensitive);
void editor_find_next(editor_ctx_t* ctx);
void editor_find_previous(editor_ctx_t* ctx);
void editor_replace_current(editor_ctx_t* ctx, const char* replacement);
void editor_replace_all(editor_ctx_t* ctx, const char* replacement);

// Syntax highlighting
void editor_detect_language(editor_buffer_t* buf, const char* file_path);
void editor_highlight_line(editor_buffer_t* buf, uint32_t line);
void editor_highlight_all(editor_buffer_t* buf);
language_t editor_detect_from_extension(const char* file_path);

// Themes
void editor_load_themes(editor_ctx_t* ctx);
void editor_set_theme(editor_ctx_t* ctx, uint32_t theme_index);

// Auto-indent
void editor_auto_indent(editor_buffer_t* buf);
uint32_t editor_get_indent_level(const char* line);

// Code folding (stub for future)
void editor_toggle_fold(editor_buffer_t* buf, uint32_t line);

// Split view
void editor_toggle_split_view(editor_ctx_t* ctx, bool vertical);
void editor_close_split_view(editor_ctx_t* ctx);

// Rendering
void editor_render(editor_ctx_t* ctx);
void editor_render_buffer(editor_ctx_t* ctx, editor_buffer_t* buf, int32_t x, int32_t y, uint32_t width, uint32_t height);
void editor_render_line_numbers(editor_ctx_t* ctx, editor_buffer_t* buf, int32_t x, int32_t y, uint32_t height);
void editor_render_cursor(editor_ctx_t* ctx, editor_buffer_t* buf, int32_t x, int32_t y);

// Input handling
void editor_handle_key(editor_ctx_t* ctx, uint32_t keycode, uint32_t modifiers, bool pressed);
void editor_handle_char(editor_ctx_t* ctx, uint32_t codepoint);

#endif // APPS_EDITOR_H
