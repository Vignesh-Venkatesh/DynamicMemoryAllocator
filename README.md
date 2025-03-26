# Dynamic Memory Allocator

This project implements a custom heap management system, featuring a dynamic memory allocator. The allocator is designed to efficiently manage memory by organizing free and allocated blocks with specific structures and alignment requirements.

## Heap Initialization

During heap initialization, the layout is as follows:

| Alignment | Prologue Header | Prologue Footer | Epilogue Footer |
| --------- | --------------- | --------------- | --------------- |
| 8 bytes   | 8 bytes         | 8 bytes         | 8 bytes         |

- **Total size**: 32 bytes

The heap starts with a prologue header, prologue footer, and an epilogue footer for maintaining the structure of the heap and facilitating boundary checking.

## Allocated Block Structure

An allocated block consists of two parts:

| Header (8 bytes)                              | Payload |
| --------------------------------------------- | ------- |
| Stores size and allocation status information |

- **Header**: The header stores the block's size and whether it is allocated.
- **Footer**: The footer mirrors the header, storing the same information.
- **Alignment**: The block is aligned to 16 bytes for optimal memory access. However, the minimum block size is 32 bytes due to the structure.

**Note:**
Both the header and footer contain essential information for managing memory allocation and deallocation.

## Free Block Structure

A free block is structured as follows:

| Header (8 bytes)                                                                  | Prev Pointer | Next Pointer | Footer (8 bytes) |
| --------------------------------------------------------------------------------- | ------------ | ------------ | ---------------- |
| Stores size, allocation status, and pointers to the previous and next free blocks |

- **Header**: Contains size information, allocation status, and pointers to the previous and next free blocks in the segregated free list.
- **Footer**: Mirrors the header and stores size and allocation status.

### Alignment:

Free blocks are aligned to 16 bytes, and the minimum size for a free block is also 32 bytes due to the structure.

## Segregated Free List

The allocator uses a **Segregated Free List** to organize free blocks into bins based on their sizes. This helps optimize memory management by allowing the system to quickly find blocks of appropriate sizes when allocating memory.
