/*
 * fs.c - "OnlyFiles" block-based filesystem implementation.
 *
 * Operating Systems - Exercise 3 (Part B)
 *
 * Disk layout (10 MB virtual disk, 4096-byte blocks, 2560 blocks total):
 *
 *   Block 0          : Superblock                (1 block,  4 KB)
 *   Block 1          : Block bitmap              (1 block,  4 KB)
 *   Blocks 2..9      : Inode table                (8 blocks, 32 KB -> 256 inodes)
 *   Blocks 10..2559  : Data blocks                (2550 blocks, ~9.96 MB)
 *
 * Only low-level system calls (open/lseek/read/write/close) are used to
 * access the virtual disk, as required by the assignment. No dynamic
 * memory allocation is used.
 */

#include "fs.h"

/* ===================================================================== */
/*                          Disk layout constants                        */
/* ===================================================================== */

#define SUPERBLOCK_BLOCK   0
#define BITMAP_BLOCK       1
#define INODE_TABLE_START  2

/*
 * Number of blocks occupied by the inode table, per the disk layout fixed
 * by the assignment spec (8 blocks = 32 KB for 256 inodes).
 *
 * Note: the spec describes each on-disk inode as "128 bytes", but the
 * compiler may pad the `inode` struct from fs.h to a different in-memory
 * size (e.g. due to alignment of the `int` array after a `char` array).
 * We must NOT derive the disk layout from sizeof(inode), since that would
 * silently shift the start of the data-block region away from block 10 as
 * specified. Instead, we treat 128 bytes as the fixed on-disk slot size
 * for each inode, and we are careful to never write more than
 * sizeof(inode) bytes into that slot (sizeof(inode) is always <= 128 here).
 */
#define INODE_TABLE_BLOCKS 8

#define DATA_BLOCK_START   (INODE_TABLE_START + INODE_TABLE_BLOCKS) /* 10 */

/* Fixed on-disk size of one inode slot, per the assignment spec. */
#define INODE_DISK_SIZE 128

/* Number of inodes that fit in a single block, using the fixed on-disk
 * inode slot size (NOT sizeof(inode), see note above). */
#define INODES_PER_BLOCK   (BLOCK_SIZE / INODE_DISK_SIZE)

/* Safety check: the in-memory `inode` struct (as defined in fs.h, with
 * whatever padding the compiler applies) must fit inside the fixed
 * 128-byte on-disk slot, or inode records would overlap on disk. */
_Static_assert(sizeof(inode) <= INODE_DISK_SIZE,
               "inode struct does not fit in the fixed 128-byte on-disk slot");

/* ===================================================================== */
/*                           Global/module state                         */
/* ===================================================================== */

/* File descriptor of the currently mounted virtual disk. -1 if not mounted. */
static int g_disk_fd = -1;

/* In-memory cached copy of the superblock, kept in sync with disk. */
static superblock g_sb;

/* ===================================================================== */
/*                          Low-level block I/O                          */
/* ===================================================================== */

/*
 * Reads exactly BLOCK_SIZE bytes from block `block_num` into `buffer`.
 * Returns 0 on success, -1 on failure.
 */
static int read_block(int block_num, void *buffer)
{
    if (g_disk_fd < 0) {
        return -1;
    }

    off_t offset = (off_t)block_num * BLOCK_SIZE;
    if (lseek(g_disk_fd, offset, SEEK_SET) != offset) {
        return -1;
    }

    ssize_t total_read = 0;
    while (total_read < BLOCK_SIZE) {
        ssize_t n = read(g_disk_fd, (char *)buffer + total_read, BLOCK_SIZE - total_read);
        if (n < 0) {
            return -1;
        }
        if (n == 0) {
            /* Unexpected EOF; treat remainder as zeroed (shouldn't happen for
             * a properly formatted disk, but avoids reading garbage). */
            memset((char *)buffer + total_read, 0, BLOCK_SIZE - total_read);
            break;
        }
        total_read += n;
    }
    return 0;
}

/*
 * Writes exactly BLOCK_SIZE bytes from `buffer` to block `block_num`.
 * Returns 0 on success, -1 on failure.
 */
static int write_block(int block_num, const void *buffer)
{
    if (g_disk_fd < 0) {
        return -1;
    }

    off_t offset = (off_t)block_num * BLOCK_SIZE;
    if (lseek(g_disk_fd, offset, SEEK_SET) != offset) {
        return -1;
    }

    ssize_t total_written = 0;
    while (total_written < BLOCK_SIZE) {
        ssize_t n = write(g_disk_fd, (const char *)buffer + total_written, BLOCK_SIZE - total_written);
        if (n < 0) {
            return -1;
        }
        total_written += n;
    }
    return 0;
}

/* ===================================================================== */
/*                       Superblock helper functions                     */
/* ===================================================================== */

/* Reads the superblock from disk into `target`. Returns 0 on success, -1 on failure. */
static int read_superblock(superblock *target)
{
    /* superblock is smaller than BLOCK_SIZE; we read a full block and copy
     * the relevant prefix out, to respect the "always operate in full
     * blocks" pattern required by the assignment. */
    char block_buf[BLOCK_SIZE];
    if (read_block(SUPERBLOCK_BLOCK, block_buf) < 0) {
        return -1;
    }
    memcpy(target, block_buf, sizeof(superblock));
    return 0;
}

/* Writes the superblock to disk. Returns 0 on success, -1 on failure. */
static int write_superblock(const superblock *source)
{
    char block_buf[BLOCK_SIZE];
    memset(block_buf, 0, BLOCK_SIZE);
    memcpy(block_buf, source, sizeof(superblock));
    return write_block(SUPERBLOCK_BLOCK, block_buf);
}

/* Persists the in-memory superblock (g_sb) to disk. Returns 0/-1. */
static int sync_superblock(void)
{
    return write_superblock(&g_sb);
}

/* ===================================================================== */
/*                        Bitmap helper functions                        */
/* ===================================================================== */

/* Reads the block bitmap from disk into `bitmap` (MAX_BLOCKS / 8 bytes). */
static int read_bitmap(unsigned char *bitmap)
{
    char block_buf[BLOCK_SIZE];
    if (read_block(BITMAP_BLOCK, block_buf) < 0) {
        return -1;
    }
    memcpy(bitmap, block_buf, MAX_BLOCKS / 8);
    return 0;
}

/* Writes the block bitmap to disk. */
static int write_bitmap(const unsigned char *bitmap)
{
    char block_buf[BLOCK_SIZE];
    memset(block_buf, 0, BLOCK_SIZE);
    memcpy(block_buf, bitmap, MAX_BLOCKS / 8);
    return write_block(BITMAP_BLOCK, block_buf);
}

/* Marks block N as used directly in a bitmap buffer (in-memory helper). */
static void mark_block_used(unsigned char *bitmap, int n)
{
    bitmap[n / 8] |= (unsigned char)(1 << (n % 8));
}

/* Marks block N as free directly in a bitmap buffer (in-memory helper). */
static void mark_block_free(unsigned char *bitmap, int n)
{
    bitmap[n / 8] &= (unsigned char)~(1 << (n % 8));
}

/* Checks whether block N is marked as used in the bitmap buffer. */
static int is_block_used(const unsigned char *bitmap, int n)
{
    return (bitmap[n / 8] & (1 << (n % 8))) != 0;
}

/*
 * Finds a free data block by scanning the bitmap.
 * Returns the block number on success, or -1 if no free block is available.
 * Only searches within the data block region (DATA_BLOCK_START..MAX_BLOCKS-1).
 */
static int find_free_block(const unsigned char *bitmap)
{
    for (int b = DATA_BLOCK_START; b < MAX_BLOCKS; b++) {
        if (!is_block_used(bitmap, b)) {
            return b;
        }
    }
    return -1;
}

/* ===================================================================== */
/*                        Inode helper functions                         */
/* ===================================================================== */

/*
 * Reads inode number `inode_num` from disk into `target`.
 * Returns 0 on success, -1 on invalid inode_num or I/O failure.
 */
static int read_inode(int inode_num, inode *target)
{
    if (inode_num < 0 || inode_num >= MAX_FILES) {
        return -1;
    }

    int block_index = inode_num / INODES_PER_BLOCK;
    int offset_in_block = (inode_num % INODES_PER_BLOCK) * INODE_DISK_SIZE;
    int block_num = INODE_TABLE_START + block_index;

    char block_buf[BLOCK_SIZE];
    if (read_block(block_num, block_buf) < 0) {
        return -1;
    }
    memcpy(target, block_buf + offset_in_block, sizeof(inode));
    return 0;
}

/*
 * Writes `source` into inode slot `inode_num` on disk.
 * Returns 0 on success, -1 on invalid inode_num or I/O failure.
 */
static int write_inode(int inode_num, const inode *source)
{
    if (inode_num < 0 || inode_num >= MAX_FILES) {
        return -1;
    }

    int block_index = inode_num / INODES_PER_BLOCK;
    int offset_in_block = (inode_num % INODES_PER_BLOCK) * INODE_DISK_SIZE;
    int block_num = INODE_TABLE_START + block_index;

    char block_buf[BLOCK_SIZE];
    if (read_block(block_num, block_buf) < 0) {
        return -1;
    }
    memcpy(block_buf + offset_in_block, source, sizeof(inode));
    return write_block(block_num, block_buf);
}

/*
 * Searches all inodes for one matching `filename`.
 * Returns the inode number on success, or -1 if not found / on error.
 */
static int find_inode(const char *filename)
{
    if (filename == NULL) {
        return -1;
    }

    inode tmp;
    for (int i = 0; i < MAX_FILES; i++) {
        if (read_inode(i, &tmp) < 0) {
            return -1;
        }
        if (tmp.used && strncmp(tmp.name, filename, MAX_FILENAME) == 0) {
            return i;
        }
    }
    return -1;
}

/*
 * Finds the first free (unused) inode slot.
 * Returns the inode number on success, or -1 if none are free / on error.
 */
static int find_free_inode(void)
{
    inode tmp;
    for (int i = 0; i < MAX_FILES; i++) {
        if (read_inode(i, &tmp) < 0) {
            return -1;
        }
        if (!tmp.used) {
            return i;
        }
    }
    return -1;
}

/* ===================================================================== */
/*                          Filesystem operations                        */
/* ===================================================================== */

int fs_format(const char *disk_path)
{
    if (disk_path == NULL) {
        return -1;
    }

    /* Create (or truncate/overwrite) the virtual disk file. */
    int fd = open(disk_path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        return -1;
    }

    char zero_block[BLOCK_SIZE];
    memset(zero_block, 0, BLOCK_SIZE);

    /* Grow the file to MAX_BLOCKS * BLOCK_SIZE bytes (10 MB) and zero it
     * out, writing block by block using only low-level I/O. */
    for (int b = 0; b < MAX_BLOCKS; b++) {
        off_t offset = (off_t)b * BLOCK_SIZE;
        if (lseek(fd, offset, SEEK_SET) != offset) {
            close(fd);
            return -1;
        }
        ssize_t total_written = 0;
        while (total_written < BLOCK_SIZE) {
            ssize_t n = write(fd, zero_block + total_written, BLOCK_SIZE - total_written);
            if (n < 0) {
                close(fd);
                return -1;
            }
            total_written += n;
        }
    }

    /* Build the superblock. */
    superblock sb;
    sb.total_blocks = MAX_BLOCKS;
    sb.block_size = BLOCK_SIZE;
    sb.total_inodes = MAX_FILES;
    sb.free_inodes = MAX_FILES; /* none allocated yet */
    /* Free blocks = all data blocks (metadata blocks are never "free"). */
    sb.free_blocks = MAX_BLOCKS - DATA_BLOCK_START;

    /* Build the bitmap: mark metadata blocks (superblock, bitmap, inode
     * table) as used; everything from DATA_BLOCK_START onward is free. */
    unsigned char bitmap[MAX_BLOCKS / 8];
    memset(bitmap, 0, sizeof(bitmap));
    for (int b = 0; b < DATA_BLOCK_START; b++) {
        mark_block_used(bitmap, b);
    }

    /* Write superblock. */
    char block_buf[BLOCK_SIZE];
    memset(block_buf, 0, BLOCK_SIZE);
    memcpy(block_buf, &sb, sizeof(superblock));
    if (lseek(fd, (off_t)SUPERBLOCK_BLOCK * BLOCK_SIZE, SEEK_SET) < 0 ||
        write(fd, block_buf, BLOCK_SIZE) != BLOCK_SIZE) {
        close(fd);
        return -1;
    }

    /* Write bitmap. */
    memset(block_buf, 0, BLOCK_SIZE);
    memcpy(block_buf, bitmap, sizeof(bitmap));
    if (lseek(fd, (off_t)BITMAP_BLOCK * BLOCK_SIZE, SEEK_SET) < 0 ||
        write(fd, block_buf, BLOCK_SIZE) != BLOCK_SIZE) {
        close(fd);
        return -1;
    }

    /* Write an empty (zeroed -> used=0) inode table. The disk was already
     * zero-filled above, so the inode table blocks are already correct
     * (used = 0 for every inode, since 0 means "free"). Nothing more to do.
     */

    close(fd);
    return 0;
}

int fs_mount(const char *disk_path)
{
    if (disk_path == NULL) {
        return -1;
    }

    /* If something is already mounted, do not silently leak the fd. */
    if (g_disk_fd >= 0) {
        return -1;
    }

    int fd = open(disk_path, O_RDWR);
    if (fd < 0) {
        return -1;
    }

    g_disk_fd = fd;

    superblock sb;
    if (read_superblock(&sb) < 0) {
        close(g_disk_fd);
        g_disk_fd = -1;
        return -1;
    }

    /* Basic sanity validation of the superblock (per the assignment we may
     * assume the disk is a valid filesystem, but a cheap check costs
     * nothing and helps catch obvious mistakes). */
    if (sb.total_blocks != MAX_BLOCKS || sb.block_size != BLOCK_SIZE ||
        sb.total_inodes != MAX_FILES) {
        close(g_disk_fd);
        g_disk_fd = -1;
        return -1;
    }

    g_sb = sb;
    return 0;
}

void fs_unmount(void)
{
    if (g_disk_fd < 0) {
        return;
    }

    /* Flush the in-memory superblock to disk (all other metadata - bitmap
     * and inodes - is written through on every modification, so there is
     * nothing else cached to flush). */
    sync_superblock();

    close(g_disk_fd);
    g_disk_fd = -1;
}

/* ===================================================================== */
/*                             File operations                           */
/* ===================================================================== */

int fs_create(const char *filename)
{
    if (g_disk_fd < 0 || filename == NULL) {
        return -3;
    }

    size_t len = strnlen(filename, MAX_FILENAME);
    if (len == 0 || len >= MAX_FILENAME) {
        /* Empty name, or name too long to fit with a null terminator. */
        return -3;
    }

    if (find_inode(filename) >= 0) {
        return -1; /* already exists */
    }

    int slot = find_free_inode();
    if (slot < 0) {
        return -2; /* no free inodes */
    }

    inode new_inode;
    memset(&new_inode, 0, sizeof(inode));
    new_inode.used = 1;
    strncpy(new_inode.name, filename, MAX_FILENAME - 1);
    new_inode.name[MAX_FILENAME - 1] = '\0';
    new_inode.size = 0;
    for (int i = 0; i < MAX_DIRECT_BLOCKS; i++) {
        new_inode.blocks[i] = -1; /* -1 marks an unused block pointer slot */
    }

    if (write_inode(slot, &new_inode) < 0) {
        return -3;
    }

    g_sb.free_inodes -= 1;
    if (sync_superblock() < 0) {
        return -3;
    }

    return 0;
}

int fs_delete(const char *filename)
{
    if (g_disk_fd < 0 || filename == NULL) {
        return -2;
    }

    int slot = find_inode(filename);
    if (slot < 0) {
        return -1; /* does not exist */
    }

    inode target;
    if (read_inode(slot, &target) < 0) {
        return -2;
    }

    unsigned char bitmap[MAX_BLOCKS / 8];
    if (read_bitmap(bitmap) < 0) {
        return -2;
    }

    int freed_blocks = 0;
    for (int i = 0; i < MAX_DIRECT_BLOCKS; i++) {
        int b = target.blocks[i];
        if (b >= DATA_BLOCK_START && b < MAX_BLOCKS) {
            mark_block_free(bitmap, b);
            freed_blocks++;
        }
    }

    if (write_bitmap(bitmap) < 0) {
        return -2;
    }

    /* Free the inode. */
    inode empty;
    memset(&empty, 0, sizeof(inode));
    empty.used = 0;
    for (int i = 0; i < MAX_DIRECT_BLOCKS; i++) {
        empty.blocks[i] = -1;
    }
    if (write_inode(slot, &empty) < 0) {
        return -2;
    }

    g_sb.free_blocks += freed_blocks;
    g_sb.free_inodes += 1;
    if (sync_superblock() < 0) {
        return -2;
    }

    return 0;
}

int fs_list(char filenames[][MAX_FILENAME], int max_files)
{
    if (g_disk_fd < 0 || filenames == NULL || max_files < 0) {
        return -1;
    }

    int count = 0;
    inode tmp;
    for (int i = 0; i < MAX_FILES && count < max_files; i++) {
        if (read_inode(i, &tmp) < 0) {
            return -1;
        }
        if (tmp.used) {
            strncpy(filenames[count], tmp.name, MAX_FILENAME - 1);
            filenames[count][MAX_FILENAME - 1] = '\0';
            count++;
        }
    }

    return count;
}

int fs_write(const char *filename, const void *data, int size)
{
    if (g_disk_fd < 0 || filename == NULL || size < 0) {
        return -3;
    }
    if (size > 0 && data == NULL) {
        return -3;
    }

    int slot = find_inode(filename);
    if (slot < 0) {
        return -1; /* file does not exist */
    }

    inode target;
    if (read_inode(slot, &target) < 0) {
        return -3;
    }

    /* Maximum file size is MAX_DIRECT_BLOCKS * BLOCK_SIZE (48 KB). */
    if (size > MAX_DIRECT_BLOCKS * BLOCK_SIZE) {
        return -2;
    }

    int blocks_needed = (size == 0) ? 0 : ((size + BLOCK_SIZE - 1) / BLOCK_SIZE);

    unsigned char bitmap[MAX_BLOCKS / 8];
    if (read_bitmap(bitmap) < 0) {
        return -3;
    }

    /* Count currently allocated blocks for this file, and free them all in
     * the bitmap first (we will reallocate fresh blocks below). This mirrors
     * the spec: "frees any previously allocated blocks" then "allocates new
     * blocks as needed". */
    int old_block_count = 0;
    for (int i = 0; i < MAX_DIRECT_BLOCKS; i++) {
        if (target.blocks[i] >= DATA_BLOCK_START && target.blocks[i] < MAX_BLOCKS) {
            old_block_count++;
        }
    }

    /* Check there is enough free space for the new content BEFORE freeing
     * the old blocks, accounting for the fact that the old blocks will be
     * released as part of this operation. */
    int available_after_free = g_sb.free_blocks + old_block_count;
    if (blocks_needed > available_after_free) {
        return -2; /* not enough free space */
    }

    /* Now actually free the old blocks in the bitmap. */
    for (int i = 0; i < MAX_DIRECT_BLOCKS; i++) {
        if (target.blocks[i] >= DATA_BLOCK_START && target.blocks[i] < MAX_BLOCKS) {
            mark_block_free(bitmap, target.blocks[i]);
        }
        target.blocks[i] = -1;
    }
    g_sb.free_blocks += old_block_count;

    /* Allocate new blocks and write the data into them. */
    const unsigned char *src = (const unsigned char *)data;
    int remaining = size;
    char block_buf[BLOCK_SIZE];

    for (int i = 0; i < blocks_needed; i++) {
        int b = find_free_block(bitmap);
        if (b < 0) {
            /* Should not happen given the check above, but bail out safely
             * if it does (no partial corrupt state left worse than needed). */
            write_bitmap(bitmap);
            sync_superblock();
            return -2;
        }
        mark_block_used(bitmap, b);
        g_sb.free_blocks -= 1;
        target.blocks[i] = b;

        int chunk = remaining < BLOCK_SIZE ? remaining : BLOCK_SIZE;
        memset(block_buf, 0, BLOCK_SIZE);
        memcpy(block_buf, src + (size_t)(size - remaining), (size_t)chunk);

        if (write_block(b, block_buf) < 0) {
            return -3;
        }
        remaining -= chunk;
    }

    target.size = size;

    if (write_bitmap(bitmap) < 0) {
        return -3;
    }
    if (write_inode(slot, &target) < 0) {
        return -3;
    }
    if (sync_superblock() < 0) {
        return -3;
    }

    return 0;
}

int fs_read(const char *filename, void *buffer, int size)
{
    if (g_disk_fd < 0 || filename == NULL || buffer == NULL || size < 0) {
        return -3;
    }

    int slot = find_inode(filename);
    if (slot < 0) {
        return -1; /* file does not exist */
    }

    inode target;
    if (read_inode(slot, &target) < 0) {
        return -3;
    }

    int bytes_to_read = target.size < size ? target.size : size;
    if (bytes_to_read <= 0) {
        return 0;
    }

    unsigned char *dst = (unsigned char *)buffer;
    int remaining = bytes_to_read;
    int block_index = 0;
    char block_buf[BLOCK_SIZE];

    while (remaining > 0) {
        if (block_index >= MAX_DIRECT_BLOCKS) {
            /* Should never happen given size constraints, but guard anyway. */
            break;
        }
        int b = target.blocks[block_index];
        if (b < DATA_BLOCK_START || b >= MAX_BLOCKS) {
            break; /* no more valid data blocks */
        }
        if (read_block(b, block_buf) < 0) {
            return -3;
        }
        int chunk = remaining < BLOCK_SIZE ? remaining : BLOCK_SIZE;
        memcpy(dst + (size_t)(bytes_to_read - remaining), block_buf, (size_t)chunk);
        remaining -= chunk;
        block_index++;
    }

    return bytes_to_read;
}
