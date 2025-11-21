/**
 * @file filemanager.c
 * @brief File Manager Implementation
 */

#include "filemanager.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Helper: Get file extension
static const char* get_extension(const char* filename) {
    const char* dot = strrchr(filename, '.');
    if (!dot || dot == filename) return "";
    return dot + 1;
}

// Helper: Get parent directory
static void get_parent_dir(const char* path, char* parent, size_t parent_size) {
    strncpy(parent, path, parent_size - 1);
    parent[parent_size - 1] = '\0';

    char* last_slash = strrchr(parent, '/');
    if (last_slash && last_slash != parent) {
        *last_slash = '\0';
    } else {
        strcpy(parent, "/");
    }
}

// Initialize pane
static void init_pane(fm_pane_t* pane, filemanager_ctx_t* ctx) {
    memset(pane, 0, sizeof(fm_pane_t));

    // Create first tab
    fm_tab_t* tab = &pane->tabs[0];
    tab->id = 1;
    strcpy(tab->current_path, "/home/user");
    tab->view_mode = VIEW_DETAIL;
    tab->sort_mode = SORT_NAME;
    tab->sort_ascending = true;

    pane->tab_count = 1;
    pane->active_tab = 0;

    // Add to history
    strcpy(pane->history[0], "/home/user");
    pane->history_count = 1;
    pane->history_index = 0;
}

// Create file manager
filemanager_ctx_t* filemanager_create(compositor_ctx_t* compositor) {
    filemanager_ctx_t* ctx = (filemanager_ctx_t*)malloc(sizeof(filemanager_ctx_t));
    if (!ctx) return NULL;

    memset(ctx, 0, sizeof(filemanager_ctx_t));

    ctx->compositor = compositor;
    ctx->dual_pane_mode = false;
    ctx->show_hidden = false;
    ctx->show_preview = true;
    ctx->show_sidebar = true;

    // Create window
    uint32_t width = 1200;
    uint32_t height = 800;
    int32_t x = (compositor->screen_width - width) / 2;
    int32_t y = (compositor->screen_height - height) / 2;

    ctx->fm_window = window_create("File Manager", width, height);
    if (!ctx->fm_window) {
        free(ctx);
        return NULL;
    }

    // Create root panel
    widget_t* root = panel_create();
    widget_set_size(root, width, height);
    widget_set_colors(root, 0xFF000000, 0xFFECF0F1);
    ctx->fm_window->root = root;

    // Create toolbar
    ctx->toolbar = panel_create();
    widget_set_position(ctx->toolbar, 0, 0);
    widget_set_size(ctx->toolbar, width, 50);
    widget_set_colors(ctx->toolbar, 0xFF000000, 0xFFBDC3C7);
    widget_add_child(root, ctx->toolbar);

    // Toolbar buttons
    ctx->btn_back = button_create("< Back");
    widget_set_position(ctx->btn_back, 10, 10);
    widget_set_size(ctx->btn_back, 70, 30);
    widget_add_child(ctx->toolbar, ctx->btn_back);

    ctx->btn_forward = button_create("Forward >");
    widget_set_position(ctx->btn_forward, 90, 10);
    widget_set_size(ctx->btn_forward, 80, 30);
    widget_add_child(ctx->toolbar, ctx->btn_forward);

    ctx->btn_up = button_create("Up");
    widget_set_position(ctx->btn_up, 180, 10);
    widget_set_size(ctx->btn_up, 50, 30);
    widget_add_child(ctx->toolbar, ctx->btn_up);

    ctx->btn_home = button_create("Home");
    widget_set_position(ctx->btn_home, 240, 10);
    widget_set_size(ctx->btn_home, 60, 30);
    widget_add_child(ctx->toolbar, ctx->btn_home);

    ctx->btn_new_folder = button_create("New Folder");
    widget_set_position(ctx->btn_new_folder, 320, 10);
    widget_set_size(ctx->btn_new_folder, 90, 30);
    widget_add_child(ctx->toolbar, ctx->btn_new_folder);

    ctx->btn_copy = button_create("Copy");
    widget_set_position(ctx->btn_copy, 420, 10);
    widget_set_size(ctx->btn_copy, 60, 30);
    widget_add_child(ctx->toolbar, ctx->btn_copy);

    ctx->btn_paste = button_create("Paste");
    widget_set_position(ctx->btn_paste, 490, 10);
    widget_set_size(ctx->btn_paste, 60, 30);
    widget_add_child(ctx->toolbar, ctx->btn_paste);

    ctx->btn_delete = button_create("Delete");
    widget_set_position(ctx->btn_delete, 560, 10);
    widget_set_size(ctx->btn_delete, 60, 30);
    widget_add_child(ctx->toolbar, ctx->btn_delete);

    // Create sidebar for bookmarks
    ctx->sidebar = panel_create();
    widget_set_position(ctx->sidebar, 0, 50);
    widget_set_size(ctx->sidebar, 150, height - 80);
    widget_set_colors(ctx->sidebar, 0xFF000000, 0xFFD5DBDB);
    widget_add_child(root, ctx->sidebar);

    widget_t* sidebar_label = label_create("Bookmarks");
    widget_set_position(sidebar_label, 10, 10);
    widget_add_child(ctx->sidebar, sidebar_label);

    // Initialize panes
    init_pane(&ctx->left_pane, ctx);
    ctx->active_pane = &ctx->left_pane;

    // Create left pane panel
    ctx->left_pane.panel = panel_create();
    widget_set_position(ctx->left_pane.panel, 150, 50);
    widget_set_size(ctx->left_pane.panel, width - 150, height - 80);
    widget_set_colors(ctx->left_pane.panel, 0xFF000000, 0xFFFFFFFF);
    widget_add_child(root, ctx->left_pane.panel);

    // Path bar
    ctx->left_pane.path_bar = text_input_create();
    widget_set_position(ctx->left_pane.path_bar, 10, 10);
    widget_set_size(ctx->left_pane.path_bar, width - 180, 30);
    text_input_set_text(ctx->left_pane.path_bar, "/home/user");
    widget_add_child(ctx->left_pane.panel, ctx->left_pane.path_bar);

    // File list
    ctx->left_pane.file_list = list_create();
    widget_set_position(ctx->left_pane.file_list, 10, 50);
    widget_set_size(ctx->left_pane.file_list, width - 180, height - 150);
    widget_add_child(ctx->left_pane.panel, ctx->left_pane.file_list);

    // Create status bar
    ctx->status_bar = panel_create();
    widget_set_position(ctx->status_bar, 0, height - 30);
    widget_set_size(ctx->status_bar, width, 30);
    widget_set_colors(ctx->status_bar, 0xFF000000, 0xFF95A5A6);
    widget_add_child(root, ctx->status_bar);

    // Add default bookmarks
    filemanager_add_bookmark(ctx, "Home", "/home/user");
    filemanager_add_bookmark(ctx, "Documents", "/home/user/Documents");
    filemanager_add_bookmark(ctx, "Downloads", "/home/user/Downloads");
    filemanager_add_bookmark(ctx, "Pictures", "/home/user/Pictures");
    filemanager_add_bookmark(ctx, "Music", "/home/user/Music");
    filemanager_add_bookmark(ctx, "Videos", "/home/user/Videos");

    ctx->running = true;

    // Load initial directory
    filemanager_load_directory(&ctx->left_pane.tabs[0], "/home/user");

    return ctx;
}

// Destroy file manager
void filemanager_destroy(filemanager_ctx_t* ctx) {
    if (!ctx) return;

    if (ctx->fm_window) {
        window_destroy(ctx->fm_window);
    }

    free(ctx);
}

// Navigate to path
void filemanager_navigate_to(fm_pane_t* pane, const char* path) {
    if (!pane || !path) return;

    fm_tab_t* tab = &pane->tabs[pane->active_tab];

    // Add current path to history
    if (pane->history_count < MAX_HISTORY) {
        strcpy(pane->history[pane->history_count], path);
        pane->history_count++;
        pane->history_index = pane->history_count - 1;
    }

    // Load new directory
    filemanager_load_directory(tab, path);
}

// Navigate back
void filemanager_navigate_back(fm_pane_t* pane) {
    if (!pane || pane->history_index == 0) return;

    pane->history_index--;
    const char* path = pane->history[pane->history_index];

    fm_tab_t* tab = &pane->tabs[pane->active_tab];
    filemanager_load_directory(tab, path);
}

// Navigate forward
void filemanager_navigate_forward(fm_pane_t* pane) {
    if (!pane || pane->history_index >= pane->history_count - 1) return;

    pane->history_index++;
    const char* path = pane->history[pane->history_index];

    fm_tab_t* tab = &pane->tabs[pane->active_tab];
    filemanager_load_directory(tab, path);
}

// Navigate up one directory
void filemanager_navigate_up(fm_pane_t* pane) {
    if (!pane) return;

    fm_tab_t* tab = &pane->tabs[pane->active_tab];

    char parent[512];
    get_parent_dir(tab->current_path, parent, sizeof(parent));

    filemanager_navigate_to(pane, parent);
}

// Navigate to home directory
void filemanager_navigate_home(fm_pane_t* pane) {
    if (!pane) return;

    filemanager_navigate_to(pane, "/home/user");
}

// Refresh current directory
void filemanager_refresh(fm_pane_t* pane) {
    if (!pane) return;

    fm_tab_t* tab = &pane->tabs[pane->active_tab];
    filemanager_load_directory(tab, tab->current_path);
}

// Create new tab
uint32_t filemanager_create_tab(fm_pane_t* pane, const char* path) {
    if (!pane || pane->tab_count >= MAX_TABS) {
        return 0;
    }

    fm_tab_t* tab = &pane->tabs[pane->tab_count];
    tab->id = pane->tab_count + 1;
    tab->view_mode = VIEW_DETAIL;
    tab->sort_mode = SORT_NAME;
    tab->sort_ascending = true;

    if (path) {
        strncpy(tab->current_path, path, 511);
        tab->current_path[511] = '\0';
        filemanager_load_directory(tab, path);
    }

    pane->tab_count++;
    return tab->id;
}

// Close tab
void filemanager_close_tab(fm_pane_t* pane, uint32_t tab_id) {
    if (!pane || pane->tab_count <= 1) return;  // Don't close last tab

    for (uint32_t i = 0; i < pane->tab_count; i++) {
        if (pane->tabs[i].id == tab_id) {
            memmove(&pane->tabs[i],
                    &pane->tabs[i + 1],
                    (pane->tab_count - i - 1) * sizeof(fm_tab_t));
            pane->tab_count--;

            if (pane->active_tab >= pane->tab_count) {
                pane->active_tab = pane->tab_count - 1;
            }

            break;
        }
    }
}

// Switch to tab
void filemanager_switch_tab(fm_pane_t* pane, uint32_t tab_id) {
    if (!pane) return;

    for (uint32_t i = 0; i < pane->tab_count; i++) {
        if (pane->tabs[i].id == tab_id) {
            pane->active_tab = i;
            break;
        }
    }
}

// Load directory contents
void filemanager_load_directory(fm_tab_t* tab, const char* path) {
    if (!tab || !path) return;

    // Update current path
    strncpy(tab->current_path, path, 511);
    tab->current_path[511] = '\0';

    // Clear existing entries
    tab->entry_count = 0;

    // TODO: Read directory via VFS
    // For now, add some dummy entries

    file_entry_t* entry;

    // Add parent directory entry
    entry = &tab->entries[tab->entry_count++];
    strcpy(entry->name, "..");
    strcpy(entry->path, path);
    entry->is_directory = true;
    entry->size = 0;

    // Add some sample entries
    const char* sample_files[] = {
        "Documents", "Downloads", "Pictures", "Music", "Videos",
        "README.txt", "notes.txt", "image.png", "video.mp4", "song.mp3"
    };

    for (uint32_t i = 0; i < 10 && tab->entry_count < MAX_FILE_ENTRIES; i++) {
        entry = &tab->entries[tab->entry_count++];
        strncpy(entry->name, sample_files[i], 255);
        entry->name[255] = '\0';

        snprintf(entry->path, 511, "%s/%s", path, sample_files[i]);

        entry->is_directory = (i < 5);  // First 5 are directories
        entry->size = entry->is_directory ? 0 : 1024 * (i + 1);
        entry->is_hidden = false;
        entry->selected = false;
    }

    // Sort entries
    filemanager_sort_entries(tab);
}

// Sort file entries
void filemanager_sort_entries(fm_tab_t* tab) {
    if (!tab || tab->entry_count <= 1) return;

    // Simple bubble sort (TODO: use qsort)
    for (uint32_t i = 1; i < tab->entry_count; i++) {
        for (uint32_t j = i + 1; j < tab->entry_count; j++) {
            bool swap = false;

            switch (tab->sort_mode) {
                case SORT_NAME:
                    swap = strcmp(tab->entries[i].name, tab->entries[j].name) > 0;
                    break;
                case SORT_SIZE:
                    swap = tab->entries[i].size > tab->entries[j].size;
                    break;
                case SORT_TYPE:
                    swap = strcmp(get_extension(tab->entries[i].name),
                                  get_extension(tab->entries[j].name)) > 0;
                    break;
                case SORT_MODIFIED:
                    swap = tab->entries[i].modified_time > tab->entries[j].modified_time;
                    break;
            }

            if (!tab->sort_ascending) swap = !swap;

            if (swap) {
                file_entry_t temp = tab->entries[i];
                tab->entries[i] = tab->entries[j];
                tab->entries[j] = temp;
            }
        }
    }
}

// Copy files to clipboard
void filemanager_copy_files(filemanager_ctx_t* ctx) {
    if (!ctx) return;

    fm_tab_t* tab = &ctx->active_pane->tabs[ctx->active_pane->active_tab];

    ctx->clipboard_count = 0;
    ctx->clipboard_operation = OP_COPY;

    for (uint32_t i = 0; i < tab->entry_count; i++) {
        if (tab->entries[i].selected && ctx->clipboard_count < 64) {
            strncpy(ctx->clipboard_paths[ctx->clipboard_count], tab->entries[i].path, 511);
            ctx->clipboard_paths[ctx->clipboard_count][511] = '\0';
            ctx->clipboard_count++;
        }
    }
}

// Cut files to clipboard
void filemanager_cut_files(filemanager_ctx_t* ctx) {
    if (!ctx) return;

    filemanager_copy_files(ctx);
    ctx->clipboard_operation = OP_MOVE;
}

// Paste files from clipboard
void filemanager_paste_files(filemanager_ctx_t* ctx) {
    if (!ctx || ctx->clipboard_count == 0) return;

    fm_tab_t* tab = &ctx->active_pane->tabs[ctx->active_pane->active_tab];
    const char* dest_dir = tab->current_path;

    for (uint32_t i = 0; i < ctx->clipboard_count; i++) {
        // TODO: Implement file copy/move via VFS
        printf("Paste: %s to %s (operation: %d)\n",
               ctx->clipboard_paths[i], dest_dir, ctx->clipboard_operation);
    }

    if (ctx->clipboard_operation == OP_MOVE) {
        ctx->clipboard_count = 0;
    }

    filemanager_refresh(ctx->active_pane);
}

// Delete selected files
void filemanager_delete_files(filemanager_ctx_t* ctx) {
    if (!ctx) return;

    fm_tab_t* tab = &ctx->active_pane->tabs[ctx->active_pane->active_tab];

    for (uint32_t i = 0; i < tab->entry_count; i++) {
        if (tab->entries[i].selected) {
            // TODO: Delete file via VFS
            printf("Delete: %s\n", tab->entries[i].path);
        }
    }

    filemanager_refresh(ctx->active_pane);
}

// Rename file
void filemanager_rename_file(filemanager_ctx_t* ctx, const char* old_name, const char* new_name) {
    if (!ctx || !old_name || !new_name) return;

    // TODO: Rename file via VFS
    printf("Rename: %s to %s\n", old_name, new_name);

    filemanager_refresh(ctx->active_pane);
}

// Create new folder
void filemanager_create_folder(filemanager_ctx_t* ctx, const char* name) {
    if (!ctx || !name) return;

    fm_tab_t* tab = &ctx->active_pane->tabs[ctx->active_pane->active_tab];

    char path[512];
    snprintf(path, 511, "%s/%s", tab->current_path, name);

    // TODO: Create directory via VFS
    printf("Create folder: %s\n", path);

    filemanager_refresh(ctx->active_pane);
}

// Move to trash
void filemanager_move_to_trash(filemanager_ctx_t* ctx) {
    if (!ctx) return;

    // TODO: Move selected files to trash
    filemanager_delete_files(ctx);
}

// Select file
void filemanager_select_file(fm_tab_t* tab, uint32_t index, bool multi) {
    if (!tab || index >= tab->entry_count) return;

    if (!multi) {
        // Clear all selections
        for (uint32_t i = 0; i < tab->entry_count; i++) {
            tab->entries[i].selected = false;
        }
    }

    tab->entries[index].selected = !tab->entries[index].selected;
}

// Select all files
void filemanager_select_all(fm_tab_t* tab) {
    if (!tab) return;

    for (uint32_t i = 0; i < tab->entry_count; i++) {
        tab->entries[i].selected = true;
    }
}

// Deselect all files
void filemanager_deselect_all(fm_tab_t* tab) {
    if (!tab) return;

    for (uint32_t i = 0; i < tab->entry_count; i++) {
        tab->entries[i].selected = false;
    }
}

// Add bookmark
void filemanager_add_bookmark(filemanager_ctx_t* ctx, const char* name, const char* path) {
    if (!ctx || !name || !path || ctx->bookmark_count >= MAX_BOOKMARKS) {
        return;
    }

    bookmark_t* bm = &ctx->bookmarks[ctx->bookmark_count];
    strncpy(bm->name, name, 63);
    bm->name[63] = '\0';

    strncpy(bm->path, path, 511);
    bm->path[511] = '\0';

    ctx->bookmark_count++;

    // TODO: Add bookmark button to sidebar
}

// Remove bookmark
void filemanager_remove_bookmark(filemanager_ctx_t* ctx, uint32_t index) {
    if (!ctx || index >= ctx->bookmark_count) return;

    memmove(&ctx->bookmarks[index],
            &ctx->bookmarks[index + 1],
            (ctx->bookmark_count - index - 1) * sizeof(bookmark_t));
    ctx->bookmark_count--;
}

// Go to bookmark
void filemanager_goto_bookmark(filemanager_ctx_t* ctx, uint32_t index) {
    if (!ctx || index >= ctx->bookmark_count) return;

    filemanager_navigate_to(ctx->active_pane, ctx->bookmarks[index].path);
}

// Start search
void filemanager_start_search(filemanager_ctx_t* ctx, const char* query) {
    if (!ctx || !query) return;

    strncpy(ctx->search_query, query, 255);
    ctx->search_query[255] = '\0';
    ctx->search_active = true;

    // TODO: Implement file search
}

// Stop search
void filemanager_stop_search(filemanager_ctx_t* ctx) {
    if (!ctx) return;

    ctx->search_active = false;
    ctx->search_query[0] = '\0';
    filemanager_refresh(ctx->active_pane);
}

// Set view mode
void filemanager_set_view_mode(fm_tab_t* tab, view_mode_t mode) {
    if (!tab) return;
    tab->view_mode = mode;
}

// Toggle dual pane mode
void filemanager_toggle_dual_pane(filemanager_ctx_t* ctx) {
    if (!ctx) return;

    ctx->dual_pane_mode = !ctx->dual_pane_mode;

    if (ctx->dual_pane_mode) {
        init_pane(&ctx->right_pane, ctx);
        // TODO: Create right pane widgets
    } else {
        // TODO: Hide right pane widgets
    }
}

// Toggle hidden files
void filemanager_toggle_hidden_files(filemanager_ctx_t* ctx) {
    if (!ctx) return;

    ctx->show_hidden = !ctx->show_hidden;
    filemanager_refresh(ctx->active_pane);
}

// Toggle preview pane
void filemanager_toggle_preview(filemanager_ctx_t* ctx) {
    if (!ctx) return;

    ctx->show_preview = !ctx->show_preview;
    // TODO: Show/hide preview panel
}

// File double-clicked
void filemanager_file_double_clicked(filemanager_ctx_t* ctx, const char* path) {
    if (!ctx || !path) return;

    // TODO: Check if directory or file
    // If directory: navigate to it
    // If file: open with default application

    filemanager_navigate_to(ctx->active_pane, path);
}

// File right-clicked
void filemanager_file_right_clicked(filemanager_ctx_t* ctx, const char* path, int32_t x, int32_t y) {
    if (!ctx || !path) return;

    // TODO: Show context menu with options:
    // - Open
    // - Open With...
    // - Cut
    // - Copy
    // - Delete
    // - Rename
    // - Properties
}

// Render file manager
void filemanager_render(filemanager_ctx_t* ctx) {
    if (!ctx || !ctx->fm_window) return;

    filemanager_render_pane(ctx, ctx->active_pane);

    if (ctx->dual_pane_mode) {
        fm_pane_t* other_pane = (ctx->active_pane == &ctx->left_pane) ?
                                 &ctx->right_pane : &ctx->left_pane;
        filemanager_render_pane(ctx, other_pane);
    }

    window_render(ctx->fm_window);
}

// Render pane
void filemanager_render_pane(filemanager_ctx_t* ctx, fm_pane_t* pane) {
    if (!ctx || !pane) return;

    fm_tab_t* tab = &pane->tabs[pane->active_tab];

    // Update file list widget
    list_clear(pane->file_list);

    for (uint32_t i = 0; i < tab->entry_count; i++) {
        file_entry_t* entry = &tab->entries[i];

        // Skip hidden files if not showing them
        if (entry->is_hidden && !ctx->show_hidden) {
            continue;
        }

        char display_name[512];
        if (entry->is_directory) {
            snprintf(display_name, 511, "[%s]", entry->name);
        } else {
            snprintf(display_name, 511, "%s (%llu bytes)", entry->name, entry->size);
        }

        list_add_item(pane->file_list, display_name);
    }
}

// Render preview
void filemanager_render_preview(filemanager_ctx_t* ctx, const char* file_path) {
    if (!ctx || !file_path || !ctx->show_preview) return;

    // TODO: Render file preview based on type
    // - Images: show thumbnail
    // - Text files: show first few lines
    // - Other: show file info
}

// Main file manager loop
void filemanager_run(filemanager_ctx_t* ctx) {
    if (!ctx) return;

    while (ctx->running) {
        // TODO: Process events

        filemanager_render(ctx);

        // TODO: Sleep or yield CPU
    }
}
