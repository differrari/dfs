#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <assert.h>
#include "files/buffer.h"
#include "syscalls/syscalls.h"
#include <unistd.h>
#include "memory.h"
#include "mail.h"
#include "files/system_module.h"

static const struct fuse_opt option_spec[] = {
	FUSE_OPT_END
};

static void *service_init(struct fuse_conn_info *conn,
			struct fuse_config *cfg)
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
	fs_stat stat = {};
	if (mail_module.get_stat) mail_module.get_stat(path+1, &stat);
	
	if (stat.type == entry_invalid) return -ENOENT;
	
	stbuf->st_mode = (stat.type == entry_directory ? S_IFDIR : S_IFREG) | 0666;
	stbuf->st_nlink = 1 + (strlen(path) == 1 && *path == '/');
	stbuf->st_size = stat.size;

	return 0;
}

extern fuse_fill_dir_t temp_filler;

static int service_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi,
			 enum fuse_readdir_flags flags)
{
    if (!mail_module.sread) return 0;
	if (strcmp(path, "/") != 0)
		return -ENOENT;

	temp_filler = filler;
	u64 off = offset;
	return mail_module.sread(path+1, buf, 0, &off);
}

static int service_open(const char *path, struct fuse_file_info *fi)
{
	return 0;
}

int id = 0;

static int service_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
    if (!mail_module.sread) return 0;
    u64 off = offset;
	return mail_module.sread(path+1, buf, size, &off);
}

int service_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi){
    if (!mail_module.swrite) return 0;
    return mail_module.swrite(path+1, buf, size);
}

static const struct fuse_operations hello_oper = {
	.init       = service_init,
	.getattr	= service_getattr,
	.readdir	= service_readdir,
	.open		= service_open,
	.read		= service_read,
	.write      = service_write,
};

int main(int argc, char *argv[])
{
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

	if (fuse_opt_parse(&args, 0, option_spec, NULL) == -1)
		return 1;
	
	fuse_opt_add_arg(&args, "/home/di/shared");
	fuse_opt_add_arg(&args, "-f");
	
	if (mail_module.init) mail_module.init();
	
	int ret = fuse_main(args.argc, args.argv, &hello_oper, NULL);
	fuse_opt_free_args(&args);
	return ret;
}