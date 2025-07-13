#include "fat_fs.h"
#include <string.h> // For strerror
#include <errno.h>  // For errno

// --- Global Variables ---
// Definition of the in-memory FAT.
uint16_t g_fat_table[CLUSTER_COUNT];

// Static global pointer to the partition file.
// 'static' makes it visible only within this file (fat_fs.c).
static FILE* g_partition_file = NULL;

int init_fs() {
    // Opens the file in "r+b" mode (read and write in binary mode; file must exist).
    // The 'init' command will use a different mode to create the file if needed.
    g_partition_file = fopen(PARTITION_NAME, "r+b");

    if (g_partition_file == NULL) {
        // If it doesn't exist, it's not a fatal error yet,
        // because the 'init' command will create it.
        // Print a warning that may help during debugging.
        printf("Warning: Could not open '%s'. The file will be created with the 'init' command.\n", PARTITION_NAME);
        return 0; // Return success for now
    }
    return 0;
}

void close_fs() {
    if (g_partition_file != NULL) {
        fclose(g_partition_file);
        g_partition_file = NULL;
    }
}

int read_cluster(uint16_t cluster_index, void* buffer) {
    if (g_partition_file == NULL) {
        fprintf(stderr, "Error: File system not initialized. Cannot read.\n");
        return -1;
    }

    if (cluster_index >= CLUSTER_COUNT) {
        fprintf(stderr, "Error: Attempt to read invalid cluster (%u).\n", cluster_index);
        return -1;
    }

    long offset = (long)cluster_index * CLUSTER_SIZE;

    if (fseek(g_partition_file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "Error seeking cluster %u: %s\n", cluster_index, strerror(errno));
        return -1;
    }

    size_t bytes_read = fread(buffer, 1, CLUSTER_SIZE, g_partition_file);

    if (bytes_read != CLUSTER_SIZE) {
        // fread may return fewer bytes than expected if feof or ferror is set
        fprintf(stderr, "Error reading cluster %u. Bytes read: %zu of %d\n", cluster_index, bytes_read, CLUSTER_SIZE);
        return -1;
    }

    return 0; // Success
}

int write_cluster(uint16_t cluster_index, const void* buffer) {
    if (g_partition_file == NULL) {
        // Special case for the 'init' command, which may need to create the file
        g_partition_file = fopen(PARTITION_NAME, "w+b");
        if (g_partition_file == NULL) {
            fprintf(stderr, "Critical error: Failed to create or open partition file '%s'.\n", PARTITION_NAME);
            return -1;
        }
    }

    if (cluster_index >= CLUSTER_COUNT) {
        fprintf(stderr, "Error: Attempt to write to invalid cluster (%u).\n", cluster_index);
        return -1;
    }

    long offset = (long)cluster_index * CLUSTER_SIZE;

    if (fseek(g_partition_file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "Error seeking cluster %u for writing: %s\n", cluster_index, strerror(errno));
        return -1;
    }

    size_t bytes_written = fwrite(buffer, 1, CLUSTER_SIZE, g_partition_file);
    
    if (bytes_written != CLUSTER_SIZE) {
        fprintf(stderr, "Error writing to cluster %u. Bytes written: %zu of %d\n", cluster_index, bytes_written, CLUSTER_SIZE);
        return -1;
    }

    // Ensure data is flushed to disk immediately.
    // Important for file system consistency.
    fflush(g_partition_file);

    return 0; // Success
}

int fs_format() {
    // We need to create the file, so we open it in "w+b" mode.
    // This creates the file if it doesn't exist, or truncates it if it does.
    if (g_partition_file != NULL) {
        fclose(g_partition_file);
    }
    g_partition_file = fopen(PARTITION_NAME, "w+b");
    if (g_partition_file == NULL) {
        perror("Error creating or truncating partition file");
        return -1;
    }

    // 1. Prepare an in-memory FAT
    printf("Formatting file system...\n");
    //memset is used to fill the FAT table with specific values.
    // FAT_ENTRY_FREE is 0x0000, meaning the cluster is free.
    // FAT_ENTRY_BOOT is 0xFFF8, meaning it's the Boot Block.
    // FAT_ENTRY_RESERVED is 0xFFF0, meaning it's reserved for the FAT itself
    // FAT_ENTRY_EOF is 0xFFFF, meaning it's the end of a file chain.
    memset(g_fat_table, FAT_ENTRY_FREE, sizeof(g_fat_table)); // Fill with 0x0000

    g_fat_table[BOOT_BLOCK_CLUSTER] = FAT_ENTRY_BOOT;         // 0 is the Boot Block
    for (uint16_t i = FAT_CLUSTER_START; i < (FAT_CLUSTER_START + FAT_CLUSTER_COUNT); ++i) {
        g_fat_table[i] = FAT_ENTRY_RESERVED;                  // 1-8 are reserved for the FAT itself
    }
    g_fat_table[ROOT_DIR_CLUSTER] = FAT_ENTRY_EOF;            // 9 is the Root Directory (and it's the end of its chain)

    // 2. Prepare an empty Boot Block buffer
    uint8_t boot_block_buffer[CLUSTER_SIZE];
    memset(boot_block_buffer, 0xBB, sizeof(boot_block_buffer));

    // 3. Prepare an empty Root Directory buffer
    union data_cluster root_dir_buffer;
    memset(&root_dir_buffer, 0x00, sizeof(root_dir_buffer));

    // 4. Write everything to the virtual disk file
    printf("Writing Boot Block...\n");
    if (write_cluster(BOOT_BLOCK_CLUSTER, boot_block_buffer) != 0) {
        fprintf(stderr, "Error writing boot block.\n");
        return -1;
    }

    printf("Writing File Allocation Table (FAT)...\n");
    // The FAT is 8 clusters long. We write it from our in-memory g_fat_table.
    // We cast our uint16_t* FAT table to a uint8_t* to treat it as raw bytes.
    if (write_cluster(FAT_CLUSTER_START, (uint8_t*)g_fat_table) != 0) {
        // Note: This simplified version writes the whole 8KB FAT starting at cluster 1.
        // The spec implies the FAT occupies 8 clusters, so we perform a single large write
        // that spans them. write_cluster is writing CLUSTER_SIZE bytes, so we'd need a loop.
        // Let's correct this to be precise.
    }
    
    // CORRECTED FAT WRITING LOGIC:
    // The FAT itself spans 8 clusters. We must write it cluster by cluster.
    uint8_t* fat_as_bytes = (uint8_t*)g_fat_table;
    for (uint16_t i = 0; i < FAT_CLUSTER_COUNT; ++i) {
        if (write_cluster(FAT_CLUSTER_START + i, fat_as_bytes + (i * CLUSTER_SIZE)) != 0) {
            fprintf(stderr, "Error writing FAT cluster #%u\n", FAT_CLUSTER_START + i);
            return -1;
        }
    }

    printf("Writing Root Directory...\n");
    if (write_cluster(ROOT_DIR_CLUSTER, &root_dir_buffer) != 0) {
        fprintf(stderr, "Error writing root directory.\n");
        return -1;
    }

    // We don't need to write the data area, as it's implicitly empty.
    
    // Finally, ensure the file is the correct size (4MB)
    // fseek to the last byte and write a null character.
    if (fseek(g_partition_file, PARTITION_SIZE - 1, SEEK_SET) != 0) {
        perror("Error seeking to end of file");
        return -1;
    }
    if (fwrite("\0", 1, 1, g_partition_file) != 1) {
        perror("Error writing last byte of file");
        return -1;
    }

    printf("Format complete. '%s' created with size %d bytes.\n", PARTITION_NAME, PARTITION_SIZE);
    fflush(g_partition_file);

    return 0;
}


int fs_load_fat() {
    printf("Loading FAT from disk...\n");
    
    // The FAT spans 8 clusters. We must read it cluster by cluster.
    uint8_t* fat_as_bytes = (uint8_t*)g_fat_table;
    for (uint16_t i = 0; i < FAT_CLUSTER_COUNT; ++i) {
        if (read_cluster(FAT_CLUSTER_START + i, fat_as_bytes + (i * CLUSTER_SIZE)) != 0) {
            fprintf(stderr, "Error loading FAT cluster #%u\n", FAT_CLUSTER_START + i);
            return -1;
        }
    }
    
    printf("FAT loaded successfully.\n");
    return 0;
}

int find_entry_by_path(const char* path, path_search_result_t* result) {
    memset(result, 0, sizeof(path_search_result_t));
    result->parent_cluster = ROOT_DIR_CLUSTER; // Start search at the root

    // strtok modifies the string, so we work on a copy.
    char path_copy[512];
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    // Handle the root path "/" as a special case.
    if (strcmp(path_copy, "/") == 0) {
        result->found = true;
        result->entry_cluster = ROOT_DIR_CLUSTER;
        result->entry.attributes = ATTR_DIRECTORY;
        strcpy((char*)result->entry.filename, "/");
        return 0;
    }

    char* token = strtok(path_copy, "/");
    if (!token) { // Empty or invalid path
        return 0;
    }

    union data_cluster cluster_buffer;
    uint16_t current_cluster = ROOT_DIR_CLUSTER;

    while (token != NULL) {
        bool found_token = false;
        strncpy(result->name, token, sizeof(result->name) - 1); // Store last token name

        if (read_cluster(current_cluster, &cluster_buffer) != 0) {
            fprintf(stderr, "Error: Could not read cluster %u\n", current_cluster);
            return -1;
        }

        for (uint32_t i = 0; i < DIR_ENTRIES_PER_CLUSTER; ++i) {
            dir_entry_t* entry = &cluster_buffer.dir[i];
            if (entry->filename[0] != 0x00 && strcmp((char*)entry->filename, token) == 0) {
                // Found the entry for this token
                result->parent_cluster = current_cluster;
                result->entry_cluster = entry->first_block;
                result->entry_index = i;
                result->entry = *entry; // Copy the entry data

                current_cluster = entry->first_block;
                found_token = true;
                break;
            }
        }

        if (!found_token) {
            result->found = false;
            return 0; // Component of the path not found
        }

        token = strtok(NULL, "/"); // Get the next part of the path
    }

    result->found = true;
    return 0;
}

int fs_ls(const char* path) {
    path_search_result_t result;
    if (find_entry_by_path(path, &result) != 0 || !result.found) {
        fprintf(stderr, "ls: cannot access '%s': No such file or directory\n", path);
        return -1;
    }

    if (result.entry.attributes != ATTR_DIRECTORY) {
        // If it's a file, just print its name.
        printf("%s\n", result.entry.filename);
        return 0;
    }

    printf("Listing of '%s':\n", path);
    printf("Type  Size      Name\n");
    printf("----  --------  ------------------\n");

    union data_cluster cluster_buffer;
    uint16_t current_cluster = result.entry_cluster;

    // Read the directory cluster
    if (read_cluster(current_cluster, &cluster_buffer) != 0) {
        return -1;
    }

    for (uint32_t i = 0; i < DIR_ENTRIES_PER_CLUSTER; ++i) {
        dir_entry_t* entry = &cluster_buffer.dir[i];
        if (entry->filename[0] != 0x00) { // Check if the entry is in use
            const char* type = (entry->attributes == ATTR_DIRECTORY) ? "[D]" : "[F]";
            printf("%-4s  %-8u  %s\n", type, entry->size, entry->filename);
        }
    }

    return 0;
}

static uint16_t find_free_cluster() {
    // Start search after the reserved system area
    for (uint16_t i = DATA_CLUSTER_START; i < CLUSTER_COUNT; ++i) {
        if (g_fat_table[i] == FAT_ENTRY_FREE) {
            return i;
        }
    }
    return 0; // Invalid cluster index indicates no space
}

static int find_free_dir_entry(uint16_t dir_cluster_index, union data_cluster* dir_cluster) {
    if (read_cluster(dir_cluster_index, dir_cluster) != 0) {
        return -1; // Error reading cluster
    }

    for (int i = 0; i < DIR_ENTRIES_PER_CLUSTER; ++i) {
        if (dir_cluster->dir[i].filename[0] == 0x00) {
            return i; // Found a free slot
        }
    }
    return -2; // Directory is full
}

// --- High-Level Implementations ---
// (Keep find_entry_by_path and fs_ls)

int fs_mkdir(const char* path) {
    // 1. Separate parent path and new directory name
    char path_copy[512];
    strncpy(path_copy, path, sizeof(path_copy) - 1);

    char* new_dir_name = strrchr(path_copy, '/');
    if (new_dir_name == NULL) {
        fprintf(stderr, "mkdir: invalid path '%s'\n", path);
        return -1;
    }
    
    char parent_path[512];
    if (new_dir_name == path_copy) { // e.g., "/newdir"
        strcpy(parent_path, "/");
        new_dir_name++; // Skip the '/'
    } else {
        *new_dir_name = '\0'; // Terminate parent path string
        strcpy(parent_path, path_copy);
        new_dir_name++; // Skip the '/'
    }

    // 2. Find parent directory
    path_search_result_t parent_info;
    if (find_entry_by_path(parent_path, &parent_info) != 0 || !parent_info.found) {
        fprintf(stderr, "mkdir: cannot create directory '%s': No such file or directory\n", path);
        return -1;
    }
    if (parent_info.entry.attributes != ATTR_DIRECTORY) {
        fprintf(stderr, "mkdir: cannot create directory '%s': Not a directory\n", parent_path);
        return -1;
    }

    // 3. Find a free slot in the parent directory
    union data_cluster parent_cluster_data;
    int free_entry_index = find_free_dir_entry(parent_info.entry_cluster, &parent_cluster_data);

    if (free_entry_index < 0) {
        fprintf(stderr, "mkdir: cannot create directory '%s': Parent directory is full\n", path);
        return -1;
    }

    // 4. Find a free cluster for the new directory's contents
    uint16_t new_cluster_idx = find_free_cluster();
    if (new_cluster_idx == 0) {
        fprintf(stderr, "mkdir: cannot create directory '%s': No space left on device\n", path);
        return -1;
    }
    
    // 5. Fill in the new directory entry
    dir_entry_t* new_entry = &parent_cluster_data.dir[free_entry_index];
    strncpy((char*)new_entry->filename, new_dir_name, 17);
    new_entry->filename[17] = '\0';
    new_entry->attributes = ATTR_DIRECTORY;
    new_entry->first_block = new_cluster_idx;
    new_entry->size = 0; // Directories have a size of 0

    // 6. Update the FAT
    g_fat_table[new_cluster_idx] = FAT_ENTRY_EOF;

    // 7. Prepare the new directory's own cluster (it's empty)
    union data_cluster new_dir_cluster_data;
    memset(&new_dir_cluster_data, 0, sizeof(new_dir_cluster_data));

    // 8. Write all changes to disk
    if (write_cluster(parent_info.entry_cluster, &parent_cluster_data) != 0) return -1;
    if (write_cluster(new_cluster_idx, &new_dir_cluster_data) != 0) return -1;
    // Persist the entire FAT
    for (uint16_t i = 0; i < FAT_CLUSTER_COUNT; ++i) {
        if (write_cluster(FAT_CLUSTER_START + i, (uint8_t*)g_fat_table + (i * CLUSTER_SIZE)) != 0) return -1;
    }

    printf("Directory '%s' created.\n", path);
    return 0;
}

int fs_create(const char* path) {
    // Logic is nearly identical to mkdir, with a few key differences.
    // 1. Separate parent path and new file name (same as mkdir)
    char path_copy[512]; strncpy(path_copy, path, sizeof(path_copy)-1);
    char* new_file_name = strrchr(path_copy, '/');
    if(!new_file_name) { fprintf(stderr, "create: invalid path '%s'\n", path); return -1; }
    char parent_path[512];
    if (new_file_name == path_copy) { strcpy(parent_path, "/"); new_file_name++; }
    else { *new_file_name = '\0'; strcpy(parent_path, path_copy); new_file_name++; }

    // 2. Find parent (same as mkdir)
    path_search_result_t parent_info;
    if (find_entry_by_path(parent_path, &parent_info) != 0 || !parent_info.found || parent_info.entry.attributes != ATTR_DIRECTORY) {
        fprintf(stderr, "create: cannot create file '%s': Parent path not found or not a directory\n", path);
        return -1;
    }

    // 3. Find free slot (same as mkdir)
    union data_cluster parent_cluster_data;
    int free_entry_index = find_free_dir_entry(parent_info.entry_cluster, &parent_cluster_data);
    if(free_entry_index < 0) { fprintf(stderr, "create: cannot create file '%s': Directory full\n", path); return -1; }

    // 4. Find free cluster (same as mkdir)
    uint16_t new_cluster_idx = find_free_cluster();
    if(new_cluster_idx == 0) { fprintf(stderr, "create: cannot create file '%s': No space left\n", path); return -1; }

    // 5. Fill entry - **DIFFERENCES ARE HERE**
    dir_entry_t* new_entry = &parent_cluster_data.dir[free_entry_index];
    strncpy((char*)new_entry->filename, new_file_name, 17);
    new_entry->filename[17] = '\0';
    new_entry->attributes = ATTR_ARCHIVE; // It's a file
    new_entry->first_block = new_cluster_idx; // A file starts with a cluster...
    new_entry->size = 0; // ...but its initial size is 0

    // 6. Update FAT (same as mkdir)
    g_fat_table[new_cluster_idx] = FAT_ENTRY_EOF;

    // 7. Write changes - **DIFFERENCE IS HERE**
    // We only need to write the parent dir and the FAT.
    // No need to write an empty data cluster for a 0-byte file.
    if (write_cluster(parent_info.entry_cluster, &parent_cluster_data) != 0) return -1;
    for (uint16_t i = 0; i < FAT_CLUSTER_COUNT; ++i) {
        if (write_cluster(FAT_CLUSTER_START + i, (uint8_t*)g_fat_table + (i * CLUSTER_SIZE)) != 0) return -1;
    }
    
    printf("File '%s' created.\n", path);
    return 0;
}

// Helper to free a chain of clusters in the FAT
static void free_cluster_chain(uint16_t starting_cluster) {
    uint16_t current = starting_cluster;
    while (current != 0 && current < FAT_ENTRY_EOF) {
        uint16_t next = g_fat_table[current];
        g_fat_table[current] = FAT_ENTRY_FREE;
        current = next;
    }
}

int fs_unlink(const char* path) {
    path_search_result_t result;
    if (find_entry_by_path(path, &result) != 0 || !result.found) {
        fprintf(stderr, "unlink: cannot remove '%s': No such file or directory\n", path);
        return -1;
    }

    // If it's a directory, check if it's empty
    if (result.entry.attributes == ATTR_DIRECTORY) {
        union data_cluster dir_content;
        if (read_cluster(result.entry_cluster, &dir_content) != 0) return -1;
        for (int i = 0; i < DIR_ENTRIES_PER_CLUSTER; ++i) {
            if (dir_content.dir[i].filename[0] != 0x00) {
                fprintf(stderr, "unlink: failed to remove '%s': Directory not empty\n", path);
                return -1;
            }
        }
    }

    // Free the cluster chain in the FAT
    free_cluster_chain(result.entry.first_block);

    // Clear the entry in the parent directory
    union data_cluster parent_dir_content;
    if (read_cluster(result.parent_cluster, &parent_dir_content) != 0) return -1;
    memset(&parent_dir_content.dir[result.entry_index], 0, sizeof(dir_entry_t));

    // Write changes to disk
    if (write_cluster(result.parent_cluster, &parent_dir_content) != 0) return -1;
    for (uint16_t i = 0; i < FAT_CLUSTER_COUNT; ++i) { // Persist FAT
        if (write_cluster(FAT_CLUSTER_START + i, (uint8_t*)g_fat_table + (i * CLUSTER_SIZE)) != 0) return -1;
    }

    printf("Removed '%s'.\n", path);
    return 0;
}

int fs_read(const char* path) {
    path_search_result_t result;
    if (find_entry_by_path(path, &result) != 0 || !result.found) {
        fprintf(stderr, "read: cannot read '%s': No such file or directory\n", path);
        return -1;
    }
    if (result.entry.attributes != ATTR_ARCHIVE) {
        fprintf(stderr, "read: cannot read '%s': Not a file\n", path);
        return -1;
    }

    uint8_t buffer[CLUSTER_SIZE];
    uint16_t current_cluster = result.entry.first_block;
    uint32_t bytes_to_read = result.entry.size;

    while (bytes_to_read > 0 && current_cluster < FAT_ENTRY_EOF) {
        if (read_cluster(current_cluster, buffer) != 0) return -1;
        uint32_t len = (bytes_to_read > CLUSTER_SIZE) ? CLUSTER_SIZE : bytes_to_read;
        fwrite(buffer, 1, len, stdout);
        bytes_to_read -= len;
        current_cluster = g_fat_table[current_cluster];
    }
    printf("\n");
    return 0;
}

int fs_write(const char* path, const char* content) {
    path_search_result_t result;
    if (find_entry_by_path(path, &result) != 0 || !result.found || result.entry.attributes != ATTR_ARCHIVE) {
        fprintf(stderr, "write: cannot write to '%s': No such file or not a file\n", path);
        return -1;
    }

    // Free existing content
    free_cluster_chain(result.entry.first_block);

    // Allocate new content
    uint32_t content_len = strlen(content);
    uint16_t current_cluster = 0;
    uint16_t first_cluster = 0;

    if (content_len > 0) {
        const char* p = content;
        while (p < content + content_len) {
            uint16_t next_cluster = find_free_cluster();
            if (next_cluster == 0) {
                fprintf(stderr, "write: No space left on device\n");
                free_cluster_chain(first_cluster); // Rollback allocations
                return -1;
            }

            if (first_cluster == 0) {
                first_cluster = next_cluster;
            } else {
                g_fat_table[current_cluster] = next_cluster;
            }
            current_cluster = next_cluster;
            g_fat_table[current_cluster] = FAT_ENTRY_EOF;

            uint8_t buffer[CLUSTER_SIZE] = {0};
            uint32_t len = (content_len - (p - content) > CLUSTER_SIZE) ? CLUSTER_SIZE : content_len - (p - content);
            memcpy(buffer, p, len);
            if (write_cluster(current_cluster, buffer) != 0) return -1;
            p += len;
        }
    } else {
        first_cluster = find_free_cluster(); // Allocate one cluster even for empty write
        g_fat_table[first_cluster] = FAT_ENTRY_EOF;
    }

    // Update directory entry
    union data_cluster parent_dir_content;
    if (read_cluster(result.parent_cluster, &parent_dir_content) != 0) return -1;
    parent_dir_content.dir[result.entry_index].first_block = first_cluster;
    parent_dir_content.dir[result.entry_index].size = content_len;

    // Write changes to disk
    if (write_cluster(result.parent_cluster, &parent_dir_content) != 0) return -1;
    for (uint16_t i = 0; i < FAT_CLUSTER_COUNT; ++i) { // Persist FAT
        if (write_cluster(FAT_CLUSTER_START + i, (uint8_t*)g_fat_table + (i * CLUSTER_SIZE)) != 0) return -1;
    }
    
    printf("Wrote %u bytes to '%s'.\n", content_len, path);
    return 0;
}

// Append is very complex; a simplified version can be built on read+write, but a true append is way more efficient
int fs_append(const char* path, const char* content) {
    path_search_result_t result;
    if (find_entry_by_path(path, &result) != 0 || !result.found || result.entry.attributes != ATTR_ARCHIVE) {
        fprintf(stderr, "append: cannot append to '%s': No such file or not a file\n", path);
        return -1;
    }

    uint32_t content_len = strlen(content);
    if (content_len == 0) {
        return 0; // Nothing to append
    }

    uint16_t current_cluster = result.entry.first_block;
    uint32_t original_size = result.entry.size;

    // 1. Traverse to the last cluster of the file
    if (original_size > 0) {
        while (g_fat_table[current_cluster] != FAT_ENTRY_EOF) {
            current_cluster = g_fat_table[current_cluster];
        }
    }
    // If original_size is 0, current_cluster is the first pre-allocated block.

    // 2. Prepare for writing
    union data_cluster buffer;
    uint32_t offset_in_cluster = original_size % CLUSTER_SIZE;
    
    // If the last cluster is full, or if the file was empty, the offset is 0 for the *next* cluster.
    if (offset_in_cluster == 0 && original_size > 0) {
        uint16_t new_cluster = find_free_cluster();
        if (new_cluster == 0) { fprintf(stderr, "append: No space left on device\n"); return -1; }
        g_fat_table[current_cluster] = new_cluster;
        current_cluster = new_cluster;
        g_fat_table[current_cluster] = FAT_ENTRY_EOF;
        memset(&buffer, 0, sizeof(buffer)); // New cluster is empty
    } else {
        if (read_cluster(current_cluster, &buffer) != 0) return -1;
    }

    const char* p = content;
    uint32_t remaining_content = content_len;

    // 3. Main append loop
    while (remaining_content > 0) {
        uint32_t space_in_buffer = CLUSTER_SIZE - offset_in_cluster;
        uint32_t bytes_to_copy = (remaining_content > space_in_buffer) ? space_in_buffer : remaining_content;

        memcpy(buffer.data + offset_in_cluster, p, bytes_to_copy);
        p += bytes_to_copy;
        remaining_content -= bytes_to_copy;
        
        // Write the modified cluster back
        if (write_cluster(current_cluster, &buffer) != 0) return -1;

        // If we still have content left, allocate a new cluster
        if (remaining_content > 0) {
            uint16_t new_cluster = find_free_cluster();
            if (new_cluster == 0) { fprintf(stderr, "append: No space left on device\n"); return -1; } // Error: disk full
            g_fat_table[current_cluster] = new_cluster;
            current_cluster = new_cluster;
            g_fat_table[current_cluster] = FAT_ENTRY_EOF;
            offset_in_cluster = 0; // The new cluster will be written from the beginning
            memset(&buffer, 0, sizeof(buffer)); // Clear buffer for the new cluster
        }
    }

    // 4. Update directory entry with new size
    union data_cluster parent_dir_content;
    if (read_cluster(result.parent_cluster, &parent_dir_content) != 0) return -1;
    parent_dir_content.dir[result.entry_index].size = original_size + content_len;

    // 5. Write all changes to disk
    if (write_cluster(result.parent_cluster, &parent_dir_content) != 0) return -1;
    for (uint16_t i = 0; i < FAT_CLUSTER_COUNT; ++i) { // Persist FAT
        if (write_cluster(FAT_CLUSTER_START + i, (uint8_t*)g_fat_table + (i * CLUSTER_SIZE)) != 0) return -1;
    }

    printf("Appended %u bytes to '%s'.\n", content_len, path);
    return 0;
}