#include "tcp_client.h"

#include <iostream>
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
    m_sock.async_connect(m_targ, boost::bind(&tcp_client::conn_token, this, _1));

    m_timeo.expires_from_now(boost::posix_time::milliseconds(m_conn_timeo_ms));
    m_timeo.async_wait(boost::bind(&tcp_client::conn_timeo_token, this, _1));
}

tcp_client::~net_endpoint()
{
    stop();
}

void tcp_client::conn_token(const boost::system::error_code& ec)
{
    if(m_stopped.load() || !m_sock.is_open())
        return;

    if(ec)
    {
        std::cerr << "Async connect error(" << m_targ << ") : " << ec.message() << "\n";
        m_sock.close();
    }
    else
    {
        std::cout << "Connection to " << m_targ << " is successful\n";
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
    if(m_stopped.load())
        return;

    if(m_timeo.expires_at() <= boost::asio::deadline_timer::traits_type::now())
    {
        m_sock.close();
        m_timeo.expires_from_now(boost::posix_time::seconds(1));
        m_sock.async_connect(m_targ, boost::bind(&tcp_client::conn_token, this, _1));
        std::cerr << "Connection to " << m_targ << " timed out, reconnecting...\n";
    }
    else if(ec)
    {
        std::cerr << "Error in connection timeout token: " << ec.message() << "\n";
        return;
    }
    m_timeo.async_wait(boost::bind(&tcp_client::conn_timeo_token, this, _1));
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
        std::cerr << "Cancelled waiting for a timeout (request #" << request_id << "): "
            << ec.message() << "\n";
        return;
    }

    std::lock_guard l(m_req_mux);
    auto it = m_req_mem.find(request_id);
    if(it == m_req_mem.end())
    {
        std::cerr << "Request #" << request_id << " no longer exists\n";
        return;
    }

    if(it->second.expires_at() > boost::asio::deadline_timer::traits_type::now())
    {
        std::cout << "Still waiting for a response (request #" << request_id << ")";
        it->second.async_wait(boost::bind(&tcp_client::resp_timeo_token, this, _1, request_id));
        return;
    }

    std::cout << "Request #" << request_id << " is expired\n";
    giveaway_response(STATUS_OK, request_id, std::vector<char>());

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
        std::cerr << "Error while sending to " << m_targ << "\n";
    }
    else
    {
        std::cout << "Sent " << bytes_count << " bytes to " << m_targ << " successfully\n";
    }
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
        std::cerr << "Error while receiving from " << m_targ << " : " << ec.message() << "\n";
    }
    else if(bytes_count < sizeof(req_id_t))
    {
        std::cerr << "Response from " << m_targ << "is too small\n";
    }
    else
    {
        const req_id_t* req_id = reinterpret_cast<const req_id_t*>(m_recv_buf.data());

        auto [payload_begin, payload_end] = std::make_tuple(
            (m_recv_buf.begin() + sizeof(req_id_t)),
            (m_recv_buf.begin() + bytes_count)
        );

        std::cout << "Received a response on request with ID " << std::hex << *req_id << "\n";
        giveaway_response(STATUS_OK, *req_id, std::vector<char>(payload_begin, payload_end));

        std::lock_guard l(m_req_mux);
        auto it = m_req_mem.find(*req_id);
        if(it != m_req_mem.end())
        {
            std::cout << "Deleting request with ID " << std::hex << *req_id << "\n";
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

    uint64_t curr_time_ms;
    switch(status)
    {
        case STATUS_TIMEOUT:
            curr_time_ms = ~0ul;
            break;
        default:
            using namespace std::chrono;
            curr_time_ms =
                duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
            break;
    }

    resp_giveaway_evt.invoke(
        scheduling::server_response(
            req_id,
            curr_time_ms,
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
