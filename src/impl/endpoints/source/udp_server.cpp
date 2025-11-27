#include "udp_server.h"

#include "spdlog/spdlog.h"

#include <chrono>

namespace utf
{
namespace endpoints
{

udp_server::net_endpoint(boost::asio::io_context& ioc, uint16_t port, uint32_t id) :
    m_sock(ioc, ip::udp::endpoint(ip::udp::v4(), port)), m_id(id)
{
    m_recv_buf.resize(4096);
    m_sock.async_receive_from(
        buffer(m_recv_buf), m_remote_ep,
        boost::bind(&udp_server::recv_token, this, placeholders::error, placeholders::bytes_transferred)
    );
}

udp_server::~net_endpoint()
{
    stop();
}

void udp_server::stop()
{
    m_sock.close();
}

void udp_server::send_token(
    const boost::system::error_code& ec,
    size_t bytes_count,
    ip::udp::endpoint receiver,
    std::shared_ptr<std::vector<char>> buf
)
{
    if(ec)
    {
        spdlog::error("({0}:{1}) Send to {2}:{3} failed",
            m_sock.local_endpoint().address().to_string(), m_sock.local_endpoint().port(),
            receiver.address().to_string(), receiver.port()
        );
    }
    else
    {
        spdlog::debug("({0}:{1}) Send {2} bytes to {3}:{4}",
            m_sock.local_endpoint().address().to_string(), m_sock.local_endpoint().port(),
            bytes_count,
            receiver.address().to_string(), receiver.port()
        );
    }
}

void udp_server::recv_token(
    const boost::system::error_code& ec,
    size_t bytes_count
)
{
    if(ec)
    {
        spdlog::error("({0}:{1}) Receive error: {2}",
            m_sock.local_endpoint().address().to_string(), m_sock.local_endpoint().port(),
            ec.message()
        );
        return;
    }

    spdlog::trace("({0}:{1}) Received message from {2}:{3}",
        m_sock.local_endpoint().address().to_string(), m_sock.local_endpoint().port(),
        m_remote_ep.address().to_string(), m_remote_ep.port()
    );

    using namespace std::chrono;
    uint64_t curr_time_us =
        duration_cast<microseconds>(system_clock::now().time_since_epoch()).count();

    // Notify everyone who wants to handle requests
    incoming_req_evt.invoke(
        utf::scheduling::client_request(
            m_id, curr_time_us,
            m_remote_ep.address().to_v4(), m_remote_ep.port(),
            m_recv_buf.begin(), m_recv_buf.begin() + bytes_count
        )
    );
    
    m_sock.async_receive_from(
        buffer(m_recv_buf), m_remote_ep,
        boost::bind(&udp_server::recv_token, this, placeholders::error, placeholders::bytes_transferred)
    );
}

}
}