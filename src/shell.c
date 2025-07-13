#include "fat_fs.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#define CMD_BUFFER_SIZE 4096 // when using append, we need a larger buffer

int main() {
    char cmd_line[CMD_BUFFER_SIZE];
    bool fs_loaded = false;

    // Try to open the partition file, but don't fail if it doesn't exist yet
    init_fs();

    printf("FAT16 File System Simulator. Type 'exit' to quit.\n");

    while (1) {
        printf("> ");
        if (fgets(cmd_line, sizeof(cmd_line), stdin) == NULL) break;

        // --- ROBUST FIX STARTS HERE ---
        // If the line does not contain a newline, it was too long
        if (strchr(cmd_line, '\n') == NULL) {
            int c;
            while ((c = getchar()) != '\n' && c != EOF); // Clear remaining input
            fprintf(stderr, "Error: Command was too long and has been ignored.\n");
            continue;
        }
        // --- ROBUST FIX ENDS HERE ---

        // Remove newline character
        cmd_line[strcspn(cmd_line, "\n")] = 0;

        // --- Command Parsing ---
        char cmd_copy[CMD_BUFFER_SIZE];
        strcpy(cmd_copy, cmd_line);
        char* command = strtok(cmd_copy, " ");
        if (command == NULL) continue;
        // --- End Command Parsing ---

        if (strcmp(command, "exit") == 0) break;

        if (strcmp(command, "init") == 0) {
            if (fs_format() == 0) {
                printf("File system formatted. Run 'load' to use it.\n");
                fs_loaded = false;
            } else {
                fprintf(stderr, "Failed to format file system.\n");
            }
        }
        else if (strcmp(command, "load") == 0) {
            if (fs_load_fat() == 0) {
                fs_loaded = true;
                printf("File system loaded and ready.\n");
            } else {
                fprintf(stderr, "Failed to load FAT. Did you run 'init' first?\n");
            }
        }

        else if (fs_loaded) { // --- Commands requiring a loaded FS ---
            if (strcmp(command, "ls") == 0) {
                char* arg1 = strtok(NULL, " ");
                fs_ls((arg1 != NULL) ? arg1 : "/");
            }
            else if (strcmp(command, "mkdir") == 0) {
                char* arg1 = strtok(NULL, " ");
                if (arg1) fs_mkdir(arg1);
                else fprintf(stderr, "mkdir: missing operand\n");
            }
            else if (strcmp(command, "create") == 0) {
                char* arg1 = strtok(NULL, " ");
                if (arg1) fs_create(arg1);
                else fprintf(stderr, "create: missing operand\n");
            }
            else if (strcmp(command, "unlink") == 0) {
                char* arg1 = strtok(NULL, " ");
                if (arg1) fs_unlink(arg1);
                else fprintf(stderr, "unlink: missing operand\n");
            }
            else if (strcmp(command, "read") == 0) {
                char* arg1 = strtok(NULL, " ");
                if (arg1) fs_read(arg1);
                else fprintf(stderr, "read: missing operand\n");
            }
            else if (strcmp(command, "write") == 0) {
                char* arg_str = strtok(NULL, "\"");
                char* arg_path = strtok(NULL, " ");
                if (arg_str && arg_path) {
                    fs_write(arg_path, arg_str);
                } else {
                    fprintf(stderr, "Usage: write \"content\" /path/to/file\n");
                }
            }
            else if (strcmp(command, "append") == 0) {
                char* arg_str = strtok(NULL, "\"");
                char* arg_path = strtok(NULL, " ");
                if (arg_str && arg_path) {
                    fs_append(arg_path, arg_str);
                } else {
                    fprintf(stderr, "Usage: append \"content\" /path/to/file\n");
                }
            }
            else {
                printf("Command '%s' not implemented or invalid.\n", command);
            }
        }

        else {
            printf("File system not loaded. Please run 'init' and 'load'.\n");
        }
    }

    printf("Shutting down simulator.\n");
    close_fs();

    return 0;
}