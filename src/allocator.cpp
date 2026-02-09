#include <iostream>
#include <cstddef>
#include <unistd.h>

const size_t ALIGNMENT = 16; // Alignment size
const size_t PROLOGUE_SIZE = 32; // Prologue Size

void* heap_start;

// block header
struct block_header {
    size_t size_and_alloc_status;
};
block_header* epilogue_ptr = nullptr; // pointer to epilogue

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

// setting heap extension size to be 4MB
const size_t EXTEND_SIZE = aligned_size(1024 * 4096);

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
    // updating header
    if (alloc_status){
        blk->size_and_alloc_status |= 0x1;
    }
    else {
        blk->size_and_alloc_status &= ~0x1;
    }

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
    } else {
    // case 2: removing from middle or end
        blk_payload->prev->next = blk_payload->next;
        if (blk_payload->next != nullptr) {
            blk_payload->next->prev = blk_payload->prev;
        }
    }

    // clearing pointers to prevent accidental reuse during coalescing
    blk_payload->prev = nullptr;
    blk_payload->next = nullptr;
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
    // initialize prologue (header + footer + padding coz of alignment)
    block_header* prologue = (block_header *) heap_start;
    setBlockSize(prologue, PROLOGUE_SIZE);
    setAllocStatus(prologue, 1);

    // initializing free block
    block_header* free_blk = (block_header *)((char *)heap_start + PROLOGUE_SIZE);
    setBlockSize(free_blk, free_space);
    setAllocStatus(free_blk, 0);
    // initializing the free block's prev and next pointers
    free_block_payload* payload = (free_block_payload *)((char *)free_blk + sizeof(block_header));
    payload->prev = nullptr;
    payload->next = nullptr;
    free_list_head = payload; // adding free block to the free list

    // initialize epilogue
    epilogue_ptr = (block_header *)((char *)free_blk + free_space);
    setBlockSize(epilogue_ptr, 0);
    setAllocStatus(epilogue_ptr, 1);
}


// initializing the heap using sbrk
void initialize_heap(){

    size_t epilogue_size = sizeof(block_header);

    size_t total_size = aligned_size(PROLOGUE_SIZE + EXTEND_SIZE + epilogue_size);

    void* result = sbrk(total_size);
    if (result == (void *) -1){
        std::cerr << "Error initializing heap" << std::endl;
        exit(-1);
    }

    heap_start = result;
    initializePrologueAndEpilogue(EXTEND_SIZE);
}

// splitting the free block
void splitBlock(block_header* free_blk_hdr, size_t size_required){

    // getting current free block's size
    size_t free_blk_size = getBlockSize(free_blk_hdr);

    // minimum required size of a free block for splitting
    // header + free_block_payload + footer
    const size_t MIN_FREE_BLOCK_SIZE =
        aligned_size(
                    sizeof(block_header) +
                    sizeof(free_block_payload) +
                    sizeof(block_header)
                    );

    // calculating remaining size
    size_t remaining_size = free_blk_size - size_required;

    // checking if remaining size left is suitable to create a free block
    if (remaining_size >= MIN_FREE_BLOCK_SIZE){
        // yes remaining size is enough to create a free block

        // setting our current free block (future allocated) header to have the required size info
        setBlockSize(free_blk_hdr, size_required);

        // creating a new free block after required size amount of space
        block_header* new_free_block = (block_header *)((char *)free_blk_hdr + size_required);

        // setting new free block's size
        setBlockSize(new_free_block, remaining_size);

        // setting new free block's allocation status to be free
        setAllocStatus(new_free_block, 0);

        // adding new free block to the free list
        addBlockToFreeList(new_free_block);
    } else {
        // This ensures the footer is placed at the very end of the physical block.
        size_required = free_blk_size;
        setBlockSize(free_blk_hdr, size_required);
    }

    // setting the allocation status of the current free block to be 1
    setAllocStatus(free_blk_hdr, 1);
}

// coalescing
block_header* coalesce(block_header* free_blk){

    size_t free_blk_size = getBlockSize(free_blk); // getting current free block size

    // coalescing prev

    // only looking back if there is space for a footer after the prologue (32 bytes)
    if ((char*)free_blk > (char*)heap_start + PROLOGUE_SIZE) {
        // accessing the previous block's footer
        block_header* prev_block_footer = (block_header *)((char *)(free_blk) - sizeof(block_header));

        if (getAllocStatus(prev_block_footer) == 0){

            size_t prev_block_size = getBlockSize(prev_block_footer); // getting previous block's size

            // verifying size is alright to avoid jumping to a random address
            if (prev_block_size > 0) {
                block_header* prev_block = (block_header *)((char *)free_blk - prev_block_size);

                // removing previous free block from the free list
                removeBlockFromFreeList(prev_block);

                // setting the prev block size to include the new combined size
                size_t new_size = prev_block_size + free_blk_size;
                setBlockSize(prev_block, new_size);

                // free_blk becomes prev_blk
                free_blk = prev_block;

                // updating free_blk_size now to possibly use it for coalescing with next block
                free_blk_size = new_size;
            }
        }
    }

    // coalescing next

    // accessing the next block's header
    block_header* next_block_header = (block_header *)((char *)(free_blk) + free_blk_size);

    // calculating next block's size
    size_t next_block_size = getBlockSize(next_block_header);

    if (next_block_size > 0 && getAllocStatus(next_block_header) == 0){
        // valid free block
        // removing block from free list
        removeBlockFromFreeList(next_block_header);

        // setting the current free block size to update size info to include next block size as well
        size_t new_size = free_blk_size + next_block_size;

        // setting the new block size
        setBlockSize(free_blk, new_size);
    }

    return free_blk;
}



void moveEpilogue(size_t extend_size){
    // old epilogue
    block_header* old_epilogue = epilogue_ptr;

    // moving the epilogue pointer to the new end of the heap
    epilogue_ptr = (block_header *)((char *)old_epilogue + extend_size);
    setBlockSize(epilogue_ptr, 0);
    setAllocStatus(epilogue_ptr, 1);

    // converting old epilogue to a new free block
    setBlockSize(old_epilogue, extend_size);
    setAllocStatus(old_epilogue, 0);

    // coalesce and adding to free list
    block_header* final_free_blk = coalesce(old_epilogue);
    addBlockToFreeList(final_free_blk);
}

void extend_heap(size_t min_size){

    // minimum free block size
    // header + free_block_payload + footer
    const size_t MIN_FREE_BLOCK_SIZE =
        aligned_size(
                    sizeof(block_header) +
                    sizeof(free_block_payload) +
                    sizeof(block_header)
                    );

    size_t extend_size = aligned_size(min_size);

    // checking if extend size meets minimum size requirement
    if (extend_size < MIN_FREE_BLOCK_SIZE){
            extend_size = MIN_FREE_BLOCK_SIZE;
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
    // header + payload + footer
    size_t requested_total = size + (2 * sizeof(block_header));
    size_t new_size = aligned_size(requested_total);

    // minimum free block size
    // header + free_block_payload + footer
    const size_t MIN_FREE_BLOCK_SIZE =
        aligned_size(
                    sizeof(block_header) +
                    sizeof(free_block_payload) +
                    sizeof(block_header)
                    );

    // making sure it is free block size compatible
    if (new_size < MIN_FREE_BLOCK_SIZE){
        new_size = MIN_FREE_BLOCK_SIZE;
    }

    block_header* blk = (block_header*) first_fit(new_size);

    if (!blk){
        extend_heap(new_size);
        blk = (block_header*) first_fit(new_size);
        if (!blk) {
          return nullptr;
        }
    }

    // removing allocated block from the free list
    removeBlockFromFreeList(blk);

    // splitting the free block
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
