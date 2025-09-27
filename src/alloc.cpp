#include "alloc.h"


internal uint64_t initialize();

internal const uint64_t PAGE_SIZE = initialize();

// Runs before main
internal uint64_t initialize() {
#if _WIN32
    return 0; // Page size not currently used for Windows

#elif __linux__
    return sysconf(_SC_PAGE_SIZE);

#endif
}

#if _WIN32
VirtualMemoryBlock osAlloc(size_t minimum_bytes, uint64_t flags) {
    assert(minimum_bytes > 0);

    void* memory = VirtualAlloc(nullptr, minimum_bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (memory == NULL) {
        printf("Error, failed to allocate Windows memory\n");
        exit(-1);
    }
    // Get real number of bytes allocated
    MEMORY_BASIC_INFORMATION mbi;
    VirtualQuery(memory, &mbi, sizeof(mbi));
    uint64_t allocated_bytes = mbi.RegionSize;

    // TODO: preload pages into memory
    return {
        .memory = memory,
        .size = allocated_bytes
    };
}

#elif __linux__
VirtualMemoryBlock osAlloc(uint64_t minimum_bytes, uint64_t flags) {
    assert(minimum_bytes > 0);

    void* memory = mmap(nullptr, minimum_bytes, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, 0, 0);
    if (memory == MAP_FAILED) {
        printf("Error, failed to allocate Linux memory\n");
        exit(-1);
    }
    // Round up to the nearest multiple of page size
    uint64_t allocated_bytes = alignPow2(minimum_bytes, PAGE_SIZE);

    return {
        .memory = memory,
        .size = allocated_bytes
    };
}

#endif

void osFree(void* ptr, uint64_t size) {
    if (ptr == nullptr) {
        printf("Error, attempted to free a null pointer\n");
        exit(-1);
    }

#if _WIN32
    if (VirtualFree(ptr, size, MEM_RELEASE) == 0) {
        printf("Error, failed to free Windows memory\n");
        exit(-1);
    }

#elif __linux__
    if (munmap(ptr, size) == -1) {
        printf("Error, failed to free Linux memory\n");
        exit(-1);
    }

    ptr = nullptr;
#endif
}

void* alloc(Allocator& allocator, uint64_t bytes, uint64_t alignment) {
    assert(allocator.alloc != nullptr && allocator.data != nullptr && bytes > 0);

    return allocator.alloc(allocator, bytes, alignment);
}

void dealloc(Allocator& allocator, uint64_t bytes, ...) {
    assert(allocator.dealloc != nullptr && allocator.data != nullptr && bytes > 0);

    allocator.dealloc(allocator, bytes);
}

uint64_t alignPow2(uint64_t num, uint64_t to) {
    return num + (to - 1) & (~to + 1);
}
