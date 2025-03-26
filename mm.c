/*
mm.c
 
Name: Vignesh Venkatesh
 
  
Design of my heap
 
During Heap Initialization
+-----------+-----------------+-----------------+----------+ 
| Alignment | Prologue Header | Prologue Footer | Epilogue |
+-----------+-----------------+-----------------+----------+
|  8 bytes  |     8 bytes     |     8 bytes     |  8bytes  |
+----------------------------------------------------------+
Total: 32 bytes

Allocated Block Structure:
+------------------+---------+
| Header (8 bytes) | Payload |
+------------------+---------+
Header: Stores size information and allocation status
Footer: Stores size information and allocation status

Aligned to 16 bytes. However, due to the nature of the block sturcture, minimum size is 32 bytes.

Free Block Structure:
+------------------+------+------+------------------+
| Header (8 bytes)   prev   next | Footer (8 bytes) |
+------------------+------+------+------------------+
Header: Stores size information and allocation status, additionally stores prev and next free blocks
Footer: Stores size information and allocation status

Aligned to 16 bytes. However, due to the nature of the block structure, minimum size is 32 bytes.

 */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>

#include "mm.h"
#include "memlib.h"

/*
 * If you want to enable your debugging output and heap checker code,
 * uncomment the following line. Be sure not to have debugging enabled
 * in your final submission.
 */
// #define DEBUG

#ifdef DEBUG
// When debugging is enabled, the underlying functions get called
#define dbg_printf(...) printf(__VA_ARGS__)
#define dbg_assert(...) assert(__VA_ARGS__)
#else
// When debugging is disabled, no code gets generated
#define dbg_printf(...)
#define dbg_assert(...)
#endif // DEBUG

// do not change the following!
#ifdef DRIVER
// create aliases for driver tests
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#define memset mm_memset
#define memcpy mm_memcpy
#endif // DRIVER

#define ALIGNMENT 16

// rounds up to the nearest multiple of ALIGNMENT
static size_t align(size_t x)
{
    return ALIGNMENT * ((x+ALIGNMENT-1)/ALIGNMENT);
}

struct block{
    size_t size:62; // stores the size of the block
    unsigned allocated:1; // stores if the block is allocated or not (0 = not allocated, 1 = allocated)
    unsigned prev_alloc:1; // stores if the block before is allocated or not (0 = not allocated, 1 = allocated)
};

struct free_block{
    size_t size:62; // stores the size of the block
    unsigned allocated:1; // stores if the block is allocated or not (0 = not allocated, 1 = allocated)
    unsigned prev_alloc:1; // stores if the block before is allocated or not (0 = not allocated, 1 = allocated)
    struct free_block *prev; // stores the previous free block
    struct free_block *next; // stores the next free block
};

// NOTE: these are just aliases for me to make sure its easy to implement (not being confused with a single alias)
typedef struct block block_t; // for alignment, prologue header, prologue footer, epilogue
typedef struct block allocated_header; // stores header information for the allocated block
typedef struct free_block free_block_start; // stores the free block start information (size, allocation status, prev free block, next free block)
typedef struct block free_block_footer; // stores footer information for the free block

// to keep track of the start of the heap
static void *heap_start;

// helper function to return the number of segregated free lists
static int get_num_lists(void) {
    return 11;
}

static free_block_start *segregated_free_lists[11]; // setting the number of free list array

// getting array index based on size of the block
static int get_seg_list_index(size_t size) {
    if (size <= 32)
        return 0;
    else if (size <= 48)
        return 1;
    else if (size <= 64)
        return 2;
    else if (size <= 96)
        return 3;
    else if (size <= 128)
        return 4;
    else if (size <= 256)
        return 5;
    else if (size <= 512)
        return 6;
    else if (size <= 1024)
        return 7;
    else if (size <= 2048)
        return 8;
    else if (size <= 4096)
        return 9;
    else
        return 10;
}


// helper function to return aligned size of allocated block
static size_t aligned_allocated_size(size_t size){
    return align(size+sizeof(allocated_header)); // includes header + size of payload + padding (optional - coz of alignment) 
}

// helper function to get footer of free block
static free_block_footer *get_free_footer(free_block_start *free_block_header){
    return (free_block_footer *)((char *)free_block_header + free_block_header->size - sizeof(free_block_footer));
}

// helper function to move the epilogue to the end of the heap during heap extension
// however returns the position where the epilogue was originally at i.e., where the new allocated block would begin
static block_t *move_epilogue(block_t *epilogue, size_t size){
    block_t *original_epilogue_location = epilogue;

    // moving the epilogue, and reinitializing epilogue info (basically I am not moving the epilogue itself, just the p ointer)
    epilogue = (block_t *)((char *)epilogue + size);
    epilogue->size = 0;
    epilogue->allocated = 1;
    return original_epilogue_location;
}

// helper function to set next block's prev_alloc bit
void set_next_block_prev_alloc(block_t *current_block, int allocation_status){
    block_t *next_block = (block_t *)((char *)current_block + current_block->size);
    next_block->prev_alloc = allocation_status;
}

// helper function to insert free block in free list
static void insert_free_block(free_block_start *free_block) {
    int index = get_seg_list_index(free_block->size); // getting which bucket to insert the free block into
    
    // setting the free block to be the first free block in the free list
    free_block->next = segregated_free_lists[index];
    free_block->prev = NULL;

    // if there was a block before in the free list
    if (segregated_free_lists[index] != NULL) {
        segregated_free_lists[index]->prev = free_block;
    }
    // if no block before
    segregated_free_lists[index] = free_block;
}


// helper function to remove free block from free list
static void remove_free_block(free_block_start *free_block) {
    int index = get_seg_list_index(free_block->size); // getting which bucket to remove the free block from

    // removing the free block from the free list
    if (free_block->prev) {
        free_block->prev->next = free_block->next;
    } else {
        segregated_free_lists[index] = free_block->next;
    }
    if (free_block->next) {
        free_block->next->prev = free_block->prev;
    }
    free_block->prev = NULL;
    free_block->next = NULL;
}


// Splitting free block
static allocated_header *split_block(free_block_start *free_block_header, size_t allocation_size){

    // calculating the remaining free space available for splitting
    size_t remaining_size = free_block_header->size - allocation_size;

    // minimum size I'm setting for the free block (in 64-bit machines this should be 32 bytes, since 24 bytes for the free block header (size, allocation, next and prev) and 8 bytes for the free block footer)
    size_t required_min_size = sizeof(free_block_start)+sizeof(free_block_footer); 

    //remove free block from the free list
    remove_free_block(free_block_header);

    // if there is enough space
    if (remaining_size >= required_min_size){
        // create a new block from the remaining free space

        // setting the free block header
        // moving to the end of the allocated block to begin the new free block
        free_block_start *new_free_block = (free_block_start *)((char *)free_block_header + allocation_size);
        new_free_block->size = remaining_size; // setting the size info of the free block
        new_free_block->allocated = 0; // changing the allocation status to "free"
        set_next_block_prev_alloc((block_t *)new_free_block, 0);

        // setting the free block footer
        free_block_footer *new_free_footer = (free_block_footer *)((char *)new_free_block + remaining_size - sizeof(free_block_footer));
        new_free_footer->size = remaining_size; // setting the size info of the free block
        new_free_footer->allocated = 0; // changing the allocation status to "free"

        // inserting the new split block into free list
        insert_free_block(new_free_block);
    } else {
        // if not enough space to split, we allocate the entire block
        allocation_size = free_block_header->size;
    }
    
    // modifying information about the allocated block
    
    // setting the header info
    allocated_header *allocated_block = (allocated_header *)free_block_header;
    allocated_block->size = allocation_size; // setting the size info of the allocated block
    allocated_block->allocated = 1; // changing the allocation status to "allocated"
    set_next_block_prev_alloc(allocated_block,1);
    
    return allocated_block; // returning the allocated block
    
}

static free_block_start *coalesce(free_block_start *free_block){
    free_block_start *prev_block = NULL;
    free_block_start *next_block = NULL;

    // getting the next block header
    free_block_start *next_header = (free_block_start *)((char *)free_block + free_block->size);

    //checking the allocation status of the next block, if it is free, we update our next block to set that block
    if (next_header->allocated == 0){

        // setting the next free block
        next_block = next_header;
    }

    // checking the allocation status of the previous block, if it is free, we update our previous block to set that block
    if (free_block->prev_alloc == 0){
        // getting the previous block footer
        free_block_footer *prev_footer = (free_block_footer *)((char *)free_block - sizeof(free_block_footer));
        
        // setting the previous free block
        prev_block = (free_block_start *)((char *)free_block-prev_footer->size);
    }

    // case 1: if both previous and next blocks are free, we coalesce both of them
    if (prev_block && next_block){
        // removing all the free blocks from the free list
        remove_free_block(prev_block);
        remove_free_block(next_block);
        remove_free_block(free_block);

        // setting the information about size and allocation in the new free block
        prev_block->size = prev_block->size + free_block->size + next_block->size;
        prev_block->allocated = 0;
        set_next_block_prev_alloc((block_t *)prev_block, 0);
        
        // setting the footer for the new free block
        free_block_footer *new_footer = get_free_footer(prev_block);
        new_footer->size= prev_block->size;
        new_footer->allocated = 0;

        // inserting the new free block in the free list
        insert_free_block(prev_block);

        return prev_block;
    
    } 

    // case 2: if only previous block is free, we coalesce with that
    else if (prev_block){
        // removing the free blocks from the free list
        remove_free_block(prev_block);
        remove_free_block(free_block);

        // setting the information about size and allocation in the new free block
        prev_block->size = prev_block->size + free_block->size;
        prev_block->allocated = 0;
        set_next_block_prev_alloc((block_t *)prev_block, 0);

        // setting the footer for the new free block
        free_block_footer *new_footer = get_free_footer(prev_block);
        new_footer->size = prev_block->size;
        new_footer->allocated = 0;

        // inserting the new free block in the free list
        insert_free_block(prev_block);

        return prev_block;
    }  

    // case 3: if only next block is free, we coalesce with that
    else if (next_block){
        // removing the free blocks from the free list
        remove_free_block(free_block);
        remove_free_block(next_block);

        // setting the information about size and allocation in the new free block
        free_block->size = free_block->size + next_block->size;
        free_block->allocated = 0;
        set_next_block_prev_alloc((block_t *)free_block, 0);

        // setting the footer for the new free block
        free_block_footer *new_footer = get_free_footer(free_block);
        new_footer->size = free_block->size;
        new_footer->allocated = 0;

        // inserting the new free block in the free list
        insert_free_block(free_block);

        return free_block;
    }

    return free_block; 
}

// best fit algorithm
static free_block_start *best_fit(size_t aligned_size) {
    int start_index = get_seg_list_index(aligned_size);
    free_block_start *best_block = NULL;  // Track the best-fitting block

    // iterating through the bin
    for (int i = start_index; i<get_num_lists(); i++) {
        free_block_start *curr = segregated_free_lists[i];

        while (curr) {
            if (curr->size>=aligned_size) {
                // if it's the first valid block found or it's a better block (meaning a smaller block)
                if (!best_block || curr->size<best_block->size) {
                    best_block = curr;
                }
                
                // if exact match is found, then we return that free block
                if (curr->size==aligned_size) {
                    return curr;
                }
            }
            curr = curr->next;
        }
        // returning the free block
        if (best_block) {
            return best_block;
        }
    }
    return NULL;  // No suitable block found
}

/*
 * mm_init: returns false on error, true on success.
 */
bool mm_init(void)
{
    // IMPLEMENT THIS

    // initializing segregated free list heads
    for (int i = 0; i < get_num_lists(); i++) {
        segregated_free_lists[i] = NULL;
    }

    // allocating the initial heap
    // 4 blocks worth of space for the alignment, prologue header, prologue footer, epilogue
    heap_start = mm_sbrk(4*sizeof(block_t));

    // if the allocation of memory fails
    if (heap_start == (void *)-1) {
        return false;
    }

    // setting alignment
    block_t *alignment = (block_t *)heap_start;
    alignment->size = sizeof(block_t);
    alignment->allocated = 1;
    alignment->prev_alloc = 1;

    // setting prologue header
    block_t *prologue_header = (block_t *)((char *)heap_start+sizeof(block_t));
    prologue_header->size = sizeof(block_t);
    prologue_header->allocated = 1;
    prologue_header->prev_alloc = 1;

    // setting prologue footer
    block_t *prologue_footer = (block_t *)((char *)heap_start+2*sizeof(block_t));
    prologue_footer->size = sizeof(block_t);
    prologue_footer->allocated = 1;
    prologue_footer->prev_alloc = 1;

    // setting epilogue
    block_t *epilogue = (block_t *)((char *)heap_start+3*sizeof(block_t));
    epilogue->size = 0; // setting size to 0, to mark end of the heap
    epilogue->allocated = 1;
    epilogue->prev_alloc = 1;

    return true;
}


/*
 * malloc
 */
void* malloc(size_t size)
{
    // IMPLEMENT THIS

    if (size == 0){
        return NULL;
    }

    size_t aligned_block_size = aligned_allocated_size(size); // returns the size of (header + size of payload + footer + (optional padding due to alignment))
    
    // checking if the minimum size if 32, if it is, we set it to be 32 (this is to ensure enough space for the free blocks)
    if (aligned_block_size < 32){
        aligned_block_size = 32;
    }    

    // getting the free block
    free_block_start *free_block = best_fit(aligned_block_size);

    if (free_block != NULL){
        // Splitting the block
        // additionally, the helper function also returns the allocated block
        allocated_header *allocated_block = split_block(free_block, aligned_block_size);    
        set_next_block_prev_alloc(allocated_block, 1); // setting the next block's prev_alloc bit

        // returning the pointer to the start of the payload
        return (void *)((char *)allocated_block + sizeof(allocated_header));
    }
    
    // If no suitable free block found, we extend the heap
    void *block_pointer = mm_sbrk(aligned_block_size);

    // cannot extend heap
    if (block_pointer == (void *)-1){
        return NULL;
    }

    block_t *epilogue = (block_t *)((char *)block_pointer - sizeof(block_t)); // getting the original epilogue location
    block_t *new_block_position = move_epilogue(epilogue, aligned_block_size); // moving the epilogue to the end

    // setting new block header
    allocated_header *new_header = (allocated_header *)new_block_position;
    new_header->size = aligned_block_size;
    new_header->allocated = 1;
    set_next_block_prev_alloc(new_header, 1);

    return (void *)((char *)new_header + sizeof(allocated_header)); // returning the starting address where the payload can sit
}

/*
 * free
 */
void free(void* ptr)
{
    // IMPLEMENT THIS
    if (ptr == NULL){
        return; // does nothing if ptr is NULL
    }

    // get the header of the allocated block
    allocated_header *header_block = (allocated_header *)((char *)ptr-sizeof(allocated_header));
    
    // getting the size information which will be used to set the size info in the free block
    size_t free_block_size = header_block->size;

    // typecasting header block to be free block header
    free_block_start *free_block_header = (free_block_start *)header_block;

    free_block_header->size = free_block_size;
    free_block_header->allocated = 0; // indicating block is free now
    set_next_block_prev_alloc((block_t *)free_block_header, 0);

    free_block_footer *free_footer = get_free_footer(free_block_header);
    free_footer->size = free_block_size;
    free_footer->allocated = 0; // indicating block is free now

    insert_free_block(free_block_header); // inserting free block into the free list

    coalesce(free_block_header); // coalescing with neighboring blocks

    return;
}

/*
 * realloc
 */
void* realloc(void* oldptr, size_t size)
{
    // IMPLEMENT THIS
 
    // if oldptr is NULL, we just malloc
    if (oldptr == NULL) {
        return malloc(size);
    }

    // if new size is 0 we free the block and return NULL.
    if (size == 0) {
        free(oldptr);
        return NULL;
    }

    // getting the header of the old block.
    allocated_header *old_header = (allocated_header *)((char *)oldptr - sizeof(allocated_header));
    size_t old_size = old_header->size;  // the size info includes header and footer
    
    // finding the new aligned block size
    size_t new_alloc_size = aligned_allocated_size(size);

    // if the new size is less than or equal to the old block size, we can keep the block as it is
    if (new_alloc_size <= old_size) {
        // returning the same pointer.
        return oldptr;
    }

    // if size is greater, we allocate a new blcok
    void *newptr = malloc(size);

    // if malloc fails, we return NULL
    if (newptr == NULL) {
        return NULL;
    }
    
    // copying the payload from the old block to the new block.
    size_t copy_size = old_size - sizeof(allocated_header);
    if (copy_size > size)
        copy_size = size;
    memcpy(newptr, oldptr, copy_size);
    
    // freeing the old block
    free(oldptr);
    
    return newptr;
}

/*
 * calloc
 * This function is not tested by mdriver, and has been implemented for you.
 */
void* calloc(size_t nmemb, size_t size)
{
    void* ptr;
    size *= nmemb;
    ptr = malloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

/*
 * Returns whether the pointer is in the heap.
 * May be useful for debugging.
 */
static bool in_heap(const void* p)
{
    return p <= mm_heap_hi() && p >= mm_heap_lo();
}

/*
 * Returns whether the pointer is aligned.
 * May be useful for debugging.
 */
static bool aligned(const void* p)
{
    size_t ip = (size_t) p;
    return align(ip) == ip;
}

/*
 * mm_checkheap
 * You call the function via mm_checkheap(__LINE__)
 * The line number can be used to print the line number of the calling
 * function where there was an invalid heap.
 */
bool mm_checkheap(int line_number)
{
#ifdef DEBUG
    // Write code to check heap invariants here
    // IMPLEMENT THIS
#endif // DEBUG
    return true;
}
