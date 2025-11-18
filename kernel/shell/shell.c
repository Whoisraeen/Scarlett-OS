/**
 * @file shell.c
 * @brief Basic shell implementation
 * 
 * Interactive shell for the operating system.
 * Provides command parsing and execution.
 */

#include "../include/types.h"
#include "../include/shell.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/process.h"
#include "../include/elf.h"
#include <stddef.h>

// Shell state
static bool shell_running = false;
static char current_directory[256] = "/";
static char command_buffer[512];
static char* argv_buffer[32];  // Max 32 arguments

// Built-in commands
static const shell_command_t builtin_commands[] = {
    {"help", "Show available commands", cmd_help},
    {"echo", "Print text", cmd_echo},
    {"exit", "Exit the shell", cmd_exit},
    {"ls", "List directory contents", cmd_ls},
    {"cd", "Change directory", cmd_cd},
    {"pwd", "Print working directory", cmd_pwd},
    {"cat", "Display file contents", cmd_cat},
    {"clear", "Clear the screen", cmd_clear},
    {NULL, NULL, NULL}  // Terminator
};

/**
 * Initialize shell
 */
void shell_init(void) {
    kinfo("Initializing shell...\n");
    
    shell_running = true;
    current_directory[0] = '/';
    current_directory[1] = '\0';
    
    kinfo("Shell initialized\n");
}

/**
 * Print shell prompt
 */
void shell_prompt(void) {
    kprintf("\n");
    kprintf("scarlett@os:%s$ ", current_directory);
}

/**
 * Parse command line into arguments
 */
int shell_parse_command(const char* line, char** argv, int max_args) {
    if (!line || !argv || max_args < 1) {
        return 0;
    }
    
    // Copy line to a modifiable buffer
    static char line_buffer[512];
    strncpy(line_buffer, line, sizeof(line_buffer) - 1);
    line_buffer[sizeof(line_buffer) - 1] = '\0';
    
    int argc = 0;
    char* p = line_buffer;
    bool in_quotes = false;
    bool in_token = false;
    
    // Skip leading whitespace
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    
    while (*p != '\0' && argc < max_args - 1) {
        if (*p == '"') {
            in_quotes = !in_quotes;
            if (!in_quotes && in_token) {
                // End of quoted token
                *p = '\0';
                in_token = false;
            } else if (in_quotes && !in_token) {
                // Start of quoted token
                argv[argc++] = (char*)p + 1;
                in_token = true;
            }
        } else if (*p == ' ' || *p == '\t') {
            if (!in_quotes) {
                if (in_token) {
                    // End of token
                    *p = '\0';
                    in_token = false;
                }
            }
        } else {
            if (!in_token) {
                // Start of new token
                argv[argc++] = (char*)p;
                in_token = true;
            }
        }
        p++;
    }
    
    // Null-terminate argument list
    argv[argc] = NULL;
    
    return argc;
}

/**
 * Simple string comparison
 */
static int shell_strcmp(const char* a, const char* b) {
    if (!a || !b) {
        return (a == b) ? 0 : 1;
    }
    while (*a && *b && *a == *b) {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

/**
 * Find built-in command
 */
static const shell_command_t* find_builtin(const char* name) {
    for (int i = 0; builtin_commands[i].name != NULL; i++) {
        if (builtin_commands[i].name && shell_strcmp(builtin_commands[i].name, name) == 0) {
            return &builtin_commands[i];
        }
    }
    return NULL;
}

/**
 * Execute a command
 */
int shell_execute_command(const char* line) {
    if (!line) {
        return -1;
    }
    
    // Copy line to buffer (we'll modify it during parsing)
    size_t len = 0;
    while (line[len] && len < sizeof(command_buffer) - 1) {
        command_buffer[len] = line[len];
        len++;
    }
    command_buffer[len] = '\0';
    
    // Skip empty lines
    if (len == 0) {
        return 0;
    }
    
    // Remove trailing newline
    if (command_buffer[len - 1] == '\n') {
        command_buffer[len - 1] = '\0';
    }
    
    // Parse command
    int argc = shell_parse_command(command_buffer, argv_buffer, 32);
    if (argc == 0) {
        return 0;
    }
    
    const char* cmd_name = argv_buffer[0];
    if (!cmd_name) {
        return -1;
    }
    
    // Check for built-in command
    const shell_command_t* cmd = find_builtin(cmd_name);
    if (cmd) {
        return cmd->handler(argc, (const char**)argv_buffer);
    }
    
    // Not a built-in command - try to execute as program
    // TODO: Implement program execution via ELF loader
    kprintf("Command not found: %s\n", cmd_name);
    kprintf("Type 'help' for available commands.\n");
    
    return -1;
}

/**
 * Read a line from serial input
 */
static int shell_read_line(char* buffer, size_t size) {
    extern char serial_getc(void);
    
    if (!buffer || size == 0) {
        return -1;
    }
    
    size_t pos = 0;
    
    while (pos < size - 1) {
        char c = serial_getc();
        
        // Handle backspace
        if (c == 0x08 || c == 0x7F) {  // Backspace or DEL
            if (pos > 0) {
                pos--;
                buffer[pos] = '\0';
                kprintf("\b \b");  // Erase character on screen
            }
            continue;
        }
        
        // Handle Enter/Return
        if (c == '\r' || c == '\n') {
            buffer[pos] = '\0';
            kprintf("\n");
            return pos;
        }
        
        // Handle printable characters
        if (c >= 0x20 && c < 0x7F) {
            buffer[pos++] = c;
            kprintf("%c", c);  // Echo character
        }
    }
    
    buffer[pos] = '\0';
    return pos;
}

/**
 * Run shell main loop
 */
void shell_run(void) {
    kinfo("Starting shell...\n");
    kprintf("\n");
    kprintf("====================================================\n");
    kprintf("           Scarlett OS Shell v0.1.0                \n");
    kprintf("====================================================\n");
    kprintf("Type 'help' for available commands.\n");
    kprintf("Type 'exit' to exit the shell.\n");
    kprintf("====================================================\n");
    
    shell_running = true;
    char input_buffer[512];
    
    while (shell_running) {
        shell_prompt();
        
        // Read command from serial
        int len = shell_read_line(input_buffer, sizeof(input_buffer));
        if (len < 0) {
            continue;
        }
        
        // Execute command
        if (len > 0) {
            shell_execute_command(input_buffer);
        }
    }
    
    kinfo("Shell exited\n");
}

/**
 * Help command
 */
int cmd_help(int argc, const char** argv) {
    (void)argc;
    (void)argv;
    
    kprintf("\nAvailable commands:\n");
    kprintf("  help  - Show this help message\n");
    kprintf("  echo  - Print text to console\n");
    kprintf("  exit  - Exit the shell\n");
    kprintf("  ls    - List directory contents (not yet implemented)\n");
    kprintf("  cd    - Change directory (not yet implemented)\n");
    kprintf("  pwd   - Print working directory\n");
    kprintf("  cat   - Display file contents (not yet implemented)\n");
    kprintf("  clear - Clear the screen\n");
    kprintf("\n");
    
    return 0;
}

/**
 * Echo command
 */
int cmd_echo(int argc, const char** argv) {
    for (int i = 1; i < argc; i++) {
        if (i > 1) {
            kprintf(" ");
        }
        kprintf("%s", argv[i]);
    }
    kprintf("\n");
    
    return 0;
}

/**
 * Exit command
 */
int cmd_exit(int argc, const char** argv) {
    (void)argc;
    (void)argv;
    
    kprintf("Exiting shell...\n");
    shell_running = false;
    
    return 0;
}

/**
 * List directory command
 */
int cmd_ls(int argc, const char** argv) {
    (void)argc;
    (void)argv;
    
    kprintf("ls: File system not yet implemented\n");
    kprintf("Current directory: %s\n", current_directory);
    
    return 0;
}

/**
 * Change directory command
 */
int cmd_cd(int argc, const char** argv) {
    if (argc < 2) {
        kprintf("cd: Missing argument\n");
        kprintf("Usage: cd <directory>\n");
        return -1;
    }
    
    kprintf("cd: File system not yet implemented\n");
    kprintf("Would change to: %s\n", argv[1]);
    
    return 0;
}

/**
 * Print working directory command
 */
int cmd_pwd(int argc, const char** argv) {
    (void)argc;
    (void)argv;
    
    kprintf("%s\n", current_directory);
    
    return 0;
}

/**
 * Cat command
 */
int cmd_cat(int argc, const char** argv) {
    if (argc < 2) {
        kprintf("cat: Missing argument\n");
        kprintf("Usage: cat <file>\n");
        return -1;
    }
    
    kprintf("cat: File system not yet implemented\n");
    kprintf("Would display: %s\n", argv[1]);
    
    return 0;
}

/**
 * Clear command
 */
int cmd_clear(int argc, const char** argv) {
    (void)argc;
    (void)argv;
    
    // Clear screen by printing newlines
    for (int i = 0; i < 50; i++) {
        kprintf("\n");
    }
    
    return 0;
}

