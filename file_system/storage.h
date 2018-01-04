#ifndef NUFS_STORAGE_H
#define NUFS_STORAGE_H

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fuse.h>

#define FUSE_USE_VERSION 26

#define DIR_FLAG 0
#define FILE_FLAG 1

#define DELETED 0
#define ACTIVE 1

#define BLK_SIZE 4096
#define BLOCKS_PER_INODE 10

#define NAME_LENGTH (64 - sizeof(int) - sizeof(char))

#define min(a, b) (a <= b ? a : b)
#define max(a, b) (a >= b? a : b)

typedef struct inode {
	dev_t dev;
	mode_t mode;
	int nlinks;
	uid_t uid;
	gid_t gid;
	dev_t rdev;
	off_t size;
	blkcnt_t num_blocks;
	time_t atime;
	time_t mtime;
	time_t ctime;
	char flag;
	int blocks[BLOCKS_PER_INODE];
	int indir_block;
} inode;

typedef struct entry {
	char file_name[NAME_LENGTH];
	int inum; // index of inode
	char flag;
} entry;

void storage_init(const char *path);
int         get_stat(const char *path, struct stat *st);
int storage_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		    off_t offset, struct fuse_file_info *fi);
int storage_mknod(char const *path, mode_t mode, dev_t rdev);
int storage_set_time(const char *path, const struct timespec ts[2]);
int storage_unlink(const char *path);
int storage_chmod(const char *path, mode_t mode);
int storage_rename(const char *from, const char *to);
int storage_truncate(const char *path, off_t size);
int storage_write(const char *path, const char *buf, size_t size, off_t offset,
		  struct fuse_file_info *fi);
int storage_read(const char *path, char *buf, size_t size, off_t offset,
		 struct fuse_file_info *fi);
int storage_link(const char *from, const char *to);
int storage_mkdir(const char *path, mode_t mode);
int storage_rmdir(const char *path);
int storage_access(const char *path, int mask);

#endif