#ifndef _FS_H
#define _FS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

/* ===================================================================== */
/*                           Static Constants                            */
/* ===================================================================== */

/** Size of each block in bytes. */
#define BLOCK_SIZE 4096

/** Total number of blocks in the 10 MB virtual disk. */
#define MAX_BLOCKS 2560

/** Maximum number of files (inodes). */
#define MAX_FILES 256

/** Maximum filename length (including null terminator). */
#define MAX_FILENAME 29

/** Maximum number of direct block pointers per inode. */
#define MAX_DIRECT_BLOCKS 12

/* ===================================================================== */
/*                        On-Disk Data Structures                        */
/* ===================================================================== */

/**
 * @brief Superblock structure.
 *
 * Stored at block 0. Contains global filesystem metadata.
 */
typedef struct {
    int total_blocks;   /**< Total number of blocks (2560 for 10 MB). */
    int block_size;     /**< Size of each block (4096 bytes). */
    int free_blocks;    /**< Number of available blocks. */
    int total_inodes;   /**< Total number of inodes (256). */
    int free_inodes;    /**< Number of available inodes. */
} superblock;

/**
 * @brief Inode structure.
 *
 * Each inode is 128 bytes. Stores metadata for a single file.
 */
typedef struct {
    int used;                       /**< 1 if inode is active, 0 if free. */
    char name[MAX_FILENAME];        /**< File name (up to 28 chars excluding null). */
    int size;                       /**< File size in bytes. */
    int blocks[MAX_DIRECT_BLOCKS];  /**< Direct block pointers (12 max). */
} inode;

/* ===================================================================== */
/*                           External Interface                          */
/* ===================================================================== */

/**
 * @brief Creates and initializes a virtual disk file.
 *
 * If the file already exists, it is overwritten. Creates a 10 MB file and initializes
 * the superblock, bitmap, and inode table. All blocks except metadata blocks are marked free.
 *
 * @param disk_path Path where the virtual disk file will be created.
 * @return 0 on success; -1 on failure.
 */
int fs_format(const char *disk_path);

/**
 * @brief Loads an existing filesystem from a virtual disk file.
 *
 * Opens the virtual disk file, reads and validates the superblock,
 * and loads necessary metadata into memory.
 *
 * @param disk_path Path to the virtual disk file.
 * @return 0 on success; -1 on failure.
 */
int fs_mount(const char *disk_path);

/**
 * @brief Unmounts the filesystem.
 *
 * Flushes any cached data to disk and closes the virtual disk file.
 */
void fs_unmount();

/**
 * @brief Creates a new empty file in the filesystem.
 *
 * @param filename Null-terminated string (max 28 chars excluding null).
 * @return 0 on success; -1 if file already exists; -2 if no free inodes; -3 for other errors.
 */
int fs_create(const char *filename);

/**
 * @brief Removes a file and frees its blocks.
 *
 * @param filename Null-terminated string.
 * @return 0 on success; -1 if file does not exist; -2 for other errors.
 */
int fs_delete(const char *filename);

/**
 * @brief Lists files in the filesystem.
 *
 * @param filenames Pre-allocated 2D array to receive filenames.
 * @param max_files Maximum number of filenames to retrieve.
 * @return Number of files found (0 to max_files); -1 on error.
 */
int fs_list(char filenames[][MAX_FILENAME], int max_files);

/**
 * @brief Writes data to a file, overwriting any existing content.
 *
 * @param filename Target file.
 * @param data Pointer to data buffer.
 * @param size Number of bytes to write.
 * @return 0 on success; -1 if file does not exist; -2 if out of space; -3 for other errors.
 */
int fs_write(const char *filename, const void *data, int size);

/**
 * @brief Reads file content into a buffer.
 *
 * @param filename Source file.
 * @param buffer Pre-allocated buffer to receive data.
 * @param size Buffer size in bytes.
 * @return Number of bytes read on success; -1 if file does not exist; -3 for other errors.
 */
int fs_read(const char *filename, void *buffer, int size);

#endif /* _FS_H */
