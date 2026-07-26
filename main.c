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

#include "files/system_module.h"
#include "module_loader.h"
// #include "files/stack_fs.h"

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

    fs_stat stat = {};
    system_module *mod = get_module((char**)&path);
    if (mod && mod->getstat) mod->getstat(path, &stat);
    
    if (stat.type == entry_invalid) return 0;
	
	stbuf->st_mode = (stat.type == entry_directory ? S_IFDIR : S_IFREG) | 0666;
	stbuf->st_nlink = 1 + (strlen(path) == 1 && *path == '/');
	stbuf->st_size = stat.size;

	return 0;
}

static int service_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi,
			 enum fuse_readdir_flags flags)
{
	if (strcmp(path, "/") == 0){
	    filler(buf, ".", NULL, 0, 0);
		filler(buf, "..", NULL, 0, 0);
		filler(buf, "clipboard", NULL, 0, 0);
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
    system_module *mod = get_module((char**)&path);
    if (!mod || !mod->read || !mod->open) return -ENOENT;
   
    file fd = {};
    FS_RESULT op = mod->open(path, &fd);
    if (op != FS_RESULT_SUCCESS) return 0;

    fd.cursor = offset;
	size_t res = mod->read(&fd, buf, size, fd.cursor);
   
	if (!res) return 0;

	if (mod->close) mod->close(&fd);
	
	return res;
}

int service_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi){
    system_module *mod = get_module((char**)&path);
    if (!mod || !mod->write || !mod->open) return -ENOENT;
   
    file fd = {};
    FS_RESULT op = mod->open(path, &fd);
    if (op != FS_RESULT_SUCCESS) return 0;

    fd.cursor = offset;
	size_t res = mod->write(&fd, buf, size, fd.cursor);
   
	if (!res) return 0;

	if (mod->close) mod->close(&fd);
	
	return res;
}

const struct fuse_operations dfs_operations = {
	.init       = service_init,
	.getattr	= service_getattr,
	.readdir	= service_readdir,
	.open		= service_open,
	.read		= service_read,
	.write      = service_write,
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