#include "mem.h"

#include <mimalloc.h>

void* memalloc(uint64_t size) {
    return mi_malloc(size);
}

void memfree(void* ptr) {
    mi_free(ptr);
}
