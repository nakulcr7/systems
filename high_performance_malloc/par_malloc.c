#include <stdlib.h>
#include <unistd.h>
#include "xmalloc.h"
#include "opt_malloc.h"

void*
xmalloc(size_t bytes)
{
    return opt_malloc(bytes);
}

void
xfree(void* ptr)
{
    opt_free(ptr);
}

void*
xrealloc(void* prev, size_t bytes)
{
	return opt_realloc(prev, bytes);
}
