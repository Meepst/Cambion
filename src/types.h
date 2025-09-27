#pragma once

#include "alloc.h"


template <typename T>
class Vector {
    public:
        T* data;
        uint64_t size;

        Vector() = delete;
        Vector(Allocator& allocator, uint64_t num_elements, uint64_t alignment = 1):
            data(alloc(allocator, sizeof(T) * num_elements, alignment)), size(alignPow2(sizeof(T) * num_elements, alignment)) {}
        T& operator[](uint64_t i) {
            if (i > size) {
                printf("Error, Vector access out of bounds\n");
                exit(-1);
            }

            return data[i];
        }
};

template <typename T>
void dealloc(Allocator& allocator, Vector<T>& vector) {
    allocator.dealloc(allocator, sizeof(T) * vector.size, vector.data);
}
