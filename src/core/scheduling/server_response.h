#pragma once

#include "utf_core.h"

#include <cstdint>
#include <vector>

namespace utf
{
namespace scheduling
{

struct server_response
{
    server_response() = delete;

    template<byte_ptr BP>
    server_response(
        uint64_t req_id,
        uint64_t resp_ts_us,
        const BP begin, const BP end) :
        request_id(req_id), resp_timestamp_us(resp_ts_us)
    {
        if(end - begin > 0)
        {
            payload.reserve(end - begin);
            payload.insert(payload.begin(), begin, end);
        }
    }

    server_response(const server_response& other)
    {
        request_id = other.request_id;
        resp_timestamp_us = other.resp_timestamp_us;
        payload = other.payload;
    }

    server_response(server_response&& other)
    {
        request_id = other.request_id;
        resp_timestamp_us = other.resp_timestamp_us;
        payload = std::move(other.payload);
    }

    server_response& operator=(const server_response& other) = delete;
    server_response& operator=(server_response&& other) = delete;

    uint64_t request_id;
    uint64_t resp_timestamp_us;
    std::vector<char> payload;
};

}
}
