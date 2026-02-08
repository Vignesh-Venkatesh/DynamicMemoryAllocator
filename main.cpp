#include <cstddef>
#include <iostream>
#include <unistd.h>


const size_t ALIGNMENT = 16; // Alignment size
const size_t WORD_SIZE = 8; // Word size
const size_t DOUBLE_WORD_SIZE = 16; // Double word size

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

// -----------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------
// helper functions
// -----------------------------------------------------------------------------------

// helper function to get block's allocation status
bool getAllocStatus(block_header *blk){
    return blk->size_and_alloc_status & 0x1;
}

// helper function to set block's allocation status
void setAllocStatus(block_header *blk, bool alloc_status){
    // clearing the least significant bit (allocation bit)
    blk->size_and_alloc_status = blk->size_and_alloc_status & ~0x1;

    // setting allocation bit
    blk->size_and_alloc_status = blk->size_and_alloc_status | (alloc_status ? 0x1 : 0x0);
}

// helper function to get block's size
size_t getBlockSize(block_header *blk){
    return blk->size_and_alloc_status & ~(ALIGNMENT-1);
}

// helper function to set block's size
void setBlockSize(block_header *blk, size_t size) {
    size_t flags = blk->size_and_alloc_status & (ALIGNMENT-1);
    blk->size_and_alloc_status = (aligned_size(size)) | flags;
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

    while (getBlockSize(blk)!=0){
        printBlockInfo(blk);
        size_t size_of_block = getBlockSize(blk);
        blk = (block_header*)((char *)blk + size_of_block);
    }
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


void initializePrologueAndEpilogue(size_t free_space){
    // initialize prologue
    block_header* prologue = (block_header *) heap_start;
    setAllocStatus(prologue, 1);
    setBlockSize(prologue, aligned_size(sizeof(block_header)));

    // initializing free block
    block_header* free_blk = (block_header *)((char *)heap_start + aligned_size(sizeof(block_header)));
    setAllocStatus(free_blk, 0);
    setBlockSize(free_blk, free_space);
    // initializing the free block's prev and next pointers
    free_block_payload* payload = (free_block_payload *)((char *)free_blk + sizeof(block_header));
    payload->prev = nullptr;
    payload->next = nullptr;
    free_list_head = payload; // adding free block to the free list

    // initialize epilogue
    block_header* epilogue = (block_header *)((char *)heap_start + aligned_size(sizeof(block_header)) + free_space);
    setAllocStatus(epilogue, 1);
    setBlockSize(epilogue, 0);
}


// initializing the heap using sbrk
void initialize_heap(){
    // 2 blocks for prologue and epilogue and rest for free blocks
    size_t free_space = 1024;
    void* result = sbrk((2*sizeof(block_header))+(free_space));
    if (result == (void *) -1){
        std::cerr << "Error initializing heap" << std::endl;
        exit(-1);
    } else {
        heap_start = result;
        initializePrologueAndEpilogue(free_space);
        return;
    }
}

void* memory_alloc(size_t size){

    // if size is 0
    if (size == 0){
        return nullptr;
    }

    // aligned block size
    size_t new_size = aligned_size(size);

    // since total free block size is 32 bytes,
    // we check if the size is 32 bytes
    // if not, we set the new size to be 32 bytes
    if (new_size<(sizeof(block_header)+sizeof(free_block_payload))){
        new_size = aligned_size((sizeof(block_header)+sizeof(free_block_payload)));
    }

    return first_fit(new_size);

}


int main(){
    initialize_heap(); // initializing heap with prologue and epilogue

    printAllBlocks();
    std::cout << memory_alloc(32) << std::endl;

    return 0;
}
