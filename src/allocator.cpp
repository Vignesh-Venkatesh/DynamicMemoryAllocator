#include <iostream>
#include <cstddef>
#include <unistd.h>


const size_t ALIGNMENT = 16; // Alignment size

void* heap_start;

// block header
struct block_header {
    size_t size_and_alloc_status;
};

// free block payload - contains pointer to the previous block and the next block
struct free_block_payload {
    free_block_payload* prev;
    free_block_payload* next;
};

free_block_payload* free_list_head = nullptr; // explicit free list

// -----------------------------------------------------------------------------------
// utility functions
// -----------------------------------------------------------------------------------

size_t aligned_size(size_t size){
    return ALIGNMENT*((size+ALIGNMENT-1)/ALIGNMENT);
}

const size_t EXTEND_SIZE = aligned_size(1024);

// -----------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------
// helper functions
// -----------------------------------------------------------------------------------

// helper function to get block's allocation status
bool getAllocStatus(block_header *blk){
    return blk->size_and_alloc_status & 0x1;
}

// helper function to get block's size
size_t getBlockSize(block_header *blk){
    return blk->size_and_alloc_status & ~(ALIGNMENT-1);
}

// helper function to set block's size
void setBlockSize(block_header *blk, size_t size) {
    size_t flags = blk->size_and_alloc_status & (ALIGNMENT-1);
    blk->size_and_alloc_status = size | flags;

    if (size == 0) return; // epilogue, no footer

    // footer details
    block_header* footer = (block_header*)((char*)blk + size - sizeof(block_header));
    footer->size_and_alloc_status = blk->size_and_alloc_status;
}

// helper function to set block's allocation status
void setAllocStatus(block_header *blk, bool alloc_status){
    // clearing the least significant bit (allocation bit)
    blk->size_and_alloc_status = blk->size_and_alloc_status & ~0x1;

    // setting allocation bit
    blk->size_and_alloc_status = blk->size_and_alloc_status | (alloc_status ? 0x1 : 0x0);

    // footer details
    size_t block_size = getBlockSize(blk);
    if (block_size == 0) return; // epilogue, no footer
    block_header* footer = (block_header*)((char*)blk + block_size - sizeof(block_header));
    footer->size_and_alloc_status = blk->size_and_alloc_status;
}

// -----------------------------------------------------------------------------------
// for debugging
// -----------------------------------------------------------------------------------

void printBlockInfo(block_header *blk, std::string blk_name = "Block"){
    std::cout << blk_name << " starting address: " << (void *) blk << std::endl;
    std::cout << "Allocation Status: " << getAllocStatus(blk) << std::endl;
    std::cout << "Size: " << getBlockSize(blk) << std::endl;
    std::cout << std::endl;
}

void printAllBlocks(){
    block_header* blk = (block_header *) (char *)(heap_start);
    std::cout << "===================================================================" << std::endl;
    while (getBlockSize(blk)!=0){
        printBlockInfo(blk);
        size_t size_of_block = getBlockSize(blk);
        blk = (block_header*)((char *)blk + size_of_block);
    }
    printBlockInfo(blk, "epilogue");
    std::cout << "===================================================================" << std::endl;
}
// -----------------------------------------------------------------------------------


// -----------------------------------------------------------------------------------
// first fit algorithm for finding free blocks
void* first_fit(size_t size){
    // creating a temp payload variable to iterate over the free list
    free_block_payload* payload = free_list_head;

    while (payload){
        // getting free block's header
        block_header* free_blk_header = (block_header *)((char *)payload - sizeof(block_header));

        // checking if enough space in free block
        if (getBlockSize(free_blk_header)>=size){
            return free_blk_header;
        }

        // else, we keep iterating over the free list
        payload = payload->next;
    }

    return nullptr; // if no free block is found
}

// removing block from free list
void removeBlockFromFreeList(block_header *blk){
    free_block_payload* blk_payload = (free_block_payload *)((char *)blk + sizeof(block_header));

    // case 1: removing the head of the list
    if (blk_payload->prev == nullptr) {
        free_list_head = blk_payload->next;
        if (blk_payload->next != nullptr) {
            blk_payload->next->prev = nullptr;
        }
        return;
    }

    // case 2: removing from middle or end
    blk_payload->prev->next = blk_payload->next;
    if (blk_payload->next != nullptr) {
        blk_payload->next->prev = blk_payload->prev;
    }
}

// adding block to free list
void addBlockToFreeList(block_header *blk_hdr){
    // setting the free block to be at the beginning of the freelist
    free_block_payload* payload = (free_block_payload *)((char *)blk_hdr + sizeof(block_header));
    payload->prev = nullptr;
    payload->next = free_list_head;

    if (free_list_head != nullptr) {
        free_list_head->prev = payload;
    }

    free_list_head = payload;
}

// initializing prologue, free block and epilogue
void initializePrologueAndEpilogue(size_t free_space){
    // initialize prologue
    block_header* prologue = (block_header *) heap_start;
    setBlockSize(prologue, aligned_size(sizeof(block_header)));
    setAllocStatus(prologue, 1);

    // initializing free block
    block_header* free_blk = (block_header *)((char *)heap_start + aligned_size(sizeof(block_header)));
    setBlockSize(free_blk, free_space);
    setAllocStatus(free_blk, 0);
    // initializing the free block's prev and next pointers
    free_block_payload* payload = (free_block_payload *)((char *)free_blk + sizeof(block_header));
    payload->prev = nullptr;
    payload->next = nullptr;
    free_list_head = payload; // adding free block to the free list

    // initialize epilogue
    block_header* epilogue = (block_header *)((char *)heap_start + aligned_size(sizeof(block_header)) + free_space);
    setBlockSize(epilogue, 0);
    setAllocStatus(epilogue, 1);
}


// initializing the heap using sbrk
void initialize_heap(){

    size_t total_size =
           aligned_size(sizeof(block_header))  // prologue
         + EXTEND_SIZE                         // free block
         + sizeof(block_header);               // epilogue
    void* result = sbrk(total_size);

    if (result == (void *) -1){
        std::cerr << "Error initializing heap" << std::endl;
        exit(-1);
    } else {
        heap_start = result;
        initializePrologueAndEpilogue(EXTEND_SIZE);
        return;
    }
}

// splitting the free block
void splitBlock(block_header* blk_hdr, size_t required_size){
    size_t block_size = getBlockSize(blk_hdr); // getting current free block size
    const size_t min_free_block_size_for_splitting = aligned_size(sizeof(block_header) + sizeof(free_block_payload) + sizeof(block_header));
    size_t remaining_size = block_size - required_size; // calculating remaining size

    // split the block if remaining size is greater than minimum size required for splitting
    // minimum size required is the aligned size of two free blocks
    if (remaining_size>=min_free_block_size_for_splitting){
        // setting block size
        setBlockSize(blk_hdr, required_size);

        // creating new free block and adding it to the free list
        block_header* new_free_blk_hdr = (block_header *)((char *)blk_hdr + required_size);
        setBlockSize(new_free_blk_hdr, remaining_size);
        setAllocStatus(new_free_blk_hdr, 0);
        addBlockToFreeList(new_free_blk_hdr);
    }

    setAllocStatus(blk_hdr, 1);

    return;
}

// coalescing
block_header* coalesce(block_header* free_blk){

    size_t free_blk_size = getBlockSize(free_blk); // getting current free block size

    // coalescing prev
    block_header* prev_block_footer = (block_header *)((char *)(free_blk) - sizeof(block_header)); // accessing previous block's footer

    if (getAllocStatus(prev_block_footer) == 0){ // if its a free block
        size_t prev_free_block_size = getBlockSize(prev_block_footer); // getting previous free block's size
        block_header* prev_free_block = (block_header *)((char *)(free_blk) - prev_free_block_size); // accessing the prev free block's header

        removeBlockFromFreeList(prev_free_block); // removing previous free block from the free list
        setBlockSize(prev_free_block, free_blk_size + prev_free_block_size); // setting the block size field of the previous free block to contain the current block + its (prev) old size

        free_blk = prev_free_block; // setting the pointer of the free block to the previous free block
        free_blk_size = getBlockSize(free_blk); // updating the size field
    }

    // coalescing next
    block_header* next_block = (block_header *)((char *)(free_blk) + free_blk_size); // getting next block's header

    if (getAllocStatus(next_block) == 0){ // if its a free block
        size_t next_block_size = getBlockSize(next_block); // getting the size of the next free block

        removeBlockFromFreeList(next_block); // removing the next block from the free list
        setBlockSize(free_blk, free_blk_size + next_block_size); // updating the current block's size to include the next free block's size
    }

    return free_blk;
}

void moveEpilogue(size_t new_heap_size){
    // getting old epilogue
    block_header* epilogue = (block_header *) (char *)(heap_start);
    while (getBlockSize(epilogue)!=0){
        size_t size_of_block = getBlockSize(epilogue);
        epilogue = (block_header*)((char *)epilogue + size_of_block);
    }

    // copying epilogue info
    block_header* new_free_blk = epilogue;

    // moving epilogue to end of the new free space
    epilogue = (block_header *)((char *)epilogue + new_heap_size);
    setBlockSize(epilogue, 0);
    setAllocStatus(epilogue, 1);

    // setting new free block allocation and size
    setBlockSize(new_free_blk, new_heap_size);
    setAllocStatus(new_free_blk, 0);

    // coalescing just in case
    new_free_blk = coalesce(new_free_blk);

    // adding new free block to the free list
    addBlockToFreeList(new_free_blk);


}

void extend_heap(size_t min_size){

    const size_t MIN_BLOCK_SIZE =
            aligned_size(sizeof(block_header) // free block header
                        + sizeof(free_block_payload) // free block payload
                        + sizeof(block_header)); // free block footer

    size_t extend_size = min_size;

    // checking if extend size meets minimum size requirement
    if (extend_size < MIN_BLOCK_SIZE){
            extend_size = MIN_BLOCK_SIZE;
    }

    // checking if extend_size is more than the minimum memory required for heap extension
    if (extend_size < EXTEND_SIZE){
        extend_size = EXTEND_SIZE;
    }

    void* new_heap = sbrk(extend_size); // extending the heap
    if (new_heap == (void *) -1){
        std::cerr << "Error extending heap" << std::endl;
        exit(-1);
    } else {
        moveEpilogue(extend_size); // moving epilogue
        return;
    }
}


// malloc
void* memory_alloc(size_t size){

    // if size is 0
    if (size == 0){
        return nullptr;
    }

    // aligned block size
    size_t new_size = aligned_size(size + (2 * sizeof(block_header))); // header + size + footer
    const size_t MIN_BLOCK_SIZE =
        aligned_size(sizeof(block_header) // free block header
                    + sizeof(free_block_payload) // free block payload (next and prev pointers)
                    + sizeof(block_header)); // free block footer

    // making sure it is free block size compatible
    // since total free block size is 32 bytes,
    // we check if the size is 32 bytes
    // if not, we set the new size to be 32 bytes
    if (new_size < MIN_BLOCK_SIZE){
        new_size = MIN_BLOCK_SIZE;
    }


    block_header* blk = (block_header*) first_fit(new_size);

    if (!blk){
        extend_heap(new_size);
        blk = (block_header*) first_fit(new_size);
        if (!blk) {
          return nullptr;
        }
    }

    removeBlockFromFreeList(blk);
    splitBlock(blk, new_size);

    return (char*)blk + sizeof(block_header);
}

// free
void memory_free(void* blk){

    if (blk == nullptr){
        std::cerr << "[memory_free] Warning: attempting to free nullptr\n";
        return;
    }

    // getting the block header
    block_header* blk_hdr = (block_header *) ((char *)blk - sizeof(block_header));

    // marking the block free
    setAllocStatus(blk_hdr, 0);

    // coalescing
    blk_hdr = coalesce(blk_hdr);

    // adding back to free list
    addBlockToFreeList(blk_hdr);
}
