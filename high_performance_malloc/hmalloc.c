#include <stdlib.h>
#include <sys/mman.h>
#include <stdio.h>
#include <pthread.h>

#include "hmalloc.h"

/*
 * size is the size of the whole node (pointer starting at node_t)
 */
typedef struct node_t {
	size_t size;
	struct node_t *next;
} node;

/*
 * size is the size of the header + pointer
 */
typedef struct header_t {
	size_t size;
} header;

const size_t PAGE_SIZE = 4096;
static hm_stats stats;
static node *free_list;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

long free_list_length()
{
	long len = 0;
	node *curr = free_list;
	while (curr != NULL) {
		len++;
		curr = curr->next;
	}

	return len;
}

hm_stats* hgetstats()
{
	stats.free_length = free_list_length();
	return &stats;
}

void hprintstats()
{
	stats.free_length = free_list_length();
	fprintf(stderr, "\n== husky malloc stats ==\n");
	fprintf(stderr, "Mapped:   %ld\n", stats.pages_mapped);
	fprintf(stderr, "Unmapped: %ld\n", stats.pages_unmapped);
	fprintf(stderr, "Allocs:   %ld\n", stats.chunks_allocated);
	fprintf(stderr, "Frees:    %ld\n", stats.chunks_freed);
	fprintf(stderr, "Freelen:  %ld\n", stats.free_length);
}

void *map_pages(long num_pages)
{
	stats.pages_mapped += num_pages;
	return mmap(0, num_pages * PAGE_SIZE, PROT_READ|PROT_WRITE,
		    MAP_ANONYMOUS|MAP_SHARED, -1, 0);
}

void init_free_list()
{
	free_list = map_pages(1);
	free_list->size = PAGE_SIZE;
	free_list->next = NULL;
}

static size_t div_up(size_t xx, size_t yy)
{
	size_t zz = xx / yy;

	if (zz * yy == xx) {
		return zz;
	} else {
		return zz + 1;
	}
}

void coalesce(node *chunk) {
	if ((void *) chunk + chunk->size == (void *) chunk->next) {
		long new_size = chunk->size + chunk->next->size;
		chunk->next = chunk->next->next;
		chunk->size = new_size;
	}
}

void add_to_free_list(node *to_add)
{
	node *left_block = free_list;
	node *right_block = free_list;
	while (right_block != NULL && to_add > right_block) {
		left_block = right_block;
		right_block = right_block->next;
	}

	if (right_block == free_list) {
		to_add->next = right_block;
		free_list = to_add;
	} else {
		left_block->next = to_add;
		to_add->next = right_block;
	}

	coalesce(to_add);

	if (to_add != free_list) {
		coalesce(left_block);
	}
}

void *hmalloc_small(size_t size)
{
	if (free_list == NULL) {
		init_free_list();
	}

	if (size < sizeof(node)) {
		size = sizeof(node);
	}

	node *block_prev = free_list;
	node *block = free_list;

	// searches for the first block that can contain size
	// stores in block
	while (block != NULL && block->size < size) {
		block_prev = block;
		block = block->next;
	}

	int need_new_map = block == NULL;

	if (need_new_map) { // there is no appropriate block, so mmap
		block = map_pages(1);
		block->size = PAGE_SIZE;
		block->next = NULL;
	}

	header *ret_hdr = (header *) block;
	size_t new_size = block->size - size;

	// unable to make a new node, so just give the user the whole block
	if (new_size < sizeof(node)) {
		if (block == free_list) {
			if (free_list->next == NULL) {
				free_list = NULL;
			} else {
				free_list = free_list->next;
			}
		} else {
			block_prev->next = block->next;
		}

		ret_hdr->size = block->size;

		return ret_hdr + 1;
	}

	node *block_new = (void *) block + size;
	block_new->size = new_size;
	block_new->next = block->next;

	if (need_new_map) {
		add_to_free_list(block_new);
	} else {
		if (block == free_list) {
			free_list = block_new;
		} else {
			block_prev->next = block_new;
		}

		block_new->next = block->next;
	}

	ret_hdr->size = size;

	return ret_hdr + 1;
}

void *hmalloc_big(size_t size)
{
	int total_pages = div_up(size, PAGE_SIZE);
	header *result = map_pages(total_pages);
	result->size = total_pages * PAGE_SIZE;
	return result + 1;
}

void *hmalloc(size_t size)
{
	pthread_mutex_lock(&lock);

	stats.chunks_allocated += 1;
	size_t total_size = size + sizeof(header);
	void *ret_val;

	if (total_size < PAGE_SIZE) {
		ret_val =  hmalloc_small(total_size);
	} else {
		ret_val = hmalloc_big(size);
	}

	pthread_mutex_unlock(&lock);
	return ret_val;
}

void hfree_big(header *hdr)
{
	long pages_unmapped = div_up(hdr->size, PAGE_SIZE);
	stats.pages_unmapped += pages_unmapped;
	munmap(hdr, pages_unmapped * PAGE_SIZE);
}

void hfree_small(header *hdr)
{
	node *free_chunk = (node *) hdr;
	free_chunk->size = hdr->size;
	free_chunk->next = NULL;
	add_to_free_list(free_chunk);
}

void hfree(void *item)
{
	pthread_mutex_lock(&lock);

	stats.chunks_freed += 1;

	header *hdr = (header *) (item - sizeof(header));

	if (hdr->size >= PAGE_SIZE) {
		hfree_big(hdr);
	} else {
		hfree_small(hdr);
	}

	pthread_mutex_unlock(&lock);
}
