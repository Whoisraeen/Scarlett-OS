/**
 * @file shell.h
 * @brief Shell interface
 * 
 * Basic shell for userspace interaction.
 */

#ifndef KERNEL_SHELL_H
#define KERNEL_SHELL_H

#include "../types.h"

// Shell command structure
typedef struct shell_command {
    const char* name;
    const char* description;
    int (*handler)(int argc, const char** argv);
} shell_command_t;

// Shell functions
void shell_init(void);
void shell_run(void);
void shell_prompt(void);
int shell_execute_command(const char* line);
int shell_parse_command(const char* line, char** argv, int max_args);

// Built-in command handlers
int cmd_help(int argc, const char** argv);
int cmd_echo(int argc, const char** argv);
int cmd_exit(int argc, const char** argv);
int cmd_ls(int argc, const char** argv);
int cmd_cd(int argc, const char** argv);
int cmd_pwd(int argc, const char** argv);
int cmd_cat(int argc, const char** argv);
int cmd_clear(int argc, const char** argv);

#endif // KERNEL_SHELL_H

