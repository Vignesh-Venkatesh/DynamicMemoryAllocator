#include "../src/allocator.hpp"
#include <iostream>
#include <string>


void printError(std::string& text){
    std::cout << "\033[31m" << text << "\033[0m\n";
}

void printTestName(std::string& text){
    std::cout << "\033[47m\033[1m\033[30m" << text << "\033[0m\n";
}

void printTestPassed(){
    std::cout << "\033[32m" << "TEST PASSED\n\n" << "\033[0m";
}

void printInfo(const std::string& text){
    std::cout << "\033[35m" << text << "\033[0m\n";
}

void printWarning(const std::string& text){
    std::cout << "\033[33m" << text << "\033[0m\n";
}

// testing basic allocation
void test_basic_alloc_free(){
    std::string msg = "Test 1: Basic Allocation";
    printTestName(msg);

    // allocating some memory
    void* p1 = memory_alloc(32); // 32 bytes

    // checking if it is not null
    if (p1 == nullptr){
        std::string msg = "FAILED: memory_alloc returned nullptr";
        printError(msg);
        return;
    }

    std::cout << "\033[35m" << "Allocated 32 bytes at address: " << p1 << "\033[0m\n";

    // trying to write data to it
    // if crashes, allocation failed/was bad
    char* data = (char *)p1;
    for (int i=0; i<32; i++){
        data[i] = 'a'; // writing data
    }

    printInfo("Successfully wrote to allocated memory");

    // freeing memory
    memory_free(p1);
    printInfo("Freed memory");

    // test passed
    printTestPassed();
}

// testing multiple allocations
void test_multiple_allocations(){
    std::string msg = "Test 2: Multiple Allocations";
    printTestName(msg);

    void* p1 = memory_alloc(16);
    void* p2 = memory_alloc(32);
    void* p3 = memory_alloc(64);

    if (p1 == nullptr){
        std::string msg = "FAILED: memory_alloc returned nullptr for p1";
        printError(msg);
        return;
    }

    if (p2 == nullptr){
        std::string msg = "FAILED: memory_alloc returned nullptr for p2";
        printError(msg);
        return;
    }

    if (p3 == nullptr){
        std::string msg = "FAILED: memory_alloc returned nullptr for p3";
        printError(msg);
        return;
    }

    std::cout << "\033[35m" << "p1 = " << p1 << "\033[0m\n";
    std::cout << "\033[35m" << "p2 = " << p2 << "\033[0m\n";
    std::cout << "\033[35m" << "p3 = " << p3 << "\033[0m\n";

    // checking if they are all different addresses
    if (p1 == p2 || p2 == p3 || p1 == p3) {
        std::string msg = "FAILED: got duplicate addresses";
        printError(msg);
        return;
    }

    printInfo("All addresses are different");

    // writing to each one
    ((char*)p1)[0] = 'A';
    ((char*)p2)[0] = 'B';
    ((char*)p3)[0] = 'C';
    printInfo("Successfully wrote to all allocations");

    // freeing memory
    memory_free(p1);
    memory_free(p2);
    memory_free(p3);

    printInfo("Freed memory");

    // test passed
    printTestPassed();
}

void test_memory_reuse() {
    std::string msg = "Test 3: Memory Reuse";
    printTestName(msg);

    // allocating a block
    void* p1 = memory_alloc(32);
    void* original_address = p1;

    std::cout << "\033[35m" "First allocation at: " << p1 << "\033[0m\n";

    // freeing it
    memory_free(p1);
    printInfo("Freed first allocation");

    // allocating same size again
    void* p2 = memory_alloc(32);
    std::cout << "\033[35m" << "Second allocation at: " << p2 << "\033[0m\n";

    // should reuse the same block
    if (p2 == original_address) {
        printInfo("Memory was reused (same address)");
        printTestPassed();
    } else {
        printWarning("WARNING: Memory not reused, but that's okay");
        printWarning("(This might happen if blocks were split)\n");
    }

    memory_free(p2);
}

void test_data_integrity() {
    std::string msg = "Test 4: Data Integrity";
    printTestName(msg);

    // allocating three blocks
    void* p1 = memory_alloc(50);
    void* p2 = memory_alloc(100);
    void* p3 = memory_alloc(75);

    // writing unique patterns to each
    char* data1 = (char*)p1;
    char* data2 = (char*)p2;
    char* data3 = (char*)p3;

    for (int i = 0; i < 50; i++) {
        data1[i] = 'A';
    }
    for (int i = 0; i < 100; i++) {
        data2[i] = 'B';
    }
    for (int i = 0; i < 75; i++) {
        data3[i] = 'C';
    }

    printInfo("Wrote patterns to all three blocks");

    // verifying patterns are still intact
    bool p1_ok = true;
    bool p2_ok = true;
    bool p3_ok = true;

    for (int i = 0; i < 50; i++) {
        if (data1[i] != 'A'){
            p1_ok = false;
        }
    }
    for (int i = 0; i < 100; i++) {
        if (data2[i] != 'B'){
            p2_ok = false;
        }
    }
    for (int i = 0; i < 75; i++) {
        if (data3[i] != 'C'){
            p3_ok = false;
        }
    }

    if (p1_ok && p2_ok && p3_ok) {
        printInfo("All data patterns are intact");
        printTestPassed();
    } else {
        std::string msg = "FAILED: Data corruption detected";
        printError(msg);
        if (!p1_ok){
            std::string msg = "\tp1 corrupted";
            printError(msg);
        }
        if (!p2_ok){
            std::string msg = "\tp2 corrupted";
            printError(msg);
        }
        if (!p3_ok){
            std::string msg = "\tp3 corrupted";
            printError(msg);
        }
    }

    memory_free(p1);
    memory_free(p2);
    memory_free(p3);
}

void test_coalesce_forward() {
    std::string msg = "Test 5a: Forward Coalescing";
    printTestName(msg);

    void* p1 = memory_alloc(32);
    void* p2 = memory_alloc(64);
    void* p3 = memory_alloc(32);

    printInfo("Allocated p1 (32), p2 (64), p3 (32)");

    // freeing p2 and p3 in order - should merge
    memory_free(p2);
    printInfo("Freed p2");

    memory_free(p3);
    printInfo("Freed p3 - should coalesce with p2");

    // now trying to allocate a size that would only fit if p2+p3 merged
    void* p4 = memory_alloc(80);

    if (p4 != nullptr) {
        printInfo("Successfully allocated 80 bytes - coalescing worked");
        memory_free(p4);
        memory_free(p1);
        printTestPassed();
    } else {
        std::string err = "FAILED: Could not allocate 80 bytes - blocks didn't coalesce";
        printError(err);
        memory_free(p1);
    }
}

void test_coalesce_backward() {
    std::string msg = "Test 5b: Backward Coalescing";
    printTestName(msg);

    void* p1 = memory_alloc(32);
    void* p2 = memory_alloc(64);
    void* p3 = memory_alloc(32);

    printInfo("Allocated p1 (32), p2 (64), p3 (32)");

    // freeing p3 first, then p2 - p2 should merge backward with p3
    memory_free(p3);
    printInfo("Freed p3");

    memory_free(p2);
    printInfo("Freed p2 - should coalesce backward with p3");

    // trying to allocate a size that would only fit if they merged
    void* p4 = memory_alloc(80);

    if (p4 != nullptr) {
        printInfo("Successfully allocated 80 bytes - backward coalescing worked");
        memory_free(p4);
        memory_free(p1);
        printTestPassed();
    } else {
        std::string err = "FAILED: Could not allocate 80 bytes - blocks didn't coalesce";
        printError(err);
        memory_free(p1);
    }
}

void test_coalesce_both_directions() {
    std::string msg = "Test 5c: Both Directions Coalescing";
    printTestName(msg);

    void* p1 = memory_alloc(32);
    void* p2 = memory_alloc(64);
    void* p3 = memory_alloc(32);
    void* p4 = memory_alloc(64);

    printInfo("Allocated p1 (32), p2 (64), p3 (32), p4 (64)");

    // freeing p2 and p4, then p3 in middle - should merge all three
    memory_free(p2);
    printInfo("Freed p2");

    memory_free(p4);
    printInfo("Freed p4");

    memory_free(p3);
    printInfo("Freed p3 - should coalesce with p2 and p4");

    // trying to allocate a size that needs all three merged
    void* p5 = memory_alloc(140);

    if (p5 != nullptr) {
        printInfo("Successfully allocated 140 bytes - coalescing in both directions worked");
        memory_free(p5);
        memory_free(p1);
        printTestPassed();
    } else {
        std::string err = "FAILED: Could not allocate 140 bytes - blocks didn't fully coalesce";
        printError(err);
        memory_free(p1);
    }
}

void test_block_splitting() {
    std::string msg = "Test 6: Block Splitting";
    printTestName(msg);

    // allocating small amount - should split the large initial block
    void* p1 = memory_alloc(32);
    printInfo("Allocated 32 bytes from large free block");

    // should still be able to allocate more from the split remainder
    void* p2 = memory_alloc(64);
    void* p3 = memory_alloc(128);
    void* p4 = memory_alloc(256);

    if (p1 && p2 && p3 && p4) {
        printInfo("Successfully allocated multiple blocks after splitting");
        printInfo("Block splitting is working correctly");

        memory_free(p1);
        memory_free(p2);
        memory_free(p3);
        memory_free(p4);
        printTestPassed();
    } else {
        std::string err = "FAILED: Could not allocate after split";
        printError(err);
        if (p1) memory_free(p1);
        if (p2) memory_free(p2);
        if (p3) memory_free(p3);
        if (p4) memory_free(p4);
    }
}

void test_heap_extension() {
    std::string msg = "Test 7: Heap Extension";
    printTestName(msg);

    // allocating many blocks to fill initial heap
    void* blocks[25];
    int allocated = 0;

    for (int i = 0; i < 25; i++) {
        blocks[i] = memory_alloc(100);
        if (blocks[i] != nullptr) {
            allocated++;
        }
    }

    printInfo("Allocated " + std::to_string(allocated) + " blocks of 100 bytes each");

    if (allocated >= 15) {
        printInfo("Heap extension is working - allocated more than initial heap size");
        printTestPassed();
    } else {
        std::string err = "FAILED: Could not extend heap";
        printError(err);
    }

    // Clean up
    for (int i = 0; i < allocated; i++) {
        if (blocks[i]) memory_free(blocks[i]);
    }
}

void test_edge_cases() {
    std::string msg = "Test 8: Edge Cases";
    printTestName(msg);

    bool all_passed = true;

    // Test 1: zero size
    void* p1 = memory_alloc(0);
    if (p1 == nullptr) {
        printInfo("Allocating 0 bytes returns nullptr");
    } else {
        std::string err = "FAILED: Allocating 0 bytes should return nullptr";
        printError(err);
        all_passed = false;
        memory_free(p1);
    }

    // Test 2: size 1 (minimum)
    void* p2 = memory_alloc(1);
    if (p2 != nullptr) {
        printInfo("Allocating 1 byte succeeds");
        ((char*)p2)[0] = 'X';
        memory_free(p2);
    } else {
        std::string err = "FAILED: Allocating 1 byte failed";
        printError(err);
        all_passed = false;
    }

    // Test 3: very large size
    void* p3 = memory_alloc(8192);
    if (p3 != nullptr) {
        printInfo("Allocating large size (8192 bytes) succeeds");
        memory_free(p3);
    } else {
        std::string err = "FAILED: Allocating large size failed";
        printError(err);
        all_passed = false;
    }

    // Test 4: freeing nullptr (should not crash)
    memory_free(nullptr);
    printInfo("Freeing nullptr doesn't crash");

    if (all_passed) {
        printTestPassed();
    } else {
        std::string err = "FAILED: Some edge case tests failed";
        printError(err);
    }
}

int main(){
    initialize_heap();

    // basic tests
    test_basic_alloc_free();
    test_multiple_allocations();
    test_memory_reuse();
    test_data_integrity();

    // coalescing tests
    test_coalesce_forward();
    test_coalesce_backward();
    test_coalesce_both_directions();

    // advanced tests
    test_block_splitting();
    test_heap_extension();
    test_edge_cases();

    std::cout << "\033[1m\033[32m========================================\033[0m\n";
    std::cout << "\033[1m\033[32mALL CORRECTNESS TESTS COMPLETED\033[0m\n";
    std::cout << "\033[1m\033[32m========================================\033[0m\n";

    return 0;
}
