#include "tcp_client.h"

#include "spdlog/spdlog.h"

#include <iostream>
#include <string>
#include <tuple>

namespace utf
{
namespace endpoints
{

tcp_client::net_endpoint(
    boost::asio::io_context& ioc,
    const boost::asio::ip::tcp::endpoint& targ,
    uint64_t conn_timeo_ms,
    uint64_t resp_timeo_ms
) :
    m_sock(ioc),
    m_timeo(ioc),
    m_targ(targ),
    m_conn_timeo_ms(conn_timeo_ms),
    m_resp_timeo_ms(resp_timeo_ms)
{
    start_connect();
}

tcp_client::~net_endpoint()
{
    stop();
}

void tcp_client::start_connect()
{
    m_sock.async_connect(m_targ, boost::bind(&tcp_client::conn_token, this, _1));

    m_timeo.expires_from_now(boost::posix_time::milliseconds(m_conn_timeo_ms));
    m_timeo.async_wait(boost::bind(&tcp_client::conn_timeo_token, this, _1));
}

void tcp_client::conn_token(const boost::system::error_code& ec)
{
    if(m_stopped.load() || !m_sock.is_open())
        return;

    if(ec)
    {
        spdlog::error("({0}:{1}) Async connect error: {2}",
             m_targ.address().to_string(), m_targ.port(), ec.message()
        );
        m_sock.close();
    }
    else
    {
        spdlog::info("({0}:{1}) Connection is successful",
            m_targ.address().to_string(), m_targ.port()
        );
        m_timeo.expires_at(boost::posix_time::pos_infin);
        m_timeo.async_wait([](const boost::system::error_code& ec){});
        m_is_conn.store(true);

        m_recv_buf.resize(4096);

        m_sock.async_receive(
            boost::asio::buffer(m_recv_buf, m_recv_buf.size()),
            boost::bind(&tcp_client::recv_token, this, _1, _2)
        );
    }
}

void tcp_client::conn_timeo_token(const boost::system::error_code& ec)
{
    if(ec || m_stopped.load())
        return;

    if(m_timeo.expires_at() > boost::asio::deadline_timer::traits_type::now())
    {
        // Continue waiting, still have time
        m_timeo.async_wait(boost::bind(&tcp_client::conn_timeo_token, this, _1));
        return;
    }

    spdlog::warn("({0}:{1}) Connection timed out, reconnecting",
        m_targ.address().to_string(), m_targ.port()
    );

    // Try reconnecting
    m_sock.close();
    start_connect();
}

void tcp_client::resp_timeo_token(
    const boost::system::error_code& ec,
    req_id_t request_id
)
{
    if(m_stopped.load())
        return;

    if(ec)
    {
        spdlog::debug("Cancelled waiting for a timeout (request #{0:x}): {1}",
            request_id, ec.message()
        );
        return;
    }

    std::lock_guard l(m_req_mux);
    auto it = m_req_mem.find(request_id);
    if(it == m_req_mem.end())
    {
        spdlog::debug("Request #{0:x} does not exists", request_id);
        return;
    }

    if(it->second.expires_at() > boost::asio::deadline_timer::traits_type::now())
    {
        spdlog::debug("Still waiting for a response (request #{0:x})", request_id);
        it->second.async_wait(boost::bind(&tcp_client::resp_timeo_token, this, _1, request_id));
        return;
    }

    // Timeout has expired, notify listeners
    giveaway_response(STATUS_TIMEOUT, request_id, std::vector<char>());

    m_req_mem.erase(it);
}

void tcp_client::send_token(
    const boost::system::error_code& ec,
    size_t bytes_count,
    std::shared_ptr<std::vector<char>> buf
)
{
    if(m_stopped.load())
        return;

    if(ec)
    {
        spdlog::error("({0}:{1}) Send failed",
            m_targ.address().to_string(), m_targ.port()
        );

        // Try reconnecting
        m_is_conn.store(false);
        if(m_sock.is_open())
            m_sock.close();
        start_connect();
        return;
    }

    spdlog::debug("({0}:{1}) Sent {2} bytes",
        m_targ.address().to_string(), m_targ.port(), bytes_count
    );
}

void tcp_client::recv_token(
    const boost::system::error_code& ec,
    size_t bytes_count
)
{
    if(m_stopped.load())
        return;

    if(ec)
    {
        spdlog::error("({0}:{1}) Receive error: {2}",
            m_targ.address().to_string(), m_targ.port(), ec.message()
        );

        // Try reconnecting
        m_is_conn.store(false);
        if(m_sock.is_open())
            m_sock.close();
        start_connect();
        return;
    }
    
    if(bytes_count < sizeof(req_id_t))
    {
        spdlog::error("({0}:{1}) Received response is shorter than size of request ID ({2})",
            m_targ.address().to_string(), m_targ.port(), sizeof(req_id_t)
        );
    }
    else
    {
        const req_id_t* req_id = reinterpret_cast<const req_id_t*>(m_recv_buf.data());

        auto [payload_begin, payload_end] = std::make_tuple(
            (m_recv_buf.begin() + sizeof(req_id_t)),
            (m_recv_buf.begin() + bytes_count)
        );

        spdlog::trace("({0}:{1}) Received a response on request#{2:x}",
            m_targ.address().to_string(), m_targ.port(), *req_id
        );

        // Received before timeout expiration, notify listeners
        giveaway_response(STATUS_OK, *req_id, std::vector<char>(payload_begin, payload_end));

        std::lock_guard l(m_req_mux);
        auto it = m_req_mem.find(*req_id);
        if(it != m_req_mem.end())
        {
            spdlog::debug("Deleting request #{0:x}", *req_id);
            m_req_mem.erase(it);
        }
    }

    m_sock.async_receive(
        boost::asio::buffer(m_recv_buf, m_recv_buf.size()),
        boost::bind(&tcp_client::recv_token, this, _1, _2)
    );
}

void tcp_client::giveaway_response(uint32_t status, req_id_t req_id, std::vector<char>&& payload)
{
    const auto* status_bytes = reinterpret_cast<const char*>(&status);
    payload.insert(payload.begin(), status_bytes, status_bytes + sizeof(status));

    // Write timestamp depending on status
    uint64_t curr_time_us;
    switch(status)
    {
        case STATUS_TIMEOUT:
            curr_time_us = TIMESTAMP_TIMEOUT;
            break;
        default:
            using namespace std::chrono;
            curr_time_us =
                duration_cast<microseconds>(system_clock::now().time_since_epoch()).count();
            break;
    }

    // Notify all response listeners
    resp_giveaway_evt.invoke(
        scheduling::server_response(
            req_id,
            curr_time_us,
            payload.begin(), payload.end()
        )
    );
}

void tcp_client::stop()
{
    m_stopped.store(true);
    m_is_conn.store(false);

    m_timeo.cancel();
    m_sock.close();

    std::lock_guard l(m_req_mux);
    for(auto& elem : m_req_mem)
        elem.second.cancel();
    m_req_mem.clear();
}

}
}
