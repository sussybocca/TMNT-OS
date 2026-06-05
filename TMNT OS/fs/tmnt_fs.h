#ifndef TMNT_FS_H
#define TMNT_FS_H

#include "../system/types.h"

#define FS_MAX_FILES 256
#define FS_MAX_FILENAME 64
#define FS_BLOCK_SIZE 512
#define FS_MAX_BLOCKS 1024
#define FS_MAGIC 0x544D4E54

typedef struct {
    char filename[FS_MAX_FILENAME];
    uint32_t size;
    uint32_t first_block;
    uint8_t type;  // 0=file, 1=directory, 2=symlink
    uint32_t created;
    uint32_t modified;
    uint8_t permissions;
} file_inode_t;

typedef struct {
    uint32_t magic;
    uint32_t total_blocks;
    uint32_t free_blocks;
    uint32_t file_count;
    uint32_t root_block;
} fs_superblock_t;

typedef struct {
    int count;
    char names[256][FS_MAX_FILENAME];
    uint32_t sizes[256];
} file_list_t;

void fs_init();
int fs_create_file(const char* path);
int fs_create_dir(const char* path);
int fs_write_file(const char* path, uint8_t* data, uint32_t size);
int fs_read_file(const char* path, uint8_t* buffer, uint32_t size);
uint32_t fs_get_file_size(const char* path);
int fs_file_exists(const char* path);
int fs_delete_file(const char* path);
file_list_t* fs_list_directory(const char* path);
void fs_format();

#endif
