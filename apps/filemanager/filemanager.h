/**
 * @file filemanager.h
 * @brief File Manager for Scarlett OS
 *
 * Dual-pane file manager with tabs, bookmarks, and advanced features
 */

#ifndef APPS_FILEMANAGER_H
#define APPS_FILEMANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "../../gui/compositor/src/compositor.h"
#include "../../gui/widgets/src/widgets.h"

// File manager configuration
#define MAX_FILE_ENTRIES 1024
#define MAX_BOOKMARKS 32
#define MAX_TABS 16
#define MAX_HISTORY 100

// View modes
typedef enum {
    VIEW_ICON = 0,
    VIEW_LIST = 1,
    VIEW_DETAIL = 2,
    VIEW_TREE = 3,
} view_mode_t;

// Sort modes
typedef enum {
    SORT_NAME = 0,
    SORT_SIZE = 1,
    SORT_TYPE = 2,
    SORT_MODIFIED = 3,
} sort_mode_t;

// File entry
typedef struct {
    char name[256];
    char path[512];
    uint64_t size;
    uint64_t modified_time;
    bool is_directory;
    bool is_hidden;
    bool selected;
    void* icon;
} file_entry_t;

// Bookmark
typedef struct {
    char name[64];
    char path[512];
} bookmark_t;

// Tab
typedef struct {
    uint32_t id;
    char current_path[512];
    file_entry_t entries[MAX_FILE_ENTRIES];
    uint32_t entry_count;
    view_mode_t view_mode;
    sort_mode_t sort_mode;
    bool sort_ascending;
    uint32_t scroll_offset;
    char* selected_files[MAX_FILE_ENTRIES];
    uint32_t selection_count;
} fm_tab_t;

// File operation
typedef enum {
    OP_COPY,
    OP_MOVE,
    OP_DELETE,
    OP_RENAME,
} file_operation_t;

// Pane
typedef struct {
    fm_tab_t tabs[MAX_TABS];
    uint32_t tab_count;
    uint32_t active_tab;

    widget_t* panel;
    widget_t* path_bar;
    widget_t* file_list;
    widget_t* tab_bar;

    char history[MAX_HISTORY][512];
    uint32_t history_count;
    uint32_t history_index;
} fm_pane_t;

// File manager context
typedef struct {
    compositor_ctx_t* compositor;
    window_t* fm_window;

    fm_pane_t left_pane;
    fm_pane_t right_pane;
    fm_pane_t* active_pane;

    bookmark_t bookmarks[MAX_BOOKMARKS];
    uint32_t bookmark_count;

    // Widgets
    widget_t* toolbar;
    widget_t* sidebar;
    widget_t* preview_panel;
    widget_t* status_bar;

    // Toolbar buttons
    widget_t* btn_back;
    widget_t* btn_forward;
    widget_t* btn_up;
    widget_t* btn_home;
    widget_t* btn_new_folder;
    widget_t* btn_delete;
    widget_t* btn_copy;
    widget_t* btn_cut;
    widget_t* btn_paste;
    widget_t* btn_view_mode;

    // Search
    widget_t* search_box;
    char search_query[256];
    bool search_active;

    // Clipboard
    char clipboard_paths[64][512];
    uint32_t clipboard_count;
    file_operation_t clipboard_operation;

    // Display options
    bool dual_pane_mode;
    bool show_hidden;
    bool show_preview;
    bool show_sidebar;

    bool running;
} filemanager_ctx_t;

// File manager initialization
filemanager_ctx_t* filemanager_create(compositor_ctx_t* compositor);
void filemanager_destroy(filemanager_ctx_t* ctx);
void filemanager_run(filemanager_ctx_t* ctx);

// Navigation
void filemanager_navigate_to(fm_pane_t* pane, const char* path);
void filemanager_navigate_back(fm_pane_t* pane);
void filemanager_navigate_forward(fm_pane_t* pane);
void filemanager_navigate_up(fm_pane_t* pane);
void filemanager_navigate_home(fm_pane_t* pane);
void filemanager_refresh(fm_pane_t* pane);

// Tab management
uint32_t filemanager_create_tab(fm_pane_t* pane, const char* path);
void filemanager_close_tab(fm_pane_t* pane, uint32_t tab_id);
void filemanager_switch_tab(fm_pane_t* pane, uint32_t tab_id);

// Directory operations
void filemanager_load_directory(fm_tab_t* tab, const char* path);
void filemanager_sort_entries(fm_tab_t* tab);

// File operations
void filemanager_copy_files(filemanager_ctx_t* ctx);
void filemanager_cut_files(filemanager_ctx_t* ctx);
void filemanager_paste_files(filemanager_ctx_t* ctx);
void filemanager_delete_files(filemanager_ctx_t* ctx);
void filemanager_rename_file(filemanager_ctx_t* ctx, const char* old_name, const char* new_name);
void filemanager_create_folder(filemanager_ctx_t* ctx, const char* name);
void filemanager_move_to_trash(filemanager_ctx_t* ctx);

// Selection
void filemanager_select_file(fm_tab_t* tab, uint32_t index, bool multi);
void filemanager_select_all(fm_tab_t* tab);
void filemanager_deselect_all(fm_tab_t* tab);

// Bookmarks
void filemanager_add_bookmark(filemanager_ctx_t* ctx, const char* name, const char* path);
void filemanager_remove_bookmark(filemanager_ctx_t* ctx, uint32_t index);
void filemanager_goto_bookmark(filemanager_ctx_t* ctx, uint32_t index);

// Search
void filemanager_start_search(filemanager_ctx_t* ctx, const char* query);
void filemanager_stop_search(filemanager_ctx_t* ctx);

// View options
void filemanager_set_view_mode(fm_tab_t* tab, view_mode_t mode);
void filemanager_toggle_dual_pane(filemanager_ctx_t* ctx);
void filemanager_toggle_hidden_files(filemanager_ctx_t* ctx);
void filemanager_toggle_preview(filemanager_ctx_t* ctx);

// Callbacks
void filemanager_file_double_clicked(filemanager_ctx_t* ctx, const char* path);
void filemanager_file_right_clicked(filemanager_ctx_t* ctx, const char* path, int32_t x, int32_t y);

// Rendering
void filemanager_render(filemanager_ctx_t* ctx);
void filemanager_render_pane(filemanager_ctx_t* ctx, fm_pane_t* pane);
void filemanager_render_preview(filemanager_ctx_t* ctx, const char* file_path);

#endif // APPS_FILEMANAGER_H
