#pragma once

#include "alloc.h"


template <typename T>
class Vector {
    public:
        T* _data;
        uint64_t _size;
        uint64_t _capacity;

        Vector() = delete;
        Vector(Allocator& allocator, uint64_t num_elements, uint64_t extended_capacity = 0, uint64_t alignment = 1):
            _data(alloc(allocator, sizeof(T) * (num_elements + extended_capacity), alignment)),
            _size(num_elements),
            _capacity(num_elements + extended_capacity) {}

        T& operator[](uint64_t i) noexcept {
            if (i > _size) {
                printf("Error, Vector access out of bounds\n");
                exit(-1);
            }
            return _data[i];
        }

        constexpr T* data() noexcept {
            return _data;
        }
        constexpr uint64_t size() noexcept {
            return _size;
        }
        constexpr T& front() noexcept {
            return _data[0];
        }
        constexpr T& back() noexcept {
            return _data[_size-1];
        }
};

template <typename T>
void dealloc(Allocator& allocator, Vector<T>& vector) {
    allocator.dealloc(allocator, vector.size * sizeof(T), vector.data);
}

template <typename T>
void push(Allocator& allocator, Vector<T>& vector, T&& value) {
    if (vector._size >= vector._capacity) {
        uint64_t new_capacity = vector._capacity + (vector._capacity >> 2) + 16; // capacity + capacity / 4 + 16 (minimum)
        void* memory = allocator.alloc(allocator, new_capacity * sizeof(T), alignof(T));
        memcpy(memory, vector._data, vector._size * sizeof(T));

        vector._data = memory;
        vector._data[vector._size] = value;
        vector._size += 1;
        vector._capacity = new_capacity;

        return;
    }

    vector._data[vector._size] = value;
    vector._size += 1;
}

template <typename T>
T pop(Vector<T>& vector) {
    if (vector._size == 0) {
        printf("Error, attempted to pop empty vector\n");
        exit(-1);
    }

    vector._size -= 1;
    return vector[vector._size];
}
