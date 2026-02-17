#include "files/system_module.h"
#include "string/string.h"
#include "files/buffer.h"
#include "syscalls/syscalls.h"

#define MAX_ENTRIES 16
fs_entry entries[MAX_ENTRIES];
int entry_count = 0;

buffer in_buffer;
buffer out_buffer;

static inline bool make_entry(const char *name, fs_backing_type back_type, fs_entry_type ent_type, getstat stat_function, sread read_func, swrite write_func){
    if (entry_count >= MAX_ENTRIES-1) return false;
    entries[entry_count++] = (fs_entry){
        .name = name,
        .backing_type = back_type,
        .entry_type = ent_type,
        .stat_func = stat_function,
        .read_func = read_func,
        .write_func = write_func
    };
    return true;
}

static inline fs_entry eval_entry(const char *path){
    for (int i = 0; i < entry_count; i++){
        if (strcmp(path, entries[i].name) == 0) return entries[i];
    }
    return (fs_entry){};
}

bool root_stat(const char *path, fs_stat *stat){
    if (!stat) return false;
    stat->size = 0;
    stat->type = entry_directory;
    return true;
}

#define FUSE_USE_VERSION 31

#include <fuse.h>

fuse_fill_dir_t temp_filler;

size_t list_root(const char *path, void* buf, size_t size, file_offset* off){
	temp_filler(buf, "..", NULL, 0, 0);
    for (int i = 0; i < entry_count; i++){
	    temp_filler(buf, strlen(entries[i].name) ? entries[i].name : ".", NULL, 0, 0);
	}
	return 0;
}

bool in_stat(const char *path, fs_stat *stat){
    if (!stat) return false;
    stat->size = in_buffer.buffer_size;
    stat->type = entry_file;
    return true;
}

size_t in_read(const char *path, void* buf, size_t size, file_offset* off){
    size_t amount = buffer_read(&in_buffer, buf, size, *off);
    *off += amount;
    return amount;
}

size_t in_write(const char *path, const void* buf, size_t size){
    return buffer_write_lim(&in_buffer, buf, size);
}

bool out_stat(const char *path, fs_stat *stat){
    if (!stat) return false;
    stat->size = out_buffer.buffer_size;
    stat->type = entry_file;
    return true;
}

size_t out_read(const char *path, void* buf, size_t size, file_offset* off){
    size_t amount = buffer_read(&out_buffer, buf, size, *off);
    print("Read %i from %i (expected %i, file size %i)",amount,*off,size,out_buffer.buffer_size);
    *off += amount;
    return amount;
}

size_t out_write(const char *path, const void* buf, size_t size){
    size_t amount = buffer_write_lim(&out_buffer, buf, size);
    return amount;
}

bool mail_getstat(const char *path, fs_stat *stat){
    fs_entry entry = eval_entry(path);
    if (!entry.entry_type){ print("No file %s",path); return false;}
    if (entry.stat_func){ return entry.stat_func(path, stat); }
    return false;
}

size_t mail_sread(const char *path, void *buf, size_t size, file_offset *offset){
    fs_entry entry = eval_entry(path);
    if (!entry.entry_type){ print("No file %s",path);return false;}
    if (entry.read_func) return entry.read_func(path, buf, size, offset);
    return false;
}

size_t mail_swrite(const char *path, const void *buf, size_t size){
    fs_entry entry = eval_entry(path);
    if (!entry.entry_type){ print("No file %s",path);return false;}
    if (entry.write_func) return entry.write_func(path, buf, size);
    return false;
}

bool load_mail(){
    in_buffer = buffer_create(256, buffer_circular);
	out_buffer = buffer_create(256, buffer_circular);
    make_entry("", backing_virtual, entry_directory, root_stat, list_root, 0);
	make_entry("in", backing_virtual, entry_directory, in_stat, in_read, in_write);
	make_entry("out", backing_virtual, entry_directory, out_stat, out_read, out_write);
    return true;
}

system_module mail_module = {
    .name = "mail",
    .mount = "/mail",
    .init = load_mail,
    .readdir = mail_sread,
    .sread = mail_sread,
    .swrite = mail_swrite,
    .get_stat = mail_getstat,
    .version = VERSION_NUM(0, 1, 0, 0),
};