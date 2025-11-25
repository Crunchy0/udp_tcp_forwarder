#pragma once

#include "event.h"
#include "utf_core.h"
#include "endpoint.h"
#include "client_request.h"

#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/atomic.hpp>
#include <boost/bind/bind.hpp>

#include <unordered_map>
#include <mutex>

#include <iostream>

using namespace boost::asio;

namespace utf
{
namespace endpoints
{

template<>
class net_endpoint<proto_t::udp, endpoint_t::server>
{
public:
    net_endpoint(boost::asio::io_context& ios, uint16_t port, uint32_t id);
    ~net_endpoint();

    template<utf::byte_ptr BP>
    int send(const boost::asio::ip::udp::endpoint& targ, const BP begin, const BP end);

    void stop();

    utf::scheduling::event<const utf::scheduling::client_request&> incoming_req_evt;
private:
    void send_token(
        const boost::system::error_code& ec,
        size_t bytes_count,
        ip::udp::endpoint receiver,
        std::shared_ptr<std::vector<char>> buf
    );
    void recv_token(
        const boost::system::error_code& ec,
        size_t bytes_count
    );

    std::vector<char> m_recv_buf;
    boost::asio::ip::udp::socket m_sock;
    boost::asio::ip::udp::endpoint m_remote_ep;

    boost::atomic_bool m_is_stopped = false;

    uint32_t m_id;
};

using udp_server = net_endpoint<proto_t::udp, endpoint_t::server>;

template<utf::byte_ptr BP>
int udp_server::send(const boost::asio::ip::udp::endpoint& targ, const BP begin, const BP end)
{
    auto send_buf = std::make_shared<std::vector<char>>();
    send_buf->reserve(end - begin);
    send_buf->insert(send_buf->begin(), begin, end);
    m_sock.async_send_to(
        boost::asio::buffer(*send_buf, send_buf->size()),
        targ,
        boost::bind(&udp_server::send_token, this, placeholders::error, placeholders::bytes_transferred, targ, send_buf)
    );

    return 0;
}

}
}

