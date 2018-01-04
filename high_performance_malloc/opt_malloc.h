#ifndef OPT_MALLOC_H
#define OPT_MALLOC_H

typedef struct node_t {
	struct node_t *next;
	size_t size;
} node;

typedef struct header_t {
	size_t size;
} header;

void *opt_malloc(size_t bytes);
void opt_free(void *ptr);
void *opt_realloc(void *prev, size_t bytes);

#endif