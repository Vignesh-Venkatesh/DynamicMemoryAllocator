#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <cstddef>

void initialize_heap();
void* memory_alloc(size_t size);
void memory_free(void* ptr);
void printAllBlocks();

#endif
