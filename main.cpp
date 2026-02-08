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

// helper function to get block's size
size_t getBlockSize(block_header *blk){
    return blk->size_and_alloc_status & ~(ALIGNMENT-1);
}

// helper function to set block's size
void setBlockSize(block_header *blk, size_t size) {
    size_t flags = blk->size_and_alloc_status & (ALIGNMENT-1);
    blk->size_and_alloc_status = (aligned_size(size)) | flags;

    // footer details
    block_header* footer = (block_header*)((char*)blk + aligned_size(size) - sizeof(block_header));
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

// splitting the free block
void splitBlock(block_header* blk_hdr, size_t required_size){
    size_t block_size = getBlockSize(blk_hdr); // getting current free block size
    const size_t min_free_block_size_for_splitting = 2*aligned_size((sizeof(block_header)+sizeof(free_block_payload)));
    size_t remaining_size = aligned_size(block_size - required_size); // calculating remaining size

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

    return;
}

// coalescing
void coalesce(block_header* free_blk){

    size_t free_blk_size = getBlockSize(free_blk); // getting current free block size

    // coalescing prev
    block_header* prev_block_footer = (block_header *)((char *)(free_blk)-sizeof(block_header)); // accessing previous block's footer
    if (getAllocStatus(prev_block_footer) == 0){ // if its a free block
        size_t prev_free_block_size = getBlockSize(prev_block_footer); // getting previous free block's size
        block_header* prev_free_block = (block_header *)((char *)(free_blk)-prev_free_block_size); // accessing the prev free block's header
        removeBlockFromFreeList(prev_free_block); // removing previous free block from the free list
        setBlockSize(prev_free_block, free_blk_size+prev_free_block_size); // setting the block size field of the previous free block to contain the current block + its (prev) old size
        free_blk = prev_free_block; // setting the pointer of the free block to the previous free block
        free_blk_size = getBlockSize(free_blk); // updating the size field
    }

    // coalescing next
    block_header* next_block = (block_header *)((char *)(free_blk)+free_blk_size); // getting next block's header
    if (getAllocStatus(next_block) == 0){ // if its a free block
        removeBlockFromFreeList(next_block); // removing the next block from the free list
        size_t next_block_size = getBlockSize(next_block); // getting the size of the next free block
        setBlockSize(free_blk, free_blk_size+next_block_size); // updating the current block's size to include the next free block's size
    }
}



// malloc
void* memory_alloc(size_t size){

    // if size is 0
    if (size == 0){
        return nullptr;
    }

    // aligned block size
    size_t new_size = aligned_size(size + sizeof(block_header));

    // making sure it is free block size compatible
    // since total free block size is 32 bytes,
    // we check if the size is 32 bytes
    // if not, we set the new size to be 32 bytes
    if (new_size<(sizeof(block_header)+sizeof(free_block_payload))){
        new_size = aligned_size((sizeof(block_header)+sizeof(free_block_payload)));
    }

    block_header* new_alloc_bloc = (block_header *)first_fit(new_size);

    // if free block exists
    if (new_alloc_bloc){
        removeBlockFromFreeList(new_alloc_bloc);
        splitBlock(new_alloc_bloc, new_size);
        setAllocStatus(new_alloc_bloc, 1);
        return (void *)((char *)(new_alloc_bloc) + sizeof(block_header));
    }

    else {
        // no free block found
        return nullptr; // for now setting it to nullptr, (will expand code later to include, increasing heap size)
    }

}

// free
void memory_free(void* blk){
    // getting the block header
    block_header* blk_hdr = (block_header *) ((char *)blk - sizeof(block_header));

    // marking the block free
    setAllocStatus(blk_hdr, 0);

    // coalescing
    coalesce(blk_hdr);

    // adding back to free list
    addBlockToFreeList(blk_hdr);
}


int main(){
    initialize_heap(); // initializing heap with prologue and epilogue

    // void* p1 = memory_alloc(32);
    // printAllBlocks();
    // std::cout << "p1: " << p1 << std::endl;
    // void* p2 = memory_alloc(64);
    // printAllBlocks();
    // std::cout << "p2: " << p2 << std::endl;
    // memory_free(p1);
    // printAllBlocks();
    // void* p3 = memory_alloc(32);
    // printAllBlocks();
    // std::cout << "p3: " << p3 << std::endl;

    // void* p1 = memory_alloc(32);
    // void* p2 = memory_alloc(64);
    // void* p3 = memory_alloc(32);
    // std::cout << "p1: " << p1 << std::endl;
    // std::cout << "p2: " << p2 << std::endl;
    // std::cout << "p3: " << p3 << std::endl;
    // printAllBlocks();
    // memory_free(p2);
    // memory_free(p3);
    // printAllBlocks();


    // // forward coalescing test
    // void* p1 = memory_alloc(32);
    // void* p2 = memory_alloc(64);
    // void* p3 = memory_alloc(32);
    // std::cout << "p1: " << p1 << std::endl;
    // std::cout << "p2: " << p2 << std::endl;
    // std::cout << "p3: " << p3 << std::endl;
    // printAllBlocks();
    // memory_free(p2);
    // memory_free(p3);
    // printAllBlocks();


    // // backward coalescing test
    // void* p1 = memory_alloc(32);
    // void* p2 = memory_alloc(64);
    // void* p3 = memory_alloc(32);
    // std::cout << "p1: " << p1 << std::endl;
    // std::cout << "p2: " << p2 << std::endl;
    // std::cout << "p3: " << p3 << std::endl;
    // printAllBlocks();
    // memory_free(p3);
    // memory_free(p2);
    // printAllBlocks();

    // // coalescing both sides test
    // void* p1 = memory_alloc(32);
    // void* p2 = memory_alloc(64);
    // void* p3 = memory_alloc(32);
    // void* p4 = memory_alloc(64);
    // std::cout << "p1: " << p1 << std::endl;
    // std::cout << "p2: " << p2 << std::endl;
    // std::cout << "p3: " << p3 << std::endl;
    // std::cout << "p4: " << p4 << std::endl;
    // printAllBlocks();
    // memory_free(p2);
    // memory_free(p4);
    // memory_free(p3);
    // printAllBlocks();

    // no coalescing test
    void* p1 = memory_alloc(32);
    void* p2 = memory_alloc(64);
    void* p3 = memory_alloc(32);
    std::cout << "p1: " << p1 << std::endl;
    std::cout << "p2: " << p2 << std::endl;
    std::cout << "p3: " << p3 << std::endl;
    printAllBlocks();
    memory_free(p1);
    memory_free(p3);
    printAllBlocks();

    return 0;
}
