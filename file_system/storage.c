#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <assert.h>
#include <time.h>
#include <stdlib.h>
#include <errno.h>

#include "storage.h"
#include "slist.h"

#define NUM_INODES 64
#define NUM_BLOCKS 250
#define INODE_BM_OFFSET 0
#define BLOCK_BM_OFFSET (INODE_BM_OFFSET + sizeof(char) * NUM_INODES)
#define INODE_OFFSET (BLOCK_BM_OFFSET + sizeof(char) * NUM_BLOCKS)
#define BLOCKS_OFFSET (INODE_OFFSET + sizeof(inode) * NUM_INODES)

#define ROOT_INUM 0

#define FREE 0
#define OCCU 1

/**
 * inum is referred to the inode number, the index of an inode in the inode
 * array.
 * dnum is referred to the data block number, the index of a data block in
 * the whole data block array (not the array in the inode).
 */

const int NUFS_SIZE = 1024 * 1024;

static int page_fd = -1;
static void *page_base;

static inode *root;
static entry root_entry = {"", ROOT_INUM};

/**
 * Gets the inode pointer at the given index.
 */
inode *get_inode_ptr(int inum)
{
	assert(inum >= 0 && inum < NUM_INODES);
	return page_base + INODE_OFFSET + sizeof(inode) * inum;
}

/**
 * Gets the start of the data block of the given index.
 */
void *get_block_ptr(int dnum)
{
	return page_base + BLOCKS_OFFSET + BLK_SIZE * dnum;
}

/**
 * Gets the pointer to the given index of the inode bitmap.
 */
char *get_inode_bm(int inum)
{
	return page_base + INODE_BM_OFFSET + inum;
}

/**
 * Gets the pointer to the given index of the data blocks bitmap.
 */
char *get_blocks_bm(int dnum)
{
	return page_base + BLOCK_BM_OFFSET + dnum;
}

void set_inode_bm(int inum, char set_to)
{
	char *ptr = get_inode_bm(inum);
	*ptr = set_to;
}

void set_block_bm(int dnum, char set_to)
{
	char *ptr = get_blocks_bm(dnum);
	*ptr = set_to;
}

int get_dnum(int inum, int blk_idx)
{
	inode *node = get_inode_ptr(inum);

	assert(blk_idx >= 0 && blk_idx < node->num_blocks);
	if (blk_idx < BLOCKS_PER_INODE) {
		return node->blocks[blk_idx];
	}

	int *indir_blocks = get_block_ptr(node->indir_block);
	return indir_blocks[blk_idx - BLOCKS_PER_INODE];
}

/**
 * Returns the total number of entries (ACTIVE and DELETED).
 */
int get_num_entries(int inum)
{
	inode *node = get_inode_ptr(inum);

	assert(node->flag == DIR_FLAG && node->size % sizeof(entry) == 0);
	return node->size / sizeof(entry);
}

/**
 * Returns an inum. Flips the inum to OCCU.
 * Returns -1 if there's no free inode.
 */
int assign_free_inode()
{
	for (int i = 1; i < NUM_INODES; i++) {
		char *ptr = get_inode_bm(i);
		if (*ptr == FREE) {
			set_inode_bm(i, OCCU);
			inode *node = get_inode_ptr(i);
			memset(node, 0, sizeof(inode));
			return i;
		}
	}

	return -1;
}

/**
 * Returns a dnum. Flips the dnum to OCCU.
 * Returns -1 if there's no free block.
 */
int assign_free_block()
{
	for (int i = 1; i < NUM_BLOCKS; i++) {
		char *ptr = get_blocks_bm(i);
		if (*ptr == FREE) {
			set_block_bm(i, OCCU);
			return i;
		}
	}

	return -1;
}

/**
 * Adds a new block to the inode.
 */
int assign_block_to_inode(int inum)
{
	inode *node = get_inode_ptr(inum);
	assert(node != NULL);
	if (node->num_blocks == BLOCKS_PER_INODE + BLK_SIZE / sizeof(int)) {
		return -ENOMEM;
	}

	int rv;
	if (node->num_blocks < BLOCKS_PER_INODE) {
		rv = assign_free_block();
		if (rv == -1) {
			return -ENOMEM;
		}
		node->blocks[node->num_blocks] = rv;
		node->num_blocks++;
		return rv;
	}

	if (node->num_blocks == BLOCKS_PER_INODE) {
		rv = assign_free_block();
		if (rv == -1) {
			return -ENOMEM;
		}

		node->indir_block = rv;
	}

	int *dnums = get_block_ptr(node->indir_block);
	rv = assign_free_block();
	if (rv == -1) {
		return -ENOMEM;
	}

	dnums[node->num_blocks - BLOCKS_PER_INODE] = rv;
	node->num_blocks++;
	return rv;
}

/**
 * Gets the entry at the given entry number from this inode.
 */
entry *get_entry(int inum, int entry_num)
{
	inode *node = get_inode_ptr(inum);
	assert(node->size % sizeof(entry) == 0
	       && node->size >= entry_num * sizeof(entry));
	assert(entry_num >= 0 && entry_num < get_num_entries(inum));

	off_t entry_offset = entry_num * sizeof(entry);
	void *block = get_block_ptr(get_dnum(inum, entry_offset / BLK_SIZE));

	return block + entry_offset % BLK_SIZE;
}

void mem_init(const char *path)
{
	page_fd = open(path, O_CREAT | O_RDWR, 0644);
	assert(page_fd != -1);

	int rv = ftruncate(page_fd, NUFS_SIZE);
	assert(rv == 0);

	page_base = mmap(0, NUFS_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,
			 page_fd, 0);
	assert(page_base != MAP_FAILED);
}

void init_root_inode()
{
	root = get_inode_ptr(0);
	set_inode_bm(0, OCCU);
	root->mode = 040755;
	root->nlinks = 1;
	root->uid = getuid();
	time_t now = time(0);
	root->atime = now;
	root->ctime = now;
	root->mtime = now;
	root->flag = DIR_FLAG;
}

void storage_init(const char *path)
{
	mem_init(path);
	init_root_inode();
}

void update_mctime(int inum)
{
	inode *node = get_inode_ptr(inum);
	time_t now = time(0);
	node->mtime = now;
	node->ctime = now;
}

void update_atime(int inum)
{
	inode *node = get_inode_ptr(inum);
	time_t now = time(0);
	node->atime = now;
}

static int streq(const char *aa, const char *bb)
{
	return strcmp(aa, bb) == 0;
}

static int div_up(int xx, int yy)
{
	int ret_val = xx / yy;

	if (xx % yy != 0) {
		ret_val++;
	}

	return ret_val;
}

/**
 * Gets the file entry based on the given list of directories.
 */
static entry *get_entry_recursive(slist *parts, entry *curr_entry)
{
	if (parts == NULL) {
		return curr_entry;
	}

	if (streq(parts->data, "")) {
		return get_entry_recursive(parts->next, &root_entry);
	}

	assert(curr_entry != NULL);
	int inum = curr_entry->inum;
	int total_entries = get_num_entries(inum);
	for (int i = 0; i < total_entries; i++) {
		entry *file_entry = get_entry(inum, i);
		if (file_entry->flag == ACTIVE
		    && streq(file_entry->file_name, parts->data)) {
			return get_entry_recursive(parts->next, file_entry);
		}
	}

	return NULL;
}

static entry *get_file_entry(const char *path) {
	slist *parts = s_split(path, '/');
	entry *rv = get_entry_recursive(parts, NULL);
	s_free(parts);
	return rv;
}

int entry_get_stat(entry *dat, struct stat *st)
{
	memset(st, 0, sizeof(struct stat));
	inode *node = get_inode_ptr(dat->inum);

	st->st_dev = node->dev;
	st->st_ino = dat->inum;
	st->st_mode = node->mode;
	st->st_nlink = node->nlinks;
	st->st_uid = node->uid;
	st->st_gid = node->gid;
	st->st_rdev = node->rdev;
	st->st_size = node->size;
	st->st_blksize = BLK_SIZE;
	st->st_blocks = node->num_blocks;
	st->st_atime = node->atime;
	st->st_mtime = node->mtime;
	st->st_ctime = node->ctime;

	return 0;
}

int get_stat(const char *path, struct stat *st)
{
	entry *dat = get_file_entry(path);
	if (!dat) {
		return -1;
	}

	return entry_get_stat(dat, st);
}

int storage_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		    off_t offset, struct fuse_file_info *fi)
{
	struct stat st;
	entry *dat = get_file_entry(path);
	entry_get_stat(dat, &st);
	filler(buf, ".", &st, 0);

	int total_entries = get_num_entries(dat->inum);
	for (int i = 0; i < total_entries; i++) {
		entry *curr_entry = get_entry(dat->inum, i);
		if (curr_entry->flag == ACTIVE) {
			entry_get_stat(curr_entry, &st);
			filler(buf, curr_entry->file_name, &st, 0);
		}
	}

	update_atime(dat->inum);
	return 0;
}

int last_slash_idx(const char *path)
{
	int str_len = strlen(path);
	int last_slash = -1;

	for (int i = str_len - 1; i >= 0; i--) {
		if (path[i] == '/') {
			last_slash = i;
			break;
		}
	}

	return last_slash;
}

/**
 * Gets the parent directory.
 * Works for nested directories.
 */
char *get_parent_dir(const char *path)
{
	int last_slash = last_slash_idx(path);
	assert(last_slash >= 0);

	char *ret_val = malloc(last_slash + 2);
	memcpy(ret_val, path, last_slash + 2);
	ret_val[last_slash + 1] = 0;

	return ret_val;
}

/**
 * Gets the name of the directory or file.
 * Works for nested directories.
 */
char *get_file_name(const char *path)
{
	int str_len = strlen(path);
	int last_slash = last_slash_idx(path);
	assert(last_slash >= 0);

	int new_len = str_len - last_slash;
	char *ret_val = malloc(new_len);
	memcpy(ret_val, path + last_slash + 1, new_len);

	return ret_val;
}

int add_at_deleted_entry(int parent_inum, int child_inum,
			 const char *child_name)
{
	int total_entries = get_num_entries(parent_inum);
	for (int i = 0; i < total_entries; i++) {
		entry *file_entry = get_entry(parent_inum, i);
		if (file_entry->flag == DELETED) {
			file_entry->inum = child_inum;
			file_entry->flag = ACTIVE;
			memcpy(file_entry->file_name,
			       child_name,
			       strlen(child_name));

			return 0;
		}
	}

	return -1;
}

int add_new_entry_at_end(int parent_inum, int child_inum,
			 const char *child_name)
{
	inode *parent_inode = get_inode_ptr(parent_inum);
	off_t new_size = parent_inode->size + sizeof(entry);
	if (new_size > parent_inode->num_blocks * BLK_SIZE) {
		int rv = assign_block_to_inode(parent_inum);
		if (rv < 0) {
			return -ENOMEM;
		}
	}

	int last_blk_idx = parent_inode->num_blocks - 1;
	void *parent_data_ptr = get_block_ptr(
		parent_inode->blocks[last_blk_idx]);
	parent_data_ptr += parent_inode->size % BLK_SIZE;
	entry *new_entry = (entry *) parent_data_ptr;

	new_entry->inum = child_inum;
	new_entry->flag = ACTIVE;
	memcpy(new_entry->file_name, child_name, strlen(child_name));

	parent_inode->size += sizeof(entry);

	return 0;
}

int add_file_entry(int parent_inum, int child_inum, const char *child_name)
{
	int rv = add_at_deleted_entry(parent_inum, child_inum, child_name);
	if (rv == 0) {
		return 0;
	}

	return add_new_entry_at_end(parent_inum, child_inum, child_name);
}

int add_file(int parent_inum, const char *file_name, mode_t mode, dev_t rdev,
	     char flag)
{
	int new_inum = assign_free_inode();
	if (new_inum == -1) {
		return -ENOMEM;
	}

	inode *new_inode = get_inode_ptr(new_inum);
	if (flag == DIR_FLAG) {
		new_inode->mode = mode | 040000;
	} else {
		new_inode->mode = mode;
	}
	new_inode->rdev = rdev;
	new_inode->nlinks = 1;
	new_inode->uid = getuid();
	time_t now = time(0);
	new_inode->atime = now;
	new_inode->ctime = now;
	new_inode->mtime = now;
	new_inode->flag = flag;

	return add_file_entry(parent_inum, new_inum, file_name);
}

int storage_mknod(char const *path, mode_t mode, dev_t rdev)
{
	char *parent_dir = get_parent_dir(path);
	char *new_file_name = get_file_name(path);
	if (strlen(new_file_name) > NAME_LENGTH) {
		return -ENAMETOOLONG;
	}

	int parent_inum = (get_file_entry(parent_dir))->inum;
	int rv = add_file(parent_inum, new_file_name, mode, rdev, FILE_FLAG);

	free(parent_dir);
	free(new_file_name);
	return rv;
}

int remove_link(int inum)
{
	inode *node = get_inode_ptr(inum);
	node->nlinks--;

	if (node->nlinks == 0) {
		set_inode_bm(inum, FREE);
		for (int i = 0; i < node->num_blocks; i++) {
			set_block_bm(get_dnum(inum, i), FREE);
			void *blk_ptr = get_block_ptr(get_dnum(inum, i));
			memset(blk_ptr, 0, BLK_SIZE);
		}
		memset(node, 0, sizeof(inode));
	}

	return 0;
}

int storage_unlink(const char *path)
{
	entry *file_entry = get_file_entry(path);
	if (file_entry == NULL || file_entry->flag == DELETED) {
		return -ENOENT;
	}

	int inum = file_entry->inum;
	memset(file_entry, 0, sizeof(entry));
	file_entry->flag = DELETED;

	return remove_link(inum);
}

int storage_set_time(const char *path, const struct timespec ts[2])
{
	inode *node = get_inode_ptr((get_file_entry(path))->inum);
	node->atime = ts[0].tv_sec;
	node->mtime = ts[1].tv_sec;

	return 0;
}

int storage_chmod(const char *path, mode_t mode)
{
	entry *file_entry = get_file_entry(path);
	if (file_entry == NULL) {
		return -ENOENT;
	}

	inode *node = get_inode_ptr(file_entry->inum);
	node->mode = mode;

	return 0;
}

int storage_rename(const char *from, const char *to)
{
	entry *file_entry = get_file_entry(from);
	if (file_entry == NULL) {
		return -ENOENT;
	}

	/*char *new_name = get_file_name(to);
	strcpy(file_entry->file_name, new_name);*/

	int rv = storage_link(from, to);
	if (rv != 0) {
		return rv;
	}

	return storage_unlink(from);
}

int truncate_to_less(int inum, off_t size)
{
	inode *node = get_inode_ptr(inum);
	assert(node->size > size);

	int new_num_blocks = div_up(size, BLK_SIZE);
	for (int i = new_num_blocks; i < node->num_blocks; i++) {
		set_block_bm(get_dnum(inum, i), FREE);
	}

	node->num_blocks = new_num_blocks;
	node->size = size;

	return 0;
}

int truncate_to_more(int inum, off_t size)
{
	inode *node = get_inode_ptr(inum);
	assert(node->size < size);

	if (node->num_blocks != 0) {
		void *last_blk_ptr = get_block_ptr(
			get_dnum(inum, node->num_blocks - 1));
		off_t offset = node->size % BLK_SIZE;
		last_blk_ptr += offset;
		size_t remaining = BLK_SIZE - offset;
		if (remaining != BLK_SIZE) {
			memset(last_blk_ptr, 0, remaining);
		}
	}

	int new_num_blocks = div_up(size, BLK_SIZE);
	for (int i = node->num_blocks; i < new_num_blocks; i++) {
		int rv = assign_block_to_inode(inum);
		if (rv == -1) {
			return -ENOMEM;
		}

		void *blk = get_block_ptr(get_dnum(inum, i));
		memset(blk, 0, BLK_SIZE);
	}

	node->size = size;
	node->num_blocks = new_num_blocks;

	return 0;
}

int storage_truncate(const char *path, off_t size)
{
	entry *file_entry = get_file_entry(path);
	if (file_entry == NULL) {
		return -ENOENT;
	}
	inode *node = get_inode_ptr(file_entry->inum);

	if (node->size > size) {
		return truncate_to_less(file_entry->inum, size);
	}

	if (node->size < size) {
		return truncate_to_more(file_entry->inum, size);
	}

	// do nothing if truncate same size
	return 0;
}

int storage_write(const char *path, const char *buf, size_t size, off_t offset,
		  struct fuse_file_info *fi)
{
	entry *file_entry = get_file_entry(path);
	if (file_entry == NULL) {
		storage_mknod(path, 0755, 0);
		file_entry = get_file_entry(path);
		assert(file_entry != NULL);
	}

	int inum = file_entry->inum;
	inode *node = get_inode_ptr(inum);
	int rv = storage_truncate(path, max(size + offset, node->size));
	if (rv != 0) {
		return rv;
	}

	int start_blk_idx = offset / BLK_SIZE;
	void *data = get_block_ptr(get_dnum(inum, start_blk_idx))
		     + offset % BLK_SIZE;
	size_t remainder = min(size, BLK_SIZE - offset % BLK_SIZE);
	memcpy(data, buf, remainder);
	if (remainder >= size) {
		update_mctime(inum);
		return remainder;
	}

	size_t size_left = size - remainder;
	off_t buf_offset = remainder;
	for (int i = start_blk_idx + 1; i < node->num_blocks; i++) {
		data = get_block_ptr(get_dnum(inum, i));
		if (size_left <= BLK_SIZE) {
			memcpy(data, buf + buf_offset, size_left);
			update_mctime(inum);
			return size;
		}

		memcpy(data, buf + buf_offset, BLK_SIZE);
		size_left -= BLK_SIZE;
		buf_offset += BLK_SIZE;
	}

	update_mctime(inum);
	return size;
}

int storage_read(const char *path, char *buf, size_t size, off_t offset,
		 struct fuse_file_info *fi)
{
	entry *file_entry = get_file_entry(path);
	if (file_entry == NULL) {
		return -ENOENT;
	}

	int inum = file_entry->inum;
	inode *node = get_inode_ptr(inum);

	if (offset > node->size || node->size == 0) {
		update_atime(inum);
		return 0;
	}

	int start_blk_idx = offset / BLK_SIZE;
	void *data = get_block_ptr(get_dnum(inum, start_blk_idx))
		     + offset % BLK_SIZE;
	size_t remainder = min(size, BLK_SIZE - offset % BLK_SIZE);
	memcpy(buf, data, remainder);

	if (remainder >= size) {
		update_atime(inum);
		return remainder;
	}

	size_t size_left = size - remainder;
	off_t buf_offset = remainder;
	for (int i = start_blk_idx + 1; i < node->num_blocks; i++) {
		data = get_block_ptr(get_dnum(inum, i));
		if (size_left <= BLK_SIZE) {
			memcpy(buf + buf_offset, data, size_left);
			update_atime(inum);
			return size;
		}

		memcpy(buf + buf_offset, data, BLK_SIZE);
		size_left -= BLK_SIZE;
		buf_offset += BLK_SIZE;

	}

	update_atime(inum);
	return size;
}

int storage_link(const char *from, const char *to)
{
	entry *file_entry = get_file_entry(from);
	if (file_entry == NULL) {
		return -ENOENT;
	}

	inode *node = get_inode_ptr(file_entry->inum);
	char *parent_dir = get_parent_dir(to);
	entry *parent_entry = get_file_entry(parent_dir);
	if (parent_entry == NULL) {
		return -ENOENT;
	}

	char *new_file_name = get_file_name(to);
	add_file_entry(parent_entry->inum, file_entry->inum, new_file_name);

	node->nlinks++;

	return 0;
}

int storage_mkdir(const char *path, mode_t mode)
{
	char *parent_dir = get_parent_dir(path);
	char *new_dir_name = get_file_name(path);
	if (strlen(new_dir_name) > NAME_LENGTH) {
		return -ENAMETOOLONG;
	}

	entry *parent_entry = get_file_entry(parent_dir);
	if (parent_entry == NULL) {
		return -ENOENT;
	}
	int parent_inum = parent_entry->inum;
	int rv = add_file(parent_inum, new_dir_name, mode, 0, DIR_FLAG);

	free(parent_dir);
	free(new_dir_name);

	return rv;
}

int dir_empty(int inum)
{
	int total_entries = get_num_entries(inum);

	for (int i = 0; i < total_entries; i++) {
		entry *curr = get_entry(inum, i);
		if (curr->flag == ACTIVE) {
			return 0;
		}
	}

	return 1;
}

int storage_rmdir(const char *path)
{
	entry *dir_entry = get_file_entry(path);
	if (dir_entry == NULL || dir_entry->flag == DELETED) {
		return -ENOENT;
	}

	int inum = dir_entry->inum;
	inode *node = get_inode_ptr(inum);
	if (node->flag == FILE_FLAG) {
		return -ENOTDIR;
	}

	if (!dir_empty(inum)) {
		return -ENOTEMPTY;
	}

	int rv = remove_link(inum);
	assert(rv == 0);

	memset(dir_entry, 0, sizeof(entry));
	dir_entry->flag = DELETED;

	return rv;
}

int storage_access(const char *path, int mask)
{
	entry *file_entry = get_file_entry(path);
	if (file_entry == NULL) {
		return -ENOENT;
	}

	inode *node = get_inode_ptr(file_entry->inum);
	int mode = node->mode | mask;
	if (mode > node->mode) {
		return -EACCES;
	}

	return 0;
}