#pragma once

#include "event.h"
#include "edr_logger.h"

#include "endpoints/include/endpoint_impl.h"

#include "client_request.h"
#include "server_response.h"
#include "forwarder.h"

#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <vector>

namespace utf
{
namespace scheduling
{

class rr_forwarder : public forwarder
{
private:
    struct pending_request
    {
        uint64_t request_id;
        uint32_t listener_id;
        uint16_t client_port;
        uint16_t server_port;
        boost::asio::ip::address_v4 client_addr;
        boost::asio::ip::address_v4 server_addr;
        uint64_t arrival_time_ms;
        uint64_t fwd_time_us;
    };

public:
    rr_forwarder() = delete;
    rr_forwarder(std::vector<std::shared_ptr<utf::endpoints::tcp_client>>&& clients);
    ~rr_forwarder() override;
    
    void schedule(const client_request& req) override;
    void schedule(client_request&& req) override;
    
    event<uint32_t, boost::asio::ip::address_v4, uint16_t, const std::vector<char>&> send_back_evt;
    event<const aux::edr&> edr_report_evt;
    
private:
    void accept_response(const server_response& response);
    void forward_requests();
    void send_responses();
    
    void main_loop();
    
    std::vector<std::shared_ptr<endpoints::tcp_client>> m_clients;
    std::unordered_map<uint64_t, pending_request> m_pending_reqs;
    std::deque<client_request> m_requests;
    std::deque<server_response> m_responses;
    
    std::mutex m_req_mx;
    std::mutex m_resp_mx;
    std::mutex m_pend_mx;
    
    std::future<void> m_stop_sync;
    std::atomic_bool m_is_stopped;
    
    decltype(m_clients)::iterator get_next_client();

    decltype(m_clients)::iterator m_curr_client;
};

}
}
