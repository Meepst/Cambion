#include "arena.h"


Allocator arenaNew(uint64_t minimum_bytes) {
    assert(minimum_bytes > 0);

    Arena* arena = (Arena*)calloc(1, sizeof(Arena));
    if (arena == nullptr) {
        printf("Error, failed to allocate arena allocator members\n");
        exit(-1);
    }

    VirtualMemoryBlock block = osAlloc(minimum_bytes);
    arena->memory = block.memory;
    arena->size = block.size;
    arena->last = 0;

    return {
        .alloc = arenaPush,
        .dealloc = arenaPop,
        .data = arena
    };
}

void* arenaPush(Allocator& allocator, uint64_t bytes, uint64_t alignment) {
    assert(allocator.data != nullptr);

    bytes = alignPow2(bytes, alignment);

    Arena* arena = (Arena*)allocator.data;
    // Check that the arena does not overflow its own allocated memory
    assert(((uintptr_t)arena->last + bytes) < (uintptr_t)arena->size);

    void* ptr = (void*)((uintptr_t)arena->memory + (uintptr_t)arena->last);
    arena->last += bytes;
    return ptr;
}

void arenaPop(Allocator& allocator, uint64_t bytes, ...) {
    assert(allocator.data != nullptr);

    Arena* arena = (Arena*)allocator.data;
    assert(((uintptr_t)arena->last - bytes) >= 0);

    void* ptr = (void*)((uintptr_t)arena->memory + (uintptr_t)arena->last);
    arena->last -= bytes;
}

void arenaReset(Allocator& allocator) {
    assert(allocator.alloc == arenaPush);

    Arena* arena = (Arena*)allocator.data;
    arena->last = 0;
}

void arenaFree(Allocator& allocator) {
    assert(allocator.alloc == arenaPush && allocator.data != nullptr);

    Arena* arena = (Arena*)allocator.data;
    osFree(arena->memory, arena->size);

    allocator.alloc = nullptr;
    allocator.dealloc = nullptr;
    allocator.data = nullptr;
}
