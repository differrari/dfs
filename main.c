#define FUSE_USE_VERSION 31

#define _GNU_SOURCE
#include <sys/stat.h>
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <assert.h>
#include <unistd.h>
#include <limits.h>

#include "files/system_module.h"
#include "module_loader.h"
#include "files/helpers.h"

string_slice fallback_dir = SLICE("/home/di/os_repo/projects/code/braincode");

extern int print(const char *fmt, ...);

static void *service_init(struct fuse_conn_info *conn, struct fuse_config *cfg)
{
	cfg->kernel_cache = 0;
	cfg->direct_io = 1;
	return NULL;
}

static int service_getattr(const char *path, struct stat *stbuf,
			 struct fuse_file_info *fi)
{
	memset(stbuf, 0, sizeof(struct stat));
	
	stbuf->st_uid = getuid();
	stbuf->st_gid = getgid();
	stbuf->st_atime = stbuf->st_mtime = time(NULL);
	
	if (strlen(path) < 1) return false;
	if (strcmp(path,"/") == 0){
    	stbuf->st_mode = S_IFDIR | 0666;
    	stbuf->st_nlink = 2;
    	stbuf->st_size = 0;
	    return 0;
	}

    fs_stat stat_s = {};
	const char *newpath = path;
    system_module *mod = get_module((char**)&newpath);
    if (mod && mod->getstat){
		mod->getstat(newpath, &stat_s);
	} else {
		if (strcmp("build", path)){
			stbuf->st_mode = S_IFREG | 0666;
			stbuf->st_size = 256;
			return 0;
		}
		string fullpath = string_format("%s%s",fallback_dir.data,path);
		int ret = stat(fullpath.data, stbuf);
		string_free(fullpath);
		return ret;
	}
    
    if (stat_s.type == entry_invalid) return 0;
	
	stbuf->st_mode = (stat_s.type == entry_directory ? S_IFDIR : S_IFREG) | 0666;
	stbuf->st_nlink = 1 + (strlen(path) == 1 && *path == '/');
	stbuf->st_size = stat_s.size;

	return 0;
}

fuse_fill_dir_t tmp_filler;
void *tmp_buf = 0;

void traverse_dir_test(const char *directory, const char *file){
	if (strcmp(file, ".") == 0 || strcmp(file, "..") == 0) return;
	tmp_filler(tmp_buf, file, NULL, 0, 0);
}

static int service_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi,
			 enum fuse_readdir_flags flags)
{
	if (strcmp(path, "/") == 0){
	    filler(buf, ".", NULL, 0, 0);
		filler(buf, "..", NULL, 0, 0);
		filler(buf, "clipboard", NULL, 0, 0);
		filler(buf, "build", NULL, 0, 0);
		tmp_filler = filler;
		tmp_buf = buf;
		traverse_directory(fallback_dir.data, false, traverse_dir_test);
		//List contents of modules
	    return 0;
	}
    system_module *mod = get_module((char**)&path);
    if (!mod || !mod->readdir) return -ENOENT;

    size_t listsize = 0x1000;
    void *listptr = zalloc(listsize);
    uint64_t off = 0;
    if (!mod->readdir(path, listptr, listsize, &off)){
        release(listptr);
        return -ENOENT;
    }
    string_list *list = (string_list*)listptr;
    if (list){
        char* reader = (char*)list->array;
        for (uint32_t i = 0; i < list->count; i++){
            char *file = reader;
            if (*file){
                if (filler(buf, file, NULL, 0, 0)){
                    release(listptr);
                    return 0;
                }
                while (*reader) reader++;
                reader++;
            }
        }
    }
    release(listptr);

    return 0;
}

static int service_open(const char *path, struct fuse_file_info *fi)
{
	return 0;
}

int id = 0;

static int service_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
	const char *mpath = path;
    system_module *mod = get_module((char**)&path);
	if (!mod){
		string fullpath = string_format("%s%s",fallback_dir.data,mpath);
		FILE *fd = fopen(fullpath.data, "r");
		string_free(fullpath);
		if (!fd) return -1;
		fseek(fd, offset, SEEK_ABSOLUTE);
		int ret = fread(buf, size, 1, fd);
		// if (ret != 1) return -1;
		size_t size = ftell(fd)-offset;
		fclose(fd);
		return size;
	}
    if (!mod->read || !mod->open) return -ENOENT;
   
    file fd = {};
    FS_RESULT op = mod->open(path, &fd);
    if (op != FS_RESULT_SUCCESS) return 0;

    fd.cursor = offset;
	size_t res = mod->read(&fd, buf, size, fd.cursor);
   
	if (!res) return 0;

	if (mod->close) mod->close(&fd);
	
	return res;
}

extern char *realpath(const char *restrict path, char *restrict resolved_path);

int service_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi){
	const char *mpath = path;
    system_module *mod = get_module((char**)&path);
	if (!mod){
		if (strcmp("build", mpath)){
			string_slice dir = { .data = (char*)buf, .length = size-1};
			char rpathbuf[256];
			string path = string_format("%s/%v",fallback_dir.data,dir);
			size_t wrote = -1;
			if (realpath(path.data, rpathbuf)){
				if (strncmp(rpathbuf, fallback_dir.data, fallback_dir.length) == 0){
					string s = string_format("cd %s && redbuild",rpathbuf);
					print("%S",s);
					system(s.data);
					wrote = size;
				}
			}
			string_free(path);
			return wrote;
		}
		string fullpath = string_format("%s%s",fallback_dir.data,mpath);
		FILE *fd = fopen(fullpath.data, "rw+");
		string_free(fullpath);
		if (!fd) {
			return -1;
		}
		fseek(fd, offset, SEEK_ABSOLUTE);
		int ret = fwrite(buf, size, 1, fd);
		fclose(fd);
		if (ret != 1) {
			return -1;
		}
		return size;
	}
	if (!mod->write || !mod->open) return -ENOENT;
   
    file fd = {};
    FS_RESULT op = mod->open(path, &fd);
    if (op != FS_RESULT_SUCCESS) return 0;

    fd.cursor = offset;
	size_t res = mod->write(&fd, buf, size, fd.cursor);
   
	if (!res) return 0;

	if (mod->close) mod->close(&fd);
	
	return res;
}

int service_truncate(const char *path, off_t offset, struct fuse_file_info *fi){
	const char *mpath = path;
    system_module *mod = get_module((char**)&path);
	if (!mod){
		string fullpath = string_format("%s%s",fallback_dir.data,mpath);
		FILE *fd = fopen(fullpath.data, "rw+");
		string_free(fullpath);
		if (!fd) return -1;
		ftruncate(fd->_fileno, offset);
		fclose(fd);
		return offset;
	}
	if (!mod->truncate) return -1;

	file fd = {};
    FS_RESULT op = mod->open(path, &fd);
    if (op != FS_RESULT_SUCCESS) return 0;

    fd.size = offset;
	return mod->truncate(&fd);
}

const struct fuse_operations dfs_operations = {
	.init       = service_init,
	.getattr	= service_getattr,
	.readdir	= service_readdir,
	.open		= service_open,
	.read		= service_read,
	.write      = service_write,
	.truncate 	= service_truncate,
};

static const struct fuse_opt option_spec[] = {
	FUSE_OPT_END
};

extern const struct fuse_operations dfs_operations;

#include "files/stack_fs.h"

system_module clipboard_mod = {
    .name = "clipboard",
    .mount = "clipboard",
    .version = VERSION_NUM(0, 1, 0, 0),
    .init = stackfs_init,
    .open = stackfs_open,
    .read = stackfs_read,
    .write = stackfs_write,
    .readdir = stackfs_readdir,
    .getstat = stackfs_stat
};

int main(int argc, char *argv[])
{
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

	if (fuse_opt_parse(&args, 0, option_spec, NULL) == -1)
		return 1;
	
	fuse_opt_add_arg(&args, "/home/di/shared");
	fuse_opt_add_arg(&args, "-f");
	
	load_module(&clipboard_mod);

	service_write("/clipboard","hello",6,0,0);
	
	int ret = fuse_main(args.argc, args.argv, &dfs_operations, NULL);
	fuse_opt_free_args(&args);
	return ret;
}