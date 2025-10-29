#pragma once

#include "alloc.h"

#include <algorithm>


template <typename T>
class DynArray {
    public:
        T* _data = nullptr;
        uint64_t _size = 0;
        uint64_t _capacity = 0;

        DynArray() {}
        DynArray(Allocator& allocator, uint64_t num_elements, uint64_t capacity = 0):
                _size(num_elements) {
            assert(capacity == 0 || num_elements <= capacity);

            if (capacity == 0) capacity = num_elements;
            _data = (T*)alloc(allocator, capacity * sizeof(T), alignof(T));
            _capacity = capacity;
        }

        void dealloc(Allocator& allocator) {
            allocator.dealloc(allocator, _capacity * sizeof(T), _data);
        }

        T& operator[](uint64_t i) {
            assert(_data != nullptr && i < _size);
            return _data[i];
        }

        void push(Allocator& allocator, T value) {
            if (_size >= _capacity) {
                // capacity + capacity / 4 + 16 (minimum), rounded up to mult of 8
                uint64_t new_capacity = alignPow2(_capacity + (_capacity >> 2) + 16, 8);
                void* memory = allocator.alloc(allocator, new_capacity * sizeof(T), alignof(T));
                memcpy(memory, _data, _size * sizeof(T));

                allocator.dealloc(allocator, _capacity * sizeof(T), _data);

                _data = (T*)memory;
                _data[_size] = value;
                _size += 1;
                _capacity = new_capacity;

                return;
            }

            _data[_size] = value;
            _size += 1;
        }
        T pop() {
            assert(_size > 0);

            _size -= 1;
            return _data[_size];
        }


        constexpr T* data() noexcept {
            assert(_data != nullptr);
            return _data;
        }
        constexpr uint64_t size() noexcept {
            assert(_data != nullptr);
            return _size;
        }
        constexpr T& front() noexcept {
            assert(_data != nullptr);
            return _data[0];
        }
        constexpr T& back() noexcept {
            assert(_data != nullptr);
            return _data[_size-1];
        }

        // Reserve memory if no data was initialized
        void reserve(Allocator& allocator, uint64_t num_elements, uint64_t capacity = 0) {
            assert(_data == nullptr && (capacity == 0 || num_elements <= capacity));

            if (capacity == 0) capacity = num_elements;

            _data = (T*)allocator.alloc(allocator, std::max(capacity, num_elements) * sizeof(T), alignof(T));
            _size = num_elements;
            _capacity = capacity;
        }
        // If num_elements is less than current size, the elements at the end are discarded, and only num_elements of
        // the original data is copied
        void resize(Allocator& allocator, uint64_t num_elements, uint64_t capacity = 0) {
            assert(capacity == 0 || num_elements <= capacity);

            if (capacity == 0) capacity = num_elements;

            void* memory = allocator.alloc(allocator, std::max(capacity, num_elements) * sizeof(T), alignof(T));
            if (_data != nullptr) {
                memcpy(memory, _data, std::min(_size, num_elements) * sizeof(T));
                allocator.dealloc(allocator, _capacity * sizeof(T), _data);
            }

            _data = (T*)memory;
            _size = num_elements;
            _capacity = capacity;
        }
};

template <typename T>
void dealloc(Allocator& allocator, DynArray<T>& array) {
    allocator.dealloc(allocator, array._capacity * sizeof(T), array.data);
}

template <typename T>
void push(Allocator& allocator, DynArray<T>& array, T value) {
    if (array._size >= array._capacity) {
        uint64_t new_capacity = alignPow2(array._capacity + (array._capacity >> 2) + 16, 8); // capacity + capacity / 4 + 16 (minimum)
        void* memory = allocator.alloc(allocator, new_capacity * sizeof(T), alignof(T));
        memcpy(memory, array._data, array._size * sizeof(T));

        allocator.dealloc(allocator, array._capacity * sizeof(T), array._data);

        array._data = memory;
        array._data[array._size] = value;
        array._size += 1;
        array._capacity = new_capacity;

        return;
    }

    array._data[array._size] = value;
    array._size += 1;
}

template <typename T>
T pop(DynArray<T>& array) {
    assert(array._size > 0);

    array._size -= 1;
    return array[array._size];
}
