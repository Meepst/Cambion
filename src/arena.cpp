#include "arena.h"
#include "defines.h"


Allocator arenaNew(uint64_t minimum_bytes) {
    assert(minimum_bytes > 0);

    VirtualMemoryBlock block = osAlloc(minimum_bytes + ALLOCATOR_RESERVED_SIZE);
    Arena* arena = (Arena*)block.memory;
    arena->memory = (void*)((uintptr_t)block.memory + ALLOCATOR_RESERVED_SIZE);
    arena->size = block.size - ALLOCATOR_RESERVED_SIZE;
    arena->last = 0;

    return {
        .alloc = arenaAlloc,
        .dealloc = arenaDealloc,
        .data = arena
    };
}

void* arenaAlloc(Allocator& allocator, uint64_t bytes, uint64_t alignment) {
    assert(allocator.data != nullptr);

    Arena* arena = (Arena*)allocator.data;

    // Round up to alignment
    uint64_t aligned_start = alignPow2(arena->last, alignment);
    // Check that the arena does not overflow its own allocated memory
    assert(aligned_start + bytes < arena->size);

    void* ptr = (void*)((uintptr_t)arena->memory + aligned_start);
    arena->last = aligned_start + bytes;
    return ptr;
}

void arenaDealloc(Allocator& allocator, uint64_t bytes, ...) {}

void arenaPop(Allocator& allocator, uint64_t bytes, ...) {
    assert(allocator.data != nullptr);

    Arena* arena = (Arena*)allocator.data;
    assert(((uintptr_t)arena->last >= bytes));

    void* ptr = (void*)((uintptr_t)arena->memory + arena->last);
    arena->last -= bytes;
}

void arenaSetLast(Allocator& allocator, uint64_t new_last) {
    assert(allocator.data != nullptr);

    Arena* arena = (Arena*)allocator.data;
    arena->last = new_last;
}

void arenaReset(Allocator& allocator) {
    assert(allocator.alloc == arenaAlloc);

    Arena* arena = (Arena*)allocator.data;
    arena->last = 0;
}

void arenaFree(Allocator& allocator) {
    assert(allocator.alloc == arenaAlloc && allocator.data != nullptr);

    Arena* arena = (Arena*)allocator.data; // os memory to free begins at allocator data
    osFree(arena, arena->size);

    allocator.alloc = nullptr;
    allocator.dealloc = nullptr;
    allocator.data = nullptr;
}
