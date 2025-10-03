#include "alloc.h"


// Runs before main
CambionInternal uint64_t initialize() {
// Error: Access Violation
// #if _WIN32
//     LPSYSTEM_INFO info;
//     GetSystemInfo(&info);

//     return info.dwPageSize;

#if __linux__
    return sysconf(_SC_PAGE_SIZE);

#else
    return 4096; // guess??

#endif
}

CambionInternal const uint64_t PAGE_SIZE = initialize();


VirtualMemoryBlock osAlloc(size_t minimum_bytes, uint64_t flags) {
    assert(minimum_bytes > 0);

    void* memory;
    // Round up to the nearest multiple of page size
    uint64_t num_bytes = alignPow2(minimum_bytes, PAGE_SIZE);

#if _WIN32
    memory = VirtualAlloc(nullptr, num_bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (memory == NULL) {
        printf("Error, failed to allocate memory from Windows\n");
        exit(-1);
    }
    // Get real number of bytes allocated
    MEMORY_BASIC_INFORMATION mbi;
    VirtualQuery(memory, &mbi, sizeof(mbi));
    uint64_t allocated_bytes = mbi.RegionSize;

    // TODO: preload pages into memory

#elif __linux__
    // Num bytes actually found should be rounded up to nearest page size
    memory = mmap(nullptr, num_bytes, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, 0, 0);
    if (memory == MAP_FAILED) {
        printf("Error, failed to allocate memory from Linux\n");
        exit(-1);
    }

#else
    memory = calloc(num_bytes, 1);
    if (memory == NULL) {
        printf("Error, failed to allocate memory from calloc\n");
        exit(-1);
    }

#endif
    return {
        .memory = memory,
        .size = num_bytes
    };
}

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

#else
    free(ptr);

#endif
    ptr = nullptr;
}

void* alloc(Allocator& allocator, uint64_t bytes, uint64_t alignment) {
    assert(allocator.alloc != nullptr && bytes > 0);

    return allocator.alloc(allocator, bytes, alignment);
}

void dealloc(Allocator& allocator, uint64_t bytes, ...) {
    assert(allocator.dealloc != nullptr && allocator.data != nullptr && bytes > 0);

    allocator.dealloc(allocator, bytes);
}
