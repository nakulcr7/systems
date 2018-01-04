#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>

#include "opt_malloc.h"

#define PAGE_SIZE 4096
#define BIG_SIZE (PAGE_SIZE * 1000)
#define SEGREGATED_LIST_ALLOC_SIZE 1000
#define BIN_ALLOC_SIZE 600
#define NUM_BINS 7

typedef struct arena_t {
	node *free_list;
	node *free_list_16;
	node *free_list_24;
	node *free_list_64;
	node *bins[NUM_BINS];

} arena;

static __thread arena aren;

/**
 * bins[0] -> 32 bytes
 * bins[1] -> 128 bytes (64 bytes handled by segregation)
 * bins[2] -> 256 bytes
 * bins[3] -> 512 bytes
 * bins[4] -> 1024 bytes
 * bins[5] -> 2048 bytes
 * bins[6] -> 4096 bytes
 **/
static size_t sizes[NUM_BINS] = {32, 128, 256, 512, 1024, 2048, 4096};

void *map(size_t bytes)
{
	return mmap(0,
		    bytes,
		    PROT_READ|PROT_WRITE,
		    MAP_SHARED|MAP_ANONYMOUS,
		    -1,
		    0);
}

node *search_size(node *list, size_t size)
{
	node *curr = list;
	while (curr != NULL && curr->size < size) {
		curr = curr->next;
	}

	return curr;
}

/**
 * Adds a page of the given number of bytes to the global free list
 * @param bytes number of bytes
 */
void fl_add_page(size_t bytes)
{
	void *ptr = map(bytes + sizeof(node));

	node *new_node = (node *) ptr;
	new_node->size = bytes;

	if (aren.free_list == NULL) {
		aren.free_list = new_node;
	} else {
		new_node->next = aren.free_list;
		aren.free_list = new_node;
	}
}

/**
 * Takes the given amount of bytes from the target node
 * @param target_node the target to take memory from
 * @param bytes number of bytes to take
 * @return the pointer to memory taken
 */
void *get_memory(node *target_node, size_t bytes)
{
	void *ret_val = target_node;

	node *new_node = (node *) (ret_val + bytes);
	new_node->size = target_node->size - bytes;
	new_node->next = target_node->next;

	if (target_node == aren.free_list) {
		aren.free_list = new_node;
	}

	target_node->size = bytes;

	return ret_val;
}

/**
 * Init the free list for that size with some number of chunks of that size
 * @param bytes 16 or 24 or 64
 */
void init_fl(size_t bytes)
{
	size_t total_size = (bytes + sizeof(node)) * SEGREGATED_LIST_ALLOC_SIZE;

	node *result = search_size(aren.free_list, total_size);
	if (result == NULL) {
		fl_add_page(BIG_SIZE);
		result = aren.free_list;
	}

	void *ptr = get_memory(result, total_size);
	if (bytes == 16) {
		aren.free_list_16 = ptr;
	} else if (bytes == 24) {
		aren.free_list_24 = ptr;
	} else {
		aren.free_list_64 = ptr;
	}

	node *list = (node *) ptr;
	list->size = bytes;

	node *curr = list;
	for (int i = 1; i < SEGREGATED_LIST_ALLOC_SIZE; i++) {
		node *new_node = (void *) (curr + 1) + bytes;

		new_node->size = bytes;
		curr->next = new_node;
		new_node->next = NULL;

		curr = new_node;
	}
}

void *opt_malloc_16()
{
	if (aren.free_list_16 == NULL) {
		init_fl(16);
	}

	void *ret_val = aren.free_list_16 + 1;
	aren.free_list_16 = aren.free_list_16->next;

	return ret_val;
}

void *opt_malloc_24()
{
	if (aren.free_list_24 == NULL) {
		init_fl(24);
	}

	void *ret_val = aren.free_list_24 + 1;
	aren.free_list_24 = aren.free_list_24->next;

	return ret_val;
}

void *opt_malloc_64()
{
	if (aren.free_list_64 == NULL) {
		init_fl(64);
	}

	void *ret_val = aren.free_list_64 + 1;
	aren.free_list_64 = aren.free_list_64->next;

	return ret_val;
}

void *opt_malloc_big(size_t bytes)
{
	header *hdr = map(bytes + sizeof(header));
	hdr->size = bytes;

	return hdr + 1;
}

int which_bin(size_t bytes)
{
	for (int i = 0; i < NUM_BINS; i++) {
		if (bytes <= sizes[i]) {
			return i;
		}
	}

	return -1;
}

void init_bin(int bin_number)
{
	size_t bytes = sizes[bin_number];

	size_t each_node = sizeof(node) + bytes;
	size_t total = each_node * BIN_ALLOC_SIZE;

	node *result = search_size(aren.free_list, total);
	if (result == NULL) {
		fl_add_page(BIG_SIZE);
		result = aren.free_list;
	}

	void *ptr = get_memory(result, total);
	aren.bins[bin_number] = ptr;

	node *list = (node *) ptr;
	list->size = bytes;
	list->next = NULL;

	node *curr = list;
	for (int i = 1; i < BIN_ALLOC_SIZE; i++) {
		node *new_node = (void *) (curr + 1) + bytes;

		new_node->size = bytes;
		curr->next = new_node;

		curr = new_node;
	}
}

void *opt_malloc_bin(size_t bytes)
{
	int bin = which_bin(bytes);

	if (aren.bins[bin] == NULL) {
		init_bin(bin);
	}

	void *ret_val = aren.bins[bin] + 1;
	aren.bins[bin] = aren.bins[bin]->next;

	return ret_val;
}

void *opt_malloc(size_t bytes)
{
	if (bytes == 16) {
		return opt_malloc_16();
	}

	if (bytes == 24) {
		return opt_malloc_24();
	}

	if (bytes == 64) {
		return opt_malloc_64();
	}

	if (bytes > PAGE_SIZE) {
		return opt_malloc_big(bytes);
	}

	return opt_malloc_bin(bytes);
}

void opt_free_segregated(void *ptr)
{
	node *ptr_node = (node *)(ptr - sizeof(node));
	if (ptr_node->size == 16) {
		ptr_node->next = aren.free_list_16;
		aren.free_list_16 = ptr_node;
	} else if (ptr_node->size == 24) {
		ptr_node->next = aren.free_list_24;
		aren.free_list_24 = ptr_node;
	} else {
		ptr_node->next = aren.free_list_64;
		aren.free_list_64 = ptr_node;
	}
}

void opt_free_big(header *hdr)
{
	munmap(hdr, hdr->size + sizeof(header));
}

void opt_free_bin(void *ptr)
{
	node *ptr_node = (node *) (ptr - sizeof(node));
	int bin = which_bin(ptr_node->size);
	if (aren.bins[bin] != NULL) {
		ptr_node->next = aren.bins[bin];
	}
	aren.bins[bin] = ptr_node;
}

void opt_free(void *ptr)
{
	header *hdr = ptr - sizeof(header);
	if (hdr->size == 16 || hdr->size == 24 || hdr->size == 64) {
		opt_free_segregated(ptr);
		return;
	}

	if (hdr->size > PAGE_SIZE) {
		opt_free_big(hdr);
		return;
	}

	opt_free_bin(ptr);
}

void *opt_realloc(void *prev, size_t size) {
	header *hdr = prev - sizeof(header);
	if (hdr->size >= size) {
		return prev;
	}
	void *new = opt_malloc(size);
	memcpy(new, prev, hdr->size);
	opt_free(prev);
	return new;
}
