// TMNT OS Custom File System - Enhanced with Linux-inspired inode management
#include "tmnt_fs.h"
#include "../system/memory.h"
#include "../system/string.h"
#include "../external/music_data.h"

#define FS_MEMORY_START 0x200000
#define FS_MEMORY_SIZE 0x6400000
#define INODE_HASH_SIZE 64

static fs_superblock_t* superblock;
static file_inode_t* inodes;
static uint8_t* data_blocks;
static file_inode_t* inode_hash[INODE_HASH_SIZE];

static inline void outb(uint16_t port, uint8_t value) {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static int find_inode(const char* path);
static const char* get_filename(const char* path);
static void get_directory_path(const char* path, char* dir);
static uint32_t get_system_time(void);

static uint32_t read_rtc_time(void) {
    uint16_t year, month, day, hour, minute, second;
    
    outb(0x70, 0x00); second = inb(0x71);
    outb(0x70, 0x02); minute = inb(0x71);
    outb(0x70, 0x04); hour = inb(0x71);
    outb(0x70, 0x07); day = inb(0x71);
    outb(0x70, 0x08); month = inb(0x71);
    outb(0x70, 0x09); year = inb(0x71);
    
    second = (second & 0x0F) + ((second >> 4) * 10);
    minute = (minute & 0x0F) + ((minute >> 4) * 10);
    hour = (hour & 0x0F) + ((hour >> 4) * 10);
    day = (day & 0x0F) + ((day >> 4) * 10);
    month = (month & 0x0F) + ((month >> 4) * 10);
    year = (year & 0x0F) + ((year >> 4) * 10) + 2000;
    
    uint32_t timestamp = second + minute * 60 + hour * 3600 + day * 86400;
    timestamp += (month - 1) * 2678400;
    timestamp += (year - 1970) * 31536000;
    
    return timestamp;
}

/* djb2 hash function for inode lookup */
static uint32_t fs_hash_index(const char* path) {
    uint32_t hash = 5381;
    int c;
    while((c = *path++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash % INODE_HASH_SIZE;
}

/* Simple password hash (djb2 variant) */
void fs_hash_password(const char* password, char* hash_out) {
    uint32_t hash = 5381;
    int c;
    while((c = *password++)) {
        hash = ((hash << 5) + hash) + c;
    }
    for(int i = 0; i < 8; i++) {
        uint8_t nibble = (hash >> (28 - i * 4)) & 0xF;
        hash_out[i] = (nibble < 10) ? '0' + nibble : 'a' + (nibble - 10);
    }
    hash_out[8] = '\0';
}

void fs_hash_init(void) {
    for(int i = 0; i < INODE_HASH_SIZE; i++) {
        inode_hash[i] = NULL;
    }
}

int fs_hash_insert(file_inode_t* inode) {
    if(!inode || inode->filename[0] == 0) return -1;
    inode->i_state &= ~I_NEW;
    return 0;
}

file_inode_t* fs_hash_lookup(const char* path) {
    int idx = find_inode(path);
    if(idx >= 0) return &inodes[idx];
    return NULL;
}

file_inode_t* fs_iget(const char* path) {
    file_inode_t* inode = fs_hash_lookup(path);
    if(inode) {
        inode->i_count++;
    }
    return inode;
}

void fs_iput(file_inode_t* inode) {
    if(!inode) return;
    if(inode->i_count > 0) {
        inode->i_count--;
    }
    if(inode->i_count == 0 && (inode->i_state & I_FREEING)) {
        memset(inode, 0, sizeof(file_inode_t));
    }
}

int fs_ilookup(const char* path) {
    return find_inode(path);
}

void fs_mark_inode_dirty(file_inode_t* inode) {
    if(inode) {
        inode->i_state |= I_DIRTY;
        inode->modified = get_system_time();
    }
}

/* XOR Encryption */
int fs_encrypt_file(const char* path, uint8_t key) {
    int idx = find_inode(path);
    if(idx < 0) return -1;
    
    file_inode_t* ino = &inodes[idx];
    if(ino->size == 0) return -1;
    
    uint8_t* buf = (uint8_t*)malloc(ino->size);
    if(!buf) return -1;
    
    fs_read_file(path, buf, ino->size);
    
    for(uint32_t i = 0; i < ino->size; i++) {
        buf[i] ^= key;
    }
    
    uint32_t block_offset = ino->first_block;
    uint32_t blocks = (ino->size + FS_BLOCK_SIZE - 1) / FS_BLOCK_SIZE;
    for(uint32_t i = 0; i < blocks; i++) {
        uint32_t bytes_to_write = FS_BLOCK_SIZE;
        if(i == blocks - 1 && (ino->size % FS_BLOCK_SIZE) != 0) {
            bytes_to_write = ino->size % FS_BLOCK_SIZE;
        }
        memcpy(&data_blocks[(block_offset + i) * FS_BLOCK_SIZE], 
               &buf[i * FS_BLOCK_SIZE], bytes_to_write);
    }
    
    ino->xor_key = key;
    ino->i_state |= I_ENCRYPTED;
    ino->i_state |= I_DIRTY;
    
    free(buf);
    return 0;
}

int fs_decrypt_file(const char* path, uint8_t key) {
    return fs_encrypt_file(path, key);
}

int fs_is_encrypted(const char* path) {
    int idx = find_inode(path);
    if(idx < 0) return 0;
    return (inodes[idx].i_state & I_ENCRYPTED) ? 1 : 0;
}

/* Password protection */
int fs_lock_file(const char* path, const char* password) {
    int idx = find_inode(path);
    if(idx < 0) return -1;
    
    char hash[FS_PASSWD_LEN];
    fs_hash_password(password, hash);
    memcpy(inodes[idx].password, hash, FS_PASSWD_LEN);
    inodes[idx].i_state |= I_LOCKED;
    inodes[idx].i_state |= I_DIRTY;
    
    return 0;
}

int fs_unlock_file(const char* path, const char* password) {
    int idx = find_inode(path);
    if(idx < 0) return -1;
    
    if(!(inodes[idx].i_state & I_LOCKED)) return 0;
    
    char hash[FS_PASSWD_LEN];
    fs_hash_password(password, hash);
    
    if(strcmp(inodes[idx].password, hash) == 0) {
        inodes[idx].i_state &= ~I_LOCKED;
        memset(inodes[idx].password, 0, FS_PASSWD_LEN);
        return 0;
    }
    return -1;
}

int fs_is_locked(const char* path) {
    int idx = find_inode(path);
    if(idx < 0) return 0;
    return (inodes[idx].i_state & I_LOCKED) ? 1 : 0;
}

/* File customization */
int fs_rename_file(const char* old_path, const char* new_path) {
    int idx = find_inode(old_path);
    if(idx < 0) return -1;
    if(fs_file_exists(new_path)) return -1;
    
    int i;
    for(i = 0; new_path[i] && i < FS_MAX_FILENAME - 1; i++) {
        inodes[idx].filename[i] = new_path[i];
    }
    inodes[idx].filename[i] = '\0';
    inodes[idx].i_state |= I_DIRTY;
    
    return 0;
}

int fs_set_display_name(const char* path, const char* display_name) {
    int idx = find_inode(path);
    if(idx < 0) return -1;
    
    int i;
    for(i = 0; display_name[i] && i < FS_MAX_FILENAME - 1; i++) {
        inodes[idx].display_name[i] = display_name[i];
    }
    inodes[idx].display_name[i] = '\0';
    inodes[idx].i_state |= I_DIRTY;
    
    return 0;
}

int fs_set_display_color(const char* path, uint32_t color) {
    int idx = find_inode(path);
    if(idx < 0) return -1;
    
    inodes[idx].display_color = color;
    inodes[idx].i_state |= I_DIRTY;
    
    return 0;
}

const char* fs_get_display_name(const char* path) {
    int idx = find_inode(path);
    if(idx < 0) return NULL;
    
    if(inodes[idx].display_name[0] != '\0') {
        return inodes[idx].display_name;
    }
    return NULL;
}

uint32_t fs_get_display_color(const char* path) {
    int idx = find_inode(path);
    if(idx < 0) return 0;
    return inodes[idx].display_color;
}

/* ============ ORIGINAL FILESYSTEM CODE BELOW ============ */

void fs_init() {
    superblock = (fs_superblock_t*)FS_MEMORY_START;
    inodes = (file_inode_t*)(FS_MEMORY_START + sizeof(fs_superblock_t));
    data_blocks = (uint8_t*)(FS_MEMORY_START + sizeof(fs_superblock_t) + 
                              sizeof(file_inode_t) * FS_MAX_FILES);
    
    fs_hash_init();
    
    if(superblock->magic != FS_MAGIC) {
        fs_format();
    }
    
    fs_create_dir("/system");
    fs_create_dir("/apps");
    fs_create_dir("/user");
    fs_create_dir("/user/music");
    
    if(tmnt_theme_raw_len > 0 && tmnt_theme_raw_len < (FS_MAX_BLOCKS * FS_BLOCK_SIZE)) {
        fs_write_file("/user/music/tmnt_theme.raw", (uint8_t*)tmnt_theme_raw, tmnt_theme_raw_len);
    }
}

void fs_format() {
    superblock->magic = FS_MAGIC;
    superblock->total_blocks = FS_MAX_BLOCKS;
    superblock->free_blocks = FS_MAX_BLOCKS;
    superblock->file_count = 0;
    superblock->root_block = 0;
    superblock->next_ino = 1;
    
    memset(inodes, 0, sizeof(file_inode_t) * FS_MAX_FILES);
    memset(data_blocks, 0, FS_BLOCK_SIZE * FS_MAX_BLOCKS);
    fs_hash_init();
}

int fs_create_file(const char* path) {
    if(superblock->file_count >= FS_MAX_FILES || superblock->free_blocks == 0) {
        return -1;
    }
    
    int inode_idx = -1;
    for(int i = 0; i < FS_MAX_FILES; i++) {
        if(inodes[i].filename[0] == 0) {
            inode_idx = i;
            break;
        }
    }
    
    if(inode_idx == -1) return -1;
    
    int i;
    for(i = 0; path[i] && i < FS_MAX_FILENAME - 1; i++) {
        inodes[inode_idx].filename[i] = path[i];
    }
    inodes[inode_idx].filename[i] = '\0';
    
    inodes[inode_idx].size = 0;
    inodes[inode_idx].first_block = superblock->root_block;
    inodes[inode_idx].type = 0;
    inodes[inode_idx].created = get_system_time();
    inodes[inode_idx].modified = inodes[inode_idx].created;
    inodes[inode_idx].permissions = 7;
    inodes[inode_idx].i_count = 1;
    inodes[inode_idx].i_state = I_NEW;
    inodes[inode_idx].i_ino = superblock->next_ino++;
    inodes[inode_idx].display_color = 0xFFFFFF;
    
    superblock->file_count++;
    inodes[inode_idx].i_state &= ~I_NEW;
    
    return inode_idx;
}

int fs_create_dir(const char* path) {
    if(!fs_file_exists(path)) {
        int idx = fs_create_file(path);
        if(idx >= 0) {
            inodes[idx].type = 1;
            inodes[idx].size = 0;
        }
        return idx;
    }
    return -1;
}

int fs_write_file(const char* path, uint8_t* data, uint32_t size) {
    int inode_idx = find_inode(path);
    if(inode_idx == -1) {
        inode_idx = fs_create_file(path);
        if(inode_idx == -1) return -1;
    }
    
    uint32_t blocks_needed = (size + FS_BLOCK_SIZE - 1) / FS_BLOCK_SIZE;
    if(blocks_needed > superblock->free_blocks) return -1;
    
    uint32_t block_offset = superblock->total_blocks - superblock->free_blocks;
    for(uint32_t i = 0; i < blocks_needed; i++) {
        uint32_t bytes_to_write = (i == blocks_needed - 1) ? 
                                  (size % FS_BLOCK_SIZE == 0 ? FS_BLOCK_SIZE : size % FS_BLOCK_SIZE) : 
                                  FS_BLOCK_SIZE;
        
        memcpy(&data_blocks[(block_offset + i) * FS_BLOCK_SIZE], 
               &data[i * FS_BLOCK_SIZE], bytes_to_write);
    }
    
    inodes[inode_idx].size = size;
    inodes[inode_idx].first_block = block_offset;
    inodes[inode_idx].modified = get_system_time();
    inodes[inode_idx].i_state |= I_DIRTY;
    superblock->free_blocks -= blocks_needed;
    
    return 0;
}

int fs_read_file(const char* path, uint8_t* buffer, uint32_t size) {
    int inode_idx = find_inode(path);
    if(inode_idx == -1) return -1;
    
    if(inodes[inode_idx].i_state & I_LOCKED) return -2;
    
    inodes[inode_idx].i_state |= I_REFERENCED;
    
    uint32_t read_size = (size < inodes[inode_idx].size) ? size : inodes[inode_idx].size;
    uint32_t blocks = (inodes[inode_idx].size + FS_BLOCK_SIZE - 1) / FS_BLOCK_SIZE;
    
    for(uint32_t i = 0; i < blocks && i * FS_BLOCK_SIZE < read_size; i++) {
        uint32_t block_addr = (inodes[inode_idx].first_block + i) * FS_BLOCK_SIZE;
        uint32_t bytes_to_read = FS_BLOCK_SIZE;
        if(i == blocks - 1) {
            bytes_to_read = inodes[inode_idx].size % FS_BLOCK_SIZE;
            if(bytes_to_read == 0) bytes_to_read = FS_BLOCK_SIZE;
        }
        memcpy(&buffer[i * FS_BLOCK_SIZE], &data_blocks[block_addr], bytes_to_read);
    }
    
    if(inodes[inode_idx].i_state & I_ENCRYPTED) {
        for(uint32_t i = 0; i < read_size; i++) {
            buffer[i] ^= inodes[inode_idx].xor_key;
        }
    }
    
    return read_size;
}

uint32_t fs_get_file_size(const char* path) {
    int inode_idx = find_inode(path);
    return (inode_idx == -1) ? 0 : inodes[inode_idx].size;
}

int fs_file_exists(const char* path) {
    return find_inode(path) != -1;
}

int fs_delete_file(const char* path) {
    int inode_idx = find_inode(path);
    if(inode_idx == -1) return -1;
    
    inodes[inode_idx].i_state |= I_FREEING;
    
    uint32_t blocks_used = (inodes[inode_idx].size + FS_BLOCK_SIZE - 1) / FS_BLOCK_SIZE;
    if(inodes[inode_idx].size == 0) blocks_used = 0;
    
    superblock->free_blocks += blocks_used;
    superblock->file_count--;
    
    memset(&inodes[inode_idx], 0, sizeof(file_inode_t));
    return 0;
}

file_list_t* fs_list_directory(const char* path) {
    static file_list_t list;
    list.count = 0;
    
    for(int i = 0; i < FS_MAX_FILES; i++) {
        if(inodes[i].filename[0] != 0 && !(inodes[i].i_state & I_FREEING)) {
            char dir_path[FS_MAX_FILENAME];
            get_directory_path(inodes[i].filename, dir_path);
            
            if(strcmp(dir_path, path) == 0 || 
               (strcmp(path, "/") == 0 && strchr(inodes[i].filename + 1, '/') == 0)) {
                const char* fname = get_filename(inodes[i].filename);
                strcpy(list.names[list.count], fname);
                list.sizes[list.count] = inodes[i].size;
                list.count++;
                if(list.count >= 256) break;
            }
        }
    }
    
    return &list;
}

static int find_inode(const char* path) {
    for(int i = 0; i < FS_MAX_FILES; i++) {
        if(inodes[i].filename[0] != 0 && 
           !(inodes[i].i_state & I_FREEING) &&
           strcmp(inodes[i].filename, path) == 0) {
            return i;
        }
    }
    return -1;
}

static const char* get_filename(const char* path) {
    const char* last = strrchr(path, '/');
    return last ? last + 1 : path;
}

static void get_directory_path(const char* path, char* dir) {
    strcpy(dir, path);
    char* last = strrchr(dir, '/');
    if(last) {
        if(last == dir) {
            dir[1] = '\0';
        } else {
            *last = '\0';
        }
    } else {
        strcpy(dir, "/");
    }
}

static uint32_t get_system_time(void) {
    return read_rtc_time();
}