
#include <stdlib.h>
#include <sys/mman.h>
#include <stdio.h>

#include "hmalloc.h"

typedef struct hcons {
    size_t size;
    struct hcons* next;
} hcons;

static hcons* free_list = 0;

/*
  typedef struct hm_stats {
  long pages_mapped;
  long pages_unmapped;
  long chunks_allocated;
  long chunks_freed;
  long free_length;
  } hm_stats;
*/

const size_t PAGE_SIZE = 4096;
static hm_stats stats;

long
free_list_length()
{
    long len = 0;
    for (hcons* pp = free_list; pp != 0; pp = pp->next) {
        len += 1;
    }
    return len;
}

hm_stats*
hgetstats()
{
    stats.free_length = free_list_length();
    return &stats;
}

void
hprintstats()
{
    stats.free_length = free_list_length();
    fprintf(stderr, "\n== husky malloc stats ==\n");
    fprintf(stderr, "Mapped:   %ld\n", stats.pages_mapped);
    fprintf(stderr, "Unmapped: %ld\n", stats.pages_unmapped);
    fprintf(stderr, "Allocs:   %ld\n", stats.chunks_allocated);
    fprintf(stderr, "Frees:    %ld\n", stats.chunks_freed);
    fprintf(stderr, "Freelen:  %ld\n", stats.free_length);
}

hcons*
hmalloc_pages(size_t npages)
{
    stats.pages_mapped += npages;

    hcons* block = mmap(0, PAGE_SIZE * npages, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);

    if (block == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    else {
        block->size = PAGE_SIZE * npages;
        return block;
    }
}

static void
free_list_insert(hcons* item)
{
    if (item == 0) {
        return;
    }

    size_t addr0 = (size_t) item;

    hcons* curr = free_list;
    hcons* prev = 0;

    while (1) {
        size_t addr1 = (size_t) curr;

        if (curr != 0 && addr1 + curr->size == addr0) {
            // Merge A!
            curr->size += item->size;

            if (prev) {
                prev->next = curr->next;
            }
            else {
                free_list = curr->next;
            }

            free_list_insert(curr);

            return;
        }

        if (curr != 0 && addr0 + item->size == addr1) {
            // Merge B!

            item->size += curr->size;

            if (prev) {
                prev->next = curr->next;
            }
            else {
                free_list = curr->next;
            }

            free_list_insert(item);

            return;
        }

        if (curr == 0 || addr0 < addr1) {
            // Insert

            if (prev) {
                item->next = curr;
                prev->next = item;
            }
            else {
                item->next = free_list;
                free_list = item;
            }
            return;
        }

        prev = curr;
        curr = curr->next;
    }

}

static
void*
hmalloc_small(size_t size)
{
    hcons* curr = free_list;
    hcons* prev = 0;
    hcons* item = 0;

    // Find a block in the free list.
    while (curr != 0) {
        if (curr->size >= size) {
            item = curr;
            break;
        }

        prev = curr;
        curr = curr->next;
    }

    // Did we get one?
    if (item) {
        if (prev) {
            prev->next = item->next;
        }
        else {
            free_list = item->next;
        }
    }
    else {
        // Nope, allocate new page.
        item = hmalloc_pages(1);
    }

    // Can we split this block?
    off_t diff = ((off_t) item->size) - ((off_t) size);
    if (diff > sizeof(hcons)) {
        hcons* bb2 = ((hcons*) (((void*)item) + size));
        bb2->size = (size_t) diff;
        free_list_insert(bb2);
        item->size = size;
    }

    return &(item->next);
}

static
size_t
div_up(size_t xx, size_t yy)
{
    size_t zz = xx / yy;

    if (zz * yy == xx) {
        return zz;
    }
    else {
        return zz + 1;
    }
}

static
void*
hmalloc_large(size_t size)
{
    size_t pages = div_up(size, PAGE_SIZE);
    hcons* item = hmalloc_pages(pages);
    return &(item->next);
}

void*
hmalloc(size_t size)
{
    stats.chunks_allocated += 1;

    size += sizeof(size_t);

    if (size < PAGE_SIZE) {
        return hmalloc_small(size);
    }
    else {
        return hmalloc_large(size);
    }
}

void
hfree(void* item)
{
    stats.chunks_freed += 1;
    hcons* cell = (item - sizeof(size_t));

    if (cell->size < PAGE_SIZE) {
        free_list_insert(cell);
    }
    else {
        stats.pages_unmapped += cell->size / PAGE_SIZE;

        int rv = munmap(cell, cell->size);
        if (rv == -1) {
            perror("munmap");
            exit(1);
        }
    }
}

