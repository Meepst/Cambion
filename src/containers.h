#pragma once

#include "alloc.h"

#include <stdio.h>


template <typename T>
class DynArray {
    public:
        T* pdata = nullptr;
        uint64_t count = 0;
        uint64_t capacity = 0;

        DynArray() {}
        DynArray(Allocator& allocator, uint64_t num_elements, uint64_t capacity = 0):
                count(num_elements) {
            assert(capacity == 0 || num_elements <= capacity);

            if (capacity == 0) capacity = num_elements;
            pdata = (T*)alloc(allocator, capacity * sizeof(T), alignof(T));
            capacity = capacity;
        }

        void dealloc(Allocator& allocator) {
        #ifdef _DEBUG
            if (pdata == nullptr) {
                printf("Attempted to delete empty DynArray\n");
                return;
            }
        #endif
            allocator.dealloc(allocator, capacity * sizeof(T), pdata);
        }

        T& operator[](uint64_t i) {
            assert(pdata != nullptr && i < count);
            return pdata[i];
        }

        void push(Allocator& allocator, T value) {
            if (count >= capacity) {
                // capacity + capacity / 4 + 16 (minimum), rounded up to mult of 8
                uint64_t new_capacity = alignPow2(capacity + (capacity >> 2) + 16, 8);
                void* memory = allocator.alloc(allocator, new_capacity * sizeof(T), alignof(T));
                memcpy(memory, pdata, count * sizeof(T));

                allocator.dealloc(allocator, capacity * sizeof(T), pdata);

                pdata = (T*)memory;
                pdata[count] = value;
                count += 1;
                capacity = new_capacity;

                return;
            }

            pdata[count] = value;
            count += 1;
        }
        T pop() {
            assert(count > 0);

            count -= 1;
            return pdata[count];
        }


        constexpr T* data() noexcept {
            return pdata;
        }
        constexpr uint64_t size() noexcept {
            return count;
        }
        constexpr T& front() noexcept {
            assert(pdata != nullptr);
            return pdata[0];
        }
        constexpr T& back() noexcept {
            assert(pdata != nullptr);
            return pdata[count-1];
        }

        // Reserve memory if no data was initialized
        void reserve(Allocator& allocator, uint64_t num_elements, uint64_t capacity = 0) {
            assert(pdata == nullptr && (capacity == 0 || num_elements <= capacity));

            if (capacity == 0) capacity = num_elements;

            pdata = (T*)allocator.alloc(allocator, capacity * sizeof(T), alignof(T));
            count = num_elements;
            this->capacity = capacity;
        }
        // If num_elements is less than current size, the elements at the end are discarded, and only num_elements of
        // the original data is copied
        void resize(Allocator& allocator, uint64_t num_elements, uint64_t capacity = 0) {
            assert(capacity == 0 || num_elements <= capacity);

            if (capacity == 0) capacity = num_elements;

            void* memory = allocator.alloc(allocator, capacity * sizeof(T), alignof(T));
            if (pdata != nullptr) {
                memcpy(memory, pdata, (count <= num_elements ? count : num_elements) * sizeof(T));
                allocator.dealloc(allocator, capacity * sizeof(T), pdata);
            }

            pdata = (T*)memory;
            count = num_elements;
            this->capacity = capacity;
        }
};

template <typename T>
void dealloc(Allocator& allocator, DynArray<T>& array) {
#ifdef _DEBUG
    if (array.pdata == nullptr) {
        printf("Attempted to delete empty DynArray\n");
        return;
    }
#endif
    allocator.dealloc(allocator, array.capacity * sizeof(T), array.data);
}

template <typename T>
void push(Allocator& allocator, DynArray<T>& array, T value) {
    if (array.count >= array.capacity) {
        uint64_t new_capacity = alignPow2(array.capacity + (array.capacity >> 2) + 16, 8); // capacity + capacity / 4 + 16 (minimum)
        void* memory = allocator.alloc(allocator, new_capacity * sizeof(T), alignof(T));
        memcpy(memory, array.pdata, array.count * sizeof(T));

        allocator.dealloc(allocator, array.capacity * sizeof(T), array.pdata);

        array.pdata = memory;
        array.pdata[array.count] = value;
        array.count += 1;
        array.capacity = new_capacity;

        return;
    }

    array.pdata[array.count] = value;
    array.count += 1;
}

template <typename T>
T pop(DynArray<T>& array) {
    assert(array.count > 0);

    array.count -= 1;
    return array[array.count];
}
