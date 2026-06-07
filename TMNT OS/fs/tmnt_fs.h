#ifndef TMNT_FS_H
#define TMNT_FS_H

#include "../system/types.h"

#define FS_MAX_FILES 256
#define FS_MAX_FILENAME 64
#define FS_BLOCK_SIZE 512
#define FS_MAX_BLOCKS 204800
#define FS_MAGIC 0x544D4E54

/* Inode state flags - inspired by Linux */
#define I_NEW        0x01
#define I_DIRTY      0x02
#define I_FREEING    0x04
#define I_REFERENCED 0x08
#define I_ENCRYPTED  0x10
#define I_LOCKED     0x20

#define FS_PASSWD_LEN 32

typedef struct {
    char filename[FS_MAX_FILENAME];
    uint32_t size;
    uint32_t first_block;
    uint8_t type;
    uint32_t created;
    uint32_t modified;
    uint8_t permissions;
    uint32_t i_count;
    uint32_t i_state;
    uint32_t i_ino;
    char password[FS_PASSWD_LEN];
    uint8_t xor_key;
    char display_name[FS_MAX_FILENAME];
    uint32_t display_color;
} file_inode_t;

typedef struct {
    uint32_t magic;
    uint32_t total_blocks;
    uint32_t free_blocks;
    uint32_t file_count;
    uint32_t root_block;
    uint32_t next_ino;
} fs_superblock_t;

typedef struct {
    int count;
    char names[256][FS_MAX_FILENAME];
    uint32_t sizes[256];
} file_list_t;

void fs_init();
void fs_format();
int fs_create_file(const char* path);
int fs_create_dir(const char* path);
int fs_write_file(const char* path, uint8_t* data, uint32_t size);
int fs_read_file(const char* path, uint8_t* buffer, uint32_t size);
int fs_read_file_chunk(const char* path, uint8_t* buffer, uint32_t size, uint32_t offset);
uint32_t fs_get_file_size(const char* path);
int fs_file_exists(const char* path);
int fs_delete_file(const char* path);
file_list_t* fs_list_directory(const char* path);
file_inode_t* fs_iget(const char* path);
void fs_iput(file_inode_t* inode);
int fs_ilookup(const char* path);
void fs_mark_inode_dirty(file_inode_t* inode);
void fs_hash_init(void);
int fs_hash_insert(file_inode_t* inode);
int fs_hash_remove(file_inode_t* inode);
file_inode_t* fs_hash_lookup(const char* path);
int fs_encrypt_file(const char* path, uint8_t key);
int fs_decrypt_file(const char* path, uint8_t key);
int fs_is_encrypted(const char* path);
int fs_lock_file(const char* path, const char* password);
int fs_unlock_file(const char* path, const char* password);
int fs_is_locked(const char* path);
int fs_rename_file(const char* old_path, const char* new_path);
int fs_set_display_name(const char* path, const char* display_name);
int fs_set_display_color(const char* path, uint32_t color);
const char* fs_get_display_name(const char* path);
uint32_t fs_get_display_color(const char* path);
void fs_hash_password(const char* password, char* hash_out);

#endif