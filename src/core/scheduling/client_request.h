#pragma once

#include "utf_core.h"

#include <boost/asio/ip/address_v4.hpp>

#include <cstdint>
#include <vector>

namespace utf
{
namespace scheduling
{

struct client_request
{
    client_request() = delete;

    template<byte_ptr BP>
    client_request(
        uint32_t l_id,
        uint64_t arr_tm_ms,
        const boost::asio::ip::address_v4& cl_addr,
        uint16_t cl_port,
        const BP begin, const BP end) :
        arr_timestamp_ms(arr_tm_ms), listener_id(l_id), client_port(cl_port), client_addr(cl_addr)
    {
        if(end - begin > 0)
        {
            payload.reserve(end - begin);
            payload.insert(payload.begin(), begin, end);
        }
    }

    client_request(const client_request& other)
    {
        arr_timestamp_ms = other.arr_timestamp_ms;
        listener_id = other.listener_id;
        client_port = other.client_port;
        client_addr = other.client_addr;
        payload = other.payload;
    }

    client_request(client_request&& other)
    {
        arr_timestamp_ms = other.arr_timestamp_ms;
        listener_id = other.listener_id;
        client_port = other.client_port;
        client_addr = other.client_addr;
        payload = std::move(other.payload);
    }

    client_request& operator=(const client_request& other) = delete;
    client_request& operator=(client_request&& other) = delete;

    std::vector<char> payload;
    uint64_t arr_timestamp_ms;
    uint32_t listener_id;
    uint16_t client_port;
    boost::asio::ip::address_v4 client_addr;
};

}
}