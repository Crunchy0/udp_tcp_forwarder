#include "udp_server.h"

namespace utf
{
namespace endpoints
{

udp_server::net_endpoint(boost::asio::io_context& ioc, uint16_t port) :
    m_sock(ioc, ip::udp::endpoint(ip::udp::v4(), port))
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
        std::cerr << "Failed sending message to " << receiver << "\n";
    }
    else
    {
        std::cout << "Send message (" << bytes_count << " bytes) to " << receiver << "\n";
    }
}

void udp_server::recv_token(
    const boost::system::error_code& ec,
    size_t bytes_count
)
{
    if(ec)
    {
        std::cerr << "UDP receive error: " << ec.message() << "\n";
        return;
    }

    std::string rs(m_recv_buf.data(), bytes_count);
    m_sock.async_receive_from(
        buffer(m_recv_buf), m_remote_ep,
        boost::bind(&udp_server::recv_token, this, placeholders::error, placeholders::bytes_transferred)
    );
}

}
}