#pragma once

#include <stdint.h>

#include <bit>
#include <type_traits>

// Round num up to the nearest multiple of to, to should be a multiple of 2
inline uint64_t alignPow2(uint64_t num, uint64_t to) {
    return num + (to - 1) & (~to + 1);
}

// log2 for numbers that satisfy 2^N, where N is the return value
template <typename T>
inline int log2n(T uint) {
    static_assert(std::is_unsigned_v<T>);
    assert((std::countl_zero(uint) + std::countr_zero(uint)) == ((sizeof(uint) << 3) - 1));

    return (sizeof(uint) << 3) - std::countl_zero(uint) - 1;
}
