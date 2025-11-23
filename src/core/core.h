#pragma once

#include <concepts>

namespace utf
{
namespace core
{
    template<typename T>
    concept byte_ptr = requires(T bptr)
    {
        *bptr;
        ++bptr, bptr++;
    } && sizeof(*std::declval<T>()) == 1;
}
}