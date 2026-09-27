# virtual-block-filesystem-c ("OnlyFiles")

A block-based virtual filesystem implemented in C from raw disk I/O — no OS filesystem calls beyond `open`/`lseek`/`read`/`write`/`close` on a single flat "virtual disk" file. No dynamic memory allocation is used anywhere in the implementation.

Built as part of the Operating Systems course at Reichman University (graded 100/100). This repository contains the implementation I wrote; course instructions are not included.

## Disk layout

A 10 MB virtual disk, addressed in 4 KB blocks (2560 blocks total):

```
Block 0          Superblock            (global metadata: block/inode counts, free counts)
Block 1          Block bitmap          (1 bit per data block, tracks free/used)
Blocks 2-9       Inode table           (8 blocks = 32 KB → 256 fixed-size inode slots)
Blocks 10-2559   Data blocks           (~9.96 MB, allocated via the bitmap)
```

## What it implements

- **`fs_format` / `fs_mount` / `fs_unmount`** — initializes or loads the on-disk layout (superblock, bitmap, inode table) from a flat file acting as the virtual disk
- **`fs_create` / `fs_delete`** — inode allocation/deallocation, scanning the inode table for a free slot
- **`fs_write` / `fs_read`** — data written/read through direct block pointers only (12 direct blocks per inode, ~48 KB max file size); block allocation goes through the on-disk bitmap, updated and persisted on every write
- **`fs_list`** — enumerates all active (used) inodes

## Design notes

- The on-disk inode is treated as a fixed 128-byte slot, independent of the in-memory `struct inode`'s actual (possibly padded) size — the disk layout is intentionally decoupled from compiler struct layout, so it can't silently drift if the struct changes.
- All persistence goes through explicit block reads/writes to the disk file; there's no in-memory caching of blocks beyond what a single operation needs, so every `fs_write`/`fs_read` is directly verifiable against the on-disk state.
- Filesystem uses **direct block pointers only** — no indirect/double-indirect blocks, which caps individual file size at `12 × 4096` bytes (~48 KB). This was a deliberate scope boundary of the assignment, not an oversight — worth knowing going in if asked about it.

## Building & running

```bash
gcc -std=gnu17 -Wall -I. -o fs_test fs.c tests/test.c
./fs_test
```

The provided test creates a fresh virtual disk, writes and reads back multiple files, and verifies content integrity end-to-end.
