#pragma once

#include <concepts>
#include <iostream>

namespace utf
{
    template<typename T>
    concept byte_ptr = requires(T bptr)
    {
        *bptr;
        ++bptr, bptr++;
    } && sizeof(*std::declval<T>()) == 1;

    template<typename T>
    concept printable = requires(T pr)
    {
        std::declval<std::ostream>() << pr;
    };
}