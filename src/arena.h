#pragma once
#include <stdint.h>
#include "alloc.h"


// Allocates memory using an increasing offset to its own reserved memory,
// returns blocks of memory from the top of the arena
struct Arena {
    void* memory = nullptr;
    uint64_t size = 0;
    uint64_t last = 0;
};

Allocator arenaNew(uint64_t minimum_bytes);
// Impl of alloc for Arenas, equivalent to alloc(arena_allocator, ...)
void* arenaPush(Allocator& allocator, uint64_t bytes, uint64_t alignment);
#define arenaPushN(allocator, type, count) \
    arenaPush(allocator, sizeof(type)*count, alignof(type))
// Reduce the offset of last, invalidating any memory allocated between old last
// and new last
void arenaPop(Allocator& allocator, uint64_t bytes, ...);
// Resets the offset of last allocation to the beginning, invalidates all
// previously allocated memory
void arenaReset(Allocator& allocator);
void arenaFree(Allocator& allocator);
