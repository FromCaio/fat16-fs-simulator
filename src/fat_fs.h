#ifndef FAT_FS_H
#define FAT_FS_H

#include <stdint.h> // For types like uint8_t, uint16_t, uint32_t
#include <stdio.h>  // For FILE*
#include <stdlib.h> // For malloc, free, exit
#include <stdbool.h>

// --- File System Constants ---
#define PARTITION_NAME "fat.part"

#define SECTOR_SIZE 512
#define CLUSTER_SIZE 1024
#define CLUSTER_COUNT 4096
#define PARTITION_SIZE (CLUSTER_SIZE * CLUSTER_COUNT) // 4MB

// Partition layout (in clusters)
#define BOOT_BLOCK_CLUSTER 0
#define FAT_CLUSTER_START 1
#define FAT_CLUSTER_COUNT 8
#define ROOT_DIR_CLUSTER 9
#define DATA_CLUSTER_START 10

// --- FAT Constants ---
#define FAT_ENTRY_FREE 0x0000
#define FAT_ENTRY_BOOT 0xFFFD
#define FAT_ENTRY_RESERVED 0xFFFE
#define FAT_ENTRY_EOF 0xFFFF // End of File

// --- Directory Constants ---
#define DIR_ENTRY_SIZE 32
#define DIR_ENTRIES_PER_CLUSTER (CLUSTER_SIZE / DIR_ENTRY_SIZE)
#define ATTR_ARCHIVE 0
#define ATTR_DIRECTORY 1

// --- Global FAT Table ---
// The in-memory copy of the File Allocation Table.
// 'extern' means it's defined in a .c file.
extern uint16_t g_fat_table[CLUSTER_COUNT];

// --- Data Structures ---

// Directory entry (32 bytes)
typedef struct {
    uint8_t filename[18];    // File or directory name
    uint8_t attributes;      // 0 = file, 1 = directory
    uint8_t reserved[7];     // Reserved for future use
    uint16_t first_block;    // First cluster used by the file/directory
    uint32_t size;           // File size in bytes
} dir_entry_t;

// Cluster used for data or directories (1024 bytes)
union data_cluster {
    dir_entry_t dir[DIR_ENTRIES_PER_CLUSTER]; // As directory
    uint8_t data[CLUSTER_SIZE];               // As raw data
};

// --- Helper Structures ---

// Holds the result of a search operation for a file/directory.
typedef struct {
    char name[18];             // The last component of the path searched for
    bool found;                // True if the entry was found
    uint16_t parent_cluster;   // Cluster number of the parent directory
    uint16_t entry_cluster;    // The first cluster of the found entry itself
    uint32_t entry_index;      // The index (0-31) of the entry within the parent directory
    dir_entry_t entry;         // A copy of the directory entry
} path_search_result_t;

// --- Prototypes for High-Level FS Operations ---
/**
 * @brief Deletes a file or an empty directory.
 * @param path The absolute path to the file or directory to delete.
 * @return 0 on success, -1 on error.
 */
int fs_unlink(const char* path);

/**
 * @brief Reads the content of a file and prints it to the console.
 * @param path The absolute path of the file to read.
 * @return 0 on success, -1 on error.
 */
int fs_read(const char* path);

/**
 * @brief Writes a string to a file, overwriting any existing content.
 * @param path The absolute path of the file to write to.
 * @param content The string content to write.
 * @return 0 on success, -1 on error.
 */
int fs_write(const char* path, const char* content);

/**
 * @brief Appends a string to the end of a file.
 * @param path The absolute path of the file to append to.
 * @param content The string content to append.
 * @return 0 on success, -1 on error.
 */
int fs_append(const char* path, const char* content);
/**
 * @brief Creates a new directory.
 * @param path The absolute path of the new directory to create.
 * @return 0 on success, -1 on error.
 */
int fs_mkdir(const char* path);

/**
 * @brief Creates a new, empty file.
 * @param path The absolute path of the new file to create.
 * @return 0 on success, -1 on error.
 */
int fs_create(const char* path);

/**
 * @brief Finds a file or directory by its absolute path.
 *
 * @param path The absolute path to the entry (e.g., "/dir1/file.txt").
 * @param result A pointer to a struct that will be filled with search results.
 * @return 0 on success (even if not found), -1 on critical error.
 */
int find_entry_by_path(const char* path, path_search_result_t* result);

/**
 * @brief Lists the contents of a directory.
 * @param path The absolute path to the directory.
 * @return 0 on success, -1 on error (e.g., path not found or not a directory).
 */
int fs_ls(const char* path);

/**
 * @brief Formats the virtual disk. Creates fat.part, writes the boot block,
 * initializes and writes the FAT, and creates an empty root directory.
 * @return 0 on success, -1 on error.
 */
int fs_format();

/**
 * @brief Loads the FAT from the virtual disk into the g_fat_table array.
 * @return 0 on success, -1 on error.
 */
int fs_load_fat();

// --- Low-Level Function Prototypes (Phase 1) ---

/**
 * @brief Opens the virtual partition file and keeps it globally accessible.
 * @return 0 on success, -1 on failure.
 */
int init_fs();

/**
 * @brief Closes the virtual partition file.
 */
void close_fs();

/**
 * @brief Reads a cluster from the virtual disk.
 * @param cluster_index Index of the cluster to read.
 * @param buffer Preallocated buffer (size = CLUSTER_SIZE) to store the data.
 * @return 0 on success, -1 on failure.
 */
int read_cluster(uint16_t cluster_index, void* buffer);

/**
 * @brief Writes the contents of a buffer to a cluster on the virtual disk.
 * @param cluster_index Index of the cluster to write.
 * @param buffer Buffer (size = CLUSTER_SIZE) containing the data to write.
 * @return 0 on success, -1 on failure.
 */
int write_cluster(uint16_t cluster_index, const void* buffer);

#endif // FAT_FS_H