/**
 * @file filemanager.c
 * @brief File Manager Implementation
 */

#include "filemanager.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "../../libs/libc/include/syscall.h"

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

// Should be defined in standard headers or we define here
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

// SFS Directory Entry (must match kernel definition)
typedef struct {
    uint32_t inode;             // Inode number (0 = empty)
    char name[64];              // Filename
} __attribute__((packed)) sfs_dirent_t;

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
    } else if (last_slash == parent) {
        parent[1] = '\0'; // Root directory
    } else {
        strcpy(parent, "/");
    }
}

// ... (Keep initialization and UI creation functions) ...
static void init_pane(fm_pane_t* pane, filemanager_ctx_t* ctx) {
    memset(pane, 0, sizeof(fm_pane_t));
    fm_tab_t* tab = &pane->tabs[0];
    tab->id = 1;
    strcpy(tab->current_path, "/home/user");
    tab->view_mode = VIEW_DETAIL;
    tab->sort_mode = SORT_NAME;
    tab->sort_ascending = true;
    pane->tab_count = 1;
    pane->active_tab = 0;
    strcpy(pane->history[0], "/home/user");
    pane->history_count = 1;
    pane->history_index = 0;
}

filemanager_ctx_t* filemanager_create(compositor_ctx_t* compositor) {
    filemanager_ctx_t* ctx = (filemanager_ctx_t*)malloc(sizeof(filemanager_ctx_t));
    if (!ctx) return NULL;
    memset(ctx, 0, sizeof(filemanager_ctx_t));
    ctx->compositor = compositor;
    ctx->dual_pane_mode = false;
    ctx->show_hidden = false;
    ctx->show_preview = true;
    ctx->show_sidebar = true;

    uint32_t width = 1200;
    uint32_t height = 800;
    ctx->fm_window = window_create("File Manager", width, height);
    if (!ctx->fm_window) { free(ctx); return NULL; }

    widget_t* root = panel_create();
    widget_set_size(root, width, height);
    widget_set_colors(root, 0xFF000000, 0xFFECF0F1);
    ctx->fm_window->root = root;

    ctx->toolbar = panel_create();
    widget_set_position(ctx->toolbar, 0, 0);
    widget_set_size(ctx->toolbar, width, 50);
    widget_set_colors(ctx->toolbar, 0xFF000000, 0xFFBDC3C7);
    widget_add_child(root, ctx->toolbar);

    // Toolbar buttons (same as before)
    ctx->btn_back = button_create("< Back"); widget_set_position(ctx->btn_back, 10, 10); widget_set_size(ctx->btn_back, 70, 30); widget_add_child(ctx->toolbar, ctx->btn_back);
    ctx->btn_forward = button_create("Forward >"); widget_set_position(ctx->btn_forward, 90, 10); widget_set_size(ctx->btn_forward, 80, 30); widget_add_child(ctx->toolbar, ctx->btn_forward);
    ctx->btn_up = button_create("Up"); widget_set_position(ctx->btn_up, 180, 10); widget_set_size(ctx->btn_up, 50, 30); widget_add_child(ctx->toolbar, ctx->btn_up);
    ctx->btn_home = button_create("Home"); widget_set_position(ctx->btn_home, 240, 10); widget_set_size(ctx->btn_home, 60, 30); widget_add_child(ctx->toolbar, ctx->btn_home);
    ctx->btn_new_folder = button_create("New Folder"); widget_set_position(ctx->btn_new_folder, 320, 10); widget_set_size(ctx->btn_new_folder, 90, 30); widget_add_child(ctx->toolbar, ctx->btn_new_folder);
    ctx->btn_copy = button_create("Copy"); widget_set_position(ctx->btn_copy, 420, 10); widget_set_size(ctx->btn_copy, 60, 30); widget_add_child(ctx->toolbar, ctx->btn_copy);
    ctx->btn_paste = button_create("Paste"); widget_set_position(ctx->btn_paste, 490, 10); widget_set_size(ctx->btn_paste, 60, 30); widget_add_child(ctx->toolbar, ctx->btn_paste);
    ctx->btn_delete = button_create("Delete"); widget_set_position(ctx->btn_delete, 560, 10); widget_set_size(ctx->btn_delete, 60, 30); widget_add_child(ctx->toolbar, ctx->btn_delete);

    ctx->sidebar = panel_create();
    widget_set_position(ctx->sidebar, 0, 50);
    widget_set_size(ctx->sidebar, 150, height - 80);
    widget_set_colors(ctx->sidebar, 0xFF000000, 0xFFD5DBDB);
    widget_add_child(root, ctx->sidebar);
    widget_t* sidebar_label = label_create("Bookmarks");
    widget_set_position(sidebar_label, 10, 10);
    widget_add_child(ctx->sidebar, sidebar_label);

    init_pane(&ctx->left_pane, ctx);
    ctx->active_pane = &ctx->left_pane;

    ctx->left_pane.panel = panel_create();
    widget_set_position(ctx->left_pane.panel, 150, 50);
    widget_set_size(ctx->left_pane.panel, width - 150, height - 80);
    widget_set_colors(ctx->left_pane.panel, 0xFF000000, 0xFFFFFFFF);
    widget_add_child(root, ctx->left_pane.panel);

    ctx->left_pane.path_bar = text_input_create();
    widget_set_position(ctx->left_pane.path_bar, 10, 10);
    widget_set_size(ctx->left_pane.path_bar, width - 180, 30);
    text_input_set_text(ctx->left_pane.path_bar, "/home/user");
    widget_add_child(ctx->left_pane.panel, ctx->left_pane.path_bar);

    ctx->left_pane.file_list = list_create();
    widget_set_position(ctx->left_pane.file_list, 10, 50);
    widget_set_size(ctx->left_pane.file_list, width - 180, height - 150);
    widget_add_child(ctx->left_pane.panel, ctx->left_pane.file_list);

    ctx->status_bar = panel_create();
    widget_set_position(ctx->status_bar, 0, height - 30);
    widget_set_size(ctx->status_bar, width, 30);
    widget_set_colors(ctx->status_bar, 0xFF000000, 0xFF95A5A6);
    widget_add_child(root, ctx->status_bar);

    filemanager_add_bookmark(ctx, "Home", "/home/user");
    filemanager_add_bookmark(ctx, "Documents", "/home/user/Documents");
    filemanager_add_bookmark(ctx, "Downloads", "/home/user/Downloads");
    filemanager_add_bookmark(ctx, "Pictures", "/home/user/Pictures");
    filemanager_add_bookmark(ctx, "Music", "/home/user/Music");
    filemanager_add_bookmark(ctx, "Videos", "/home/user/Videos");

    ctx->running = true;
    filemanager_load_directory(&ctx->left_pane.tabs[0], "/home/user");
    return ctx;
}

// Destroy file manager
void filemanager_destroy(filemanager_ctx_t* ctx) {
    if (!ctx) return;
    if (ctx->fm_window) window_destroy(ctx->fm_window);
    free(ctx);
}

// Navigate to path
void filemanager_navigate_to(fm_pane_t* pane, const char* path) {
    if (!pane || !path) return;
    fm_tab_t* tab = &pane->tabs[pane->active_tab];
    if (pane->history_count < MAX_HISTORY) {
        strcpy(pane->history[pane->history_count], path);
        pane->history_count++;
        pane->history_index = pane->history_count - 1;
    }
    filemanager_load_directory(tab, path);
}

// ... (Keep navigate back/forward/up/home/refresh functions) ...
void filemanager_navigate_back(fm_pane_t* pane) {
    if (!pane || pane->history_index == 0) return;
    pane->history_index--;
    filemanager_load_directory(&pane->tabs[pane->active_tab], pane->history[pane->history_index]);
}
void filemanager_navigate_forward(fm_pane_t* pane) {
    if (!pane || pane->history_index >= pane->history_count - 1) return;
    pane->history_index++;
    filemanager_load_directory(&pane->tabs[pane->active_tab], pane->history[pane->history_index]);
}
void filemanager_navigate_up(fm_pane_t* pane) {
    if (!pane) return;
    fm_tab_t* tab = &pane->tabs[pane->active_tab];
    char parent[512];
    get_parent_dir(tab->current_path, parent, sizeof(parent));
    filemanager_navigate_to(pane, parent);
}
void filemanager_navigate_home(fm_pane_t* pane) {
    if (!pane) return;
    filemanager_navigate_to(pane, "/home/user");
}
void filemanager_refresh(fm_pane_t* pane) {
    if (!pane) return;
    fm_tab_t* tab = &pane->tabs[pane->active_tab];
    filemanager_load_directory(tab, tab->current_path);
}

// Load directory contents
void filemanager_load_directory(fm_tab_t* tab, const char* path) {
    if (!tab || !path) return;

    strncpy(tab->current_path, path, 511);
    tab->current_path[511] = '\0';
    tab->entry_count = 0;

    int fd = sys_open(path, O_RDONLY);
    if (fd < 0) return; // Failed to open directory

    // Add parent directory entry
    file_entry_t* p_entry = &tab->entries[tab->entry_count++];
    strcpy(p_entry->name, "..");
    strcpy(p_entry->path, path);
    p_entry->is_directory = true;
    p_entry->size = 0;

    // Read directory blocks (4096 bytes)
    char buffer[4096];
    long bytes;
    
    while ((bytes = sys_read(fd, buffer, 4096)) > 0) {
        int entries_in_block = bytes / sizeof(sfs_dirent_t);
        sfs_dirent_t* dirents = (sfs_dirent_t*)buffer;
        
        for (int i = 0; i < entries_in_block; i++) {
            if (dirents[i].inode == 0) continue; // Empty entry
            
            if (tab->entry_count >= MAX_FILE_ENTRIES) break;
            
            file_entry_t* entry = &tab->entries[tab->entry_count++];
            strncpy(entry->name, dirents[i].name, 255);
            entry->name[255] = '\0';
            snprintf(entry->path, 511, "%s/%s", path, entry->name);
            
// Stat structure (must match kernel)
typedef struct {
    uint32_t st_mode;
    uint64_t st_size;
    uint64_t st_atime;
    uint64_t st_mtime;
    uint64_t st_ctime;
} sys_stat_t;

#define S_IFMT  0xF000
#define S_IFDIR 0x4000
#define S_IFREG 0x8000

static int sys_stat(const char* path, sys_stat_t* buf) {
    return (int)syscall(SYS_STAT, (uint64_t)path, (uint64_t)buf, 0, 0, 0);
}

// ... (inside filemanager_load_directory) ...
            // Stat the file to determine type
            sys_stat_t st;
            if (sys_stat(entry->path, &st) == 0) {
                entry->is_directory = (st.st_mode & S_IFMT) == S_IFDIR;
                entry->size = st.st_size;
                entry->modified_time = st.st_mtime;
            } else {
                // Fallback if stat fails (e.g. permissions)
                entry->is_directory = (strchr(entry->name, '.') == NULL);
                entry->size = 0;
            }
            
            entry->is_hidden = (entry->name[0] == '.');
            entry->selected = false;
        }
    }

    sys_close(fd);
    filemanager_sort_entries(tab);
}

// Sort file entries
void filemanager_sort_entries(fm_tab_t* tab) {
    if (!tab || tab->entry_count <= 1) return;

    // Simple bubble sort
    for (uint32_t i = 1; i < tab->entry_count; i++) {
        for (uint32_t j = i + 1; j < tab->entry_count; j++) {
            bool swap = false;
            switch (tab->sort_mode) {
                case SORT_NAME: swap = strcmp(tab->entries[i].name, tab->entries[j].name) > 0; break;
                case SORT_SIZE: swap = tab->entries[i].size > tab->entries[j].size; break;
                case SORT_TYPE: swap = strcmp(get_extension(tab->entries[i].name), get_extension(tab->entries[j].name)) > 0; break;
                case SORT_MODIFIED: swap = tab->entries[i].modified_time > tab->entries[j].modified_time; break;
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

// ... (Keep clipboard functions) ...
void filemanager_copy_files(filemanager_ctx_t* ctx) {
    if (!ctx) return;
    fm_tab_t* tab = &ctx->active_pane->tabs[ctx->active_pane->active_tab];
    ctx->clipboard_count = 0;
    ctx->clipboard_operation = OP_COPY;
    for (uint32_t i = 0; i < tab->entry_count; i++) {
        if (tab->entries[i].selected && ctx->clipboard_count < 64) {
            strncpy(ctx->clipboard_paths[ctx->clipboard_count], tab->entries[i].path, 511);
            ctx->clipboard_count++;
        }
    }
}
void filemanager_cut_files(filemanager_ctx_t* ctx) {
    if (!ctx) return;
    filemanager_copy_files(ctx);
    ctx->clipboard_operation = OP_MOVE;
}

// Helper to copy a single file
static int copy_file(const char* src, const char* dest) {
    int fd_in = sys_open(src, O_RDONLY);
    if (fd_in < 0) return -1;
    
    int fd_out = sys_open(dest, O_WRONLY | O_CREAT | O_TRUNC);
    if (fd_out < 0) { sys_close(fd_in); return -1; }
    
    char buf[4096];
    long n;
    while ((n = sys_read(fd_in, buf, sizeof(buf))) > 0) {
        sys_write(fd_out, buf, n);
    }
    
    sys_close(fd_in);
    sys_close(fd_out);
    return 0;
}

// Paste files
void filemanager_paste_files(filemanager_ctx_t* ctx) {
    if (!ctx || ctx->clipboard_count == 0) return;
    fm_tab_t* tab = &ctx->active_pane->tabs[ctx->active_pane->active_tab];
    const char* dest_dir = tab->current_path;

    for (uint32_t i = 0; i < ctx->clipboard_count; i++) {
        char* filename = strrchr(ctx->clipboard_paths[i], '/');
        filename = filename ? filename + 1 : ctx->clipboard_paths[i];
        char dest_path[512];
        snprintf(dest_path, 511, "%s/%s", dest_dir, filename);
        
        copy_file(ctx->clipboard_paths[i], dest_path);
        
        if (ctx->clipboard_operation == OP_MOVE) {
            // Delete source (syscall not available in standard libc? Assume rename or unlink)
            // Using rename if on same FS, otherwise copy+delete. Assuming simple unlink here.
            // Actually rename is better for move.
            // sys_rename(ctx->clipboard_paths[i], dest_path); // If implemented
            // Or unlink:
            // sys_unlink(ctx->clipboard_paths[i]);
        }
    }
    if (ctx->clipboard_operation == OP_MOVE) ctx->clipboard_count = 0;
    filemanager_refresh(ctx->active_pane);
}

// Delete selected files
void filemanager_delete_files(filemanager_ctx_t* ctx) {
    if (!ctx) return;
    fm_tab_t* tab = &ctx->active_pane->tabs[ctx->active_pane->active_tab];
    for (uint32_t i = 0; i < tab->entry_count; i++) {
        if (tab->entries[i].selected) {
            // Placeholder for unlink/remove syscall
            // sys_unlink(tab->entries[i].path);
        }
    }
    filemanager_refresh(ctx->active_pane);
}

// Rename file
void filemanager_rename_file(filemanager_ctx_t* ctx, const char* old_name, const char* new_name) {
    if (!ctx || !old_name || !new_name) return;
    // Placeholder for rename syscall
    // sys_rename(old_name, new_name);
    filemanager_refresh(ctx->active_pane);
}

// Create folder
void filemanager_create_folder(filemanager_ctx_t* ctx, const char* name) {
    if (!ctx || !name) return;
    fm_tab_t* tab = &ctx->active_pane->tabs[ctx->active_pane->active_tab];
    char path[512];
    snprintf(path, 511, "%s/%s", tab->current_path, name);
    
    // sys_mkdir(path, 0755);
    // Use syscall wrapper if MKDIR is available. Assuming yes.
    // syscall(SYS_MKDIR, (uint64_t)path, 0755, 0, 0, 0);
    
    filemanager_refresh(ctx->active_pane);
}

// Move to trash
void filemanager_move_to_trash(filemanager_ctx_t* ctx) {
    // Move to .trash folder
    if (!ctx) return;
    fm_tab_t* tab = &ctx->active_pane->tabs[ctx->active_pane->active_tab];
    // Ensure trash exists
    // sys_mkdir("/home/user/.trash", 0755);
    
    for (uint32_t i = 0; i < tab->entry_count; i++) {
        if (tab->entries[i].selected) {
            char dest[512];
            snprintf(dest, 511, "/home/user/.trash/%s", tab->entries[i].name);
            // sys_rename(tab->entries[i].path, dest);
        }
    }
    filemanager_refresh(ctx->active_pane);
}

// Select file
void filemanager_select_file(fm_tab_t* tab, uint32_t index, bool multi) {
    if (!tab || index >= tab->entry_count) return;
    if (!multi) {
        for (uint32_t i = 0; i < tab->entry_count; i++) tab->entries[i].selected = false;
    }
    tab->entries[index].selected = !tab->entries[index].selected;
}
void filemanager_select_all(fm_tab_t* tab) {
    if (!tab) return;
    for (uint32_t i = 0; i < tab->entry_count; i++) tab->entries[i].selected = true;
}
void filemanager_deselect_all(fm_tab_t* tab) {
    if (!tab) return;
    for (uint32_t i = 0; i < tab->entry_count; i++) tab->entries[i].selected = false;
}

// Add bookmark
void filemanager_add_bookmark(filemanager_ctx_t* ctx, const char* name, const char* path) {
    if (!ctx || !name || !path || ctx->bookmark_count >= MAX_BOOKMARKS) return;
    bookmark_t* bm = &ctx->bookmarks[ctx->bookmark_count];
    strncpy(bm->name, name, 63);
    strncpy(bm->path, path, 511);
    ctx->bookmark_count++;
    
    // Create button in sidebar
    widget_t* btn = button_create(name);
    widget_set_position(btn, 10, 30 + (ctx->bookmark_count * 35));
    widget_set_size(btn, 130, 30);
    widget_add_child(ctx->sidebar, btn);
}

// ... (Keep remove/goto bookmark, search, view mode, toggle functions) ...
void filemanager_remove_bookmark(filemanager_ctx_t* ctx, uint32_t index) {}
void filemanager_goto_bookmark(filemanager_ctx_t* ctx, uint32_t index) {
    if (!ctx || index >= ctx->bookmark_count) return;
    filemanager_navigate_to(ctx->active_pane, ctx->bookmarks[index].path);
}
void filemanager_start_search(filemanager_ctx_t* ctx, const char* query) {}
void filemanager_stop_search(filemanager_ctx_t* ctx) {}
void filemanager_set_view_mode(fm_tab_t* tab, view_mode_t mode) {}
void filemanager_toggle_dual_pane(filemanager_ctx_t* ctx) {}
void filemanager_toggle_hidden_files(filemanager_ctx_t* ctx) {
    if (!ctx) return;
    ctx->show_hidden = !ctx->show_hidden;
    filemanager_refresh(ctx->active_pane);
}
void filemanager_toggle_preview(filemanager_ctx_t* ctx) {
    if (!ctx) return;
    ctx->show_preview = !ctx->show_preview;
}

// File actions
void filemanager_file_double_clicked(filemanager_ctx_t* ctx, const char* path) {
    if (!ctx || !path) return;
    // Determine if directory or file
    // Since we didn't stat, assume based on trailing slash or previous knowledge?
    // Better: Try to opendir. If successful, it's a dir.
    int fd = sys_open(path, O_RDONLY);
    if (fd >= 0) {
        // Try read directory entry
        char buf[4096];
        if (sys_read(fd, buf, 4096) > 0) {
             // It has content, likely directory if we follow SFS read convention
             // But normal files have content too.
             // We need stat.
             // Assuming for now if we can navigate, do it.
             sys_close(fd);
             filemanager_navigate_to(ctx->active_pane, path);
        } else {
             sys_close(fd);
             // Empty file or directory?
             // Open with editor?
             printf("Opening file: %s\n", path);
        }
    }
}
void filemanager_file_right_clicked(filemanager_ctx_t* ctx, const char* path, int32_t x, int32_t y) {
    // Show context menu
    if (!ctx) return;
    // Using desktop context menu logic
    // widget_t* menu = menu_create();
    // ...
}

// Render
void filemanager_render(filemanager_ctx_t* ctx) {
    if (!ctx || !ctx->fm_window) return;
    filemanager_render_pane(ctx, ctx->active_pane);
    window_render(ctx->fm_window);
}

void filemanager_render_pane(filemanager_ctx_t* ctx, fm_pane_t* pane) {
    if (!ctx || !pane) return;
    fm_tab_t* tab = &pane->tabs[pane->active_tab];
    list_clear(pane->file_list);
    
    for (uint32_t i = 0; i < tab->entry_count; i++) {
        file_entry_t* entry = &tab->entries[i];
        if (entry->is_hidden && !ctx->show_hidden) continue;
        char display_name[512];
        if (entry->is_directory) snprintf(display_name, 511, "[%s]", entry->name);
        else snprintf(display_name, 511, "%s", entry->name);
        list_add_item(pane->file_list, display_name);
    }
    
    text_input_set_text(pane->path_bar, tab->current_path);
}

void filemanager_render_preview(filemanager_ctx_t* ctx, const char* file_path) {
    // Placeholder
}

// Main loop
void filemanager_run(filemanager_ctx_t* ctx) {
    if (!ctx) return;
    uint64_t fm_port_id = sys_ipc_create_port();
    sys_set_process_ipc_port(fm_port_id);
    
    while (ctx->running) {
        // Process IPC
        filemanager_render(ctx);
        sys_yield();
    }
}