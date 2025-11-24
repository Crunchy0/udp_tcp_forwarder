#pragma once

#include <concepts>

namespace utf
{
    template<typename T>
    concept byte_ptr = requires(T bptr)
    {
        *bptr;
        ++bptr, bptr++;
    } && sizeof(*std::declval<T>()) == 1;
}