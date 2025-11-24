#pragma once

#include "core.h"
#include "endpoint.h"

#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/atomic.hpp>
#include <boost/bind/bind.hpp>

#include <unordered_map>
#include <mutex>

#include <iostream>

using namespace boost::placeholders;

namespace utf
{
namespace endpoints
{

template<>
class net_endpoint<proto_t::tcp, endpoint_t::client>
{
public:
    using req_id_t = uint64_t;

    net_endpoint(boost::asio::io_context& ios, const boost::asio::ip::tcp::endpoint& targ);
    ~net_endpoint();

    template<utf::byte_ptr BP>
    int send(uint64_t req_id, const BP begin, const BP end);

    void stop();
    bool is_connected() const {return m_is_conn.load();}

private:
    void conn_timeo_token(const boost::system::error_code& ec);
    void resp_timeo_token(
        const boost::system::error_code& ec,
        req_id_t request_id
    );

    void conn_token(const boost::system::error_code& ec);
    void send_token(
        const boost::system::error_code& ec,
        size_t bytes_count,
        std::shared_ptr<std::vector<char>> buf
    );
    void recv_token(
        const boost::system::error_code& ec,
        size_t bytes_count
    );

    boost::asio::deadline_timer m_timeo;
    boost::asio::ip::tcp::socket m_sock;
    boost::asio::ip::tcp::endpoint m_targ;

    std::vector<char> m_recv_buf;

    boost::atomic_bool m_is_conn = false;
    boost::atomic_bool m_stopped = false;

    std::mutex m_req_mux;
    std::unordered_map<req_id_t, boost::asio::deadline_timer> m_req_mem;
};

using tcp_client = net_endpoint<proto_t::tcp, endpoint_t::client>;

template<utf::byte_ptr BP>
int tcp_client::send(uint64_t req_id, const BP begin, const BP end)
{
    if(!m_is_conn.load() || end <= begin)
        return -1;
    {
        std::lock_guard l(m_req_mux);
        if(m_req_mem.contains(req_id))
            return -1;
        
        auto emp = m_req_mem.emplace(req_id, boost::asio::deadline_timer(m_sock.get_executor()));
        emp.first->second.expires_from_now(boost::posix_time::seconds(10));
        emp.first->second.async_wait(boost::bind(&tcp_client::resp_timeo_token, this, _1, req_id));
    }

    auto send_buf = std::make_shared<std::vector<char>>();
    send_buf->reserve(end - begin);
    send_buf->insert(send_buf->begin(), begin, end);
    m_sock.async_send(
        boost::asio::buffer(*send_buf, send_buf->size()),
        boost::bind(&tcp_client::send_token, this, _1, _2, send_buf)
    );

    return 0;
}

}
}

