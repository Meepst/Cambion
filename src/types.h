#pragma once

#include "alloc.h"


template <typename T>
class Vector {
    T* data;
    uint64_t size;

    Vector() = delete;
    Vector(Allocator& allocator, uint64_t size, uint64_t alignment = 1):
        data(alloc(allocator, size, alignment)), size(alignPow2(size, alignment)) {}
    T& operator[](uint64_t i) {
        if (i > size) {
            printf("Error, Vector access out of bounds\n");
            exit(-1);
        }

        return data[i];
    }
};
