#pragma once

#include "core.h"

#include <boost/asio/ip/address.hpp>
#include <boost/asio/io_context.hpp>

#include <cstdint>
#include <vector>

namespace utf
{
namespace endpoints
{

enum proto_t
{
    udp,
    tcp
};

enum endpoint_t
{
    client,
    server
};

struct net_recv_frame
{
    boost::asio::ip::address_v4 sender_addr;
    uint16_t sender_port;

    std::vector<char> payload;
};

template<proto_t PrT, endpoint_t EpT>
class net_endpoint
{
public:
    net_endpoint(boost::asio::io_context& ioc) = delete;

    ~net_endpoint() = delete;
    net_endpoint() = delete;
    net_endpoint(const net_endpoint& other) = delete;
    net_endpoint(net_endpoint&& other) = delete;
    net_endpoint& operator=(const net_endpoint& other) = delete;
    net_endpoint& operator=(net_endpoint&& other) = delete;

    template<byte_ptr BP>
    void send(const BP begin, const BP end);
    
    void stop();
};

}
}
