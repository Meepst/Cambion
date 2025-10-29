#pragma once

#include "bits.h"
#include "defines.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>


#if _WIN32
    #include <windows.h>
#elif __linux__
    #include <sys/mman.h>
    #include <unistd.h>
#else
    #error Your platform is not currently supported.
#endif

struct Allocator;

struct VirtualMemoryBlock {
    void* memory = nullptr;
    uint64_t size = 0;
};

// Allocates contiguous block of read/writable virtual memory of at minimum
// the bytes requested, initialized to 0
VirtualMemoryBlock osAlloc(uint64_t minimum_bytes, uint64_t flags = 0);
void osFree(void* memory, uint64_t size);


typedef void*(*AllocFnPtr)(Allocator& allocator, uint64_t bytes, uint64_t alignment);
typedef void(*DeallocFnPtr)(Allocator& allocator, uint64_t bytes, ...);

struct Allocator {
    AllocFnPtr alloc = nullptr;
    DeallocFnPtr dealloc = nullptr;
    void* data = nullptr;
};
// General alloc/dealloc functions, dispatches impl on allocator's function pointer
void* alloc(Allocator& allocator, uint64_t bytes, uint64_t alignment);
void dealloc(Allocator& allocator, uint64_t bytes, ...);
