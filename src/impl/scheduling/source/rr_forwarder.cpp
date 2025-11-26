#include "rr_forwarder.h"

#include <chrono>
#include <exception>
#include <random>
#include <thread>
#include <limits>

using uint64_t_lim = std::numeric_limits<uint64_t>;

static std::random_device rand_dev;
static std::default_random_engine rand_eng(rand_dev());
static std::uniform_int_distribution<uint64_t> id_distr(uint64_t_lim::min(), uint64_t_lim::max());

namespace utf
{
namespace scheduling
{

rr_forwarder::rr_forwarder(std::vector<std::shared_ptr<utf::endpoints::tcp_client>>&& clients) :
    m_clients(clients),
    m_curr_client(m_clients.begin())
{
    if(m_clients.empty())
        throw std::runtime_error("rr_forwarder: Empty clients list");
    
    for(const auto& cl : m_clients)
    {
        cl->resp_giveaway_evt.subscribe(this, &rr_forwarder::accept_response);
    }

    m_stop_sync = std::async(
        &rr_forwarder::main_loop,
        this
    );
}

rr_forwarder::~rr_forwarder()
{
    m_is_stopped.store(true);
    m_stop_sync.wait();

    {
        std::scoped_lock l(m_pend_mx, m_req_mx, m_resp_mx);
    }

    for(const auto& pr : m_pending_reqs)
    {
        // Build edr log
        aux::edr edr
        {
            .arrival_time_ms = pr.second.arrival_time_ms,
            .tcp_resp_dur_us = TIMESTAMP_TIMEOUT,
            .client_addr = pr.second.client_addr,
            .server_addr = pr.second.server_addr,
            .client_port = pr.second.client_port,
            .server_port = pr.second.server_port
        };

        edr_report_evt.invoke(edr);
    }
}

void rr_forwarder::schedule(const client_request& req)
{
    std::lock_guard l(m_req_mx);
    m_requests.push_back(req);
}

void rr_forwarder::schedule(client_request&& req)
{
    std::lock_guard l(m_req_mx);
    m_requests.push_back(std::move(req));
}

void rr_forwarder::accept_response(const server_response& response)
{
    std::lock_guard l (m_resp_mx);
    m_responses.push_back(response);
}

decltype(rr_forwarder::m_clients)::iterator rr_forwarder::get_next_client()
{
    auto it = m_curr_client + 1;
    for(;it != m_curr_client;)
    {
        if(it == m_clients.end())
        {
            it = m_clients.begin();
            continue;
        }

        if(!it->get()->is_connected())
        {
            ++it;
            continue;
        }

        m_curr_client = it;
        return m_curr_client;
    }
    
    if(m_curr_client->get()->is_connected())
    {
        return m_curr_client;
    }
    return m_clients.end();
}

void rr_forwarder::forward_requests()
{
    std::lock_guard l1(m_req_mx);
    while(!m_requests.empty())
    {
        if(m_curr_client == m_clients.end())
            m_curr_client = m_clients.begin();

        auto& req = m_requests.front();

        auto it = get_next_client();
        if(it == m_clients.end())
            return;
        

        uint64_t rid;
        {
            std::lock_guard l2(m_pend_mx);
            do
            {
                rid = id_distr(rand_eng);
            } while (m_pending_reqs.contains(rid));

            using namespace chrono;
            uint64_t current_time_us =
                duration_cast<microseconds>(system_clock::now().time_since_epoch()).count();
            pending_request pr
            {
                .request_id = rid,
                .listener_id = req.listener_id,
                .client_port = req.client_port,
                .server_port = it->get()->get_port(),
                .client_addr = req.client_addr,
                .server_addr = it->get()->get_address(),
                .arrival_time_ms = req.arr_timestamp_ms,
                .fwd_time_us = current_time_us
            };
            m_pending_reqs.emplace(rid, pr);
        }
        it->get()->send(rid, req.payload.begin(), req.payload.end());
        m_requests.pop_front();
    }
}

void rr_forwarder::send_responses()
{
    std::lock_guard l(m_resp_mx);
    while(!m_responses.empty())
    {
        const auto& resp = m_responses.front();

        pending_request pr;
        {
            std::lock_guard l(m_pend_mx);
            if(!m_pending_reqs.contains(resp.request_id))
            {
                m_responses.pop_front();
                continue;
            }

            auto it = m_pending_reqs.find(resp.request_id);
            pr = it->second;
            m_pending_reqs.erase(it);
        }

        auto response_time_us =
            (resp.resp_timestamp_us == TIMESTAMP_TIMEOUT) ?
            TIMESTAMP_TIMEOUT : (resp.resp_timestamp_us - pr.fwd_time_us);

        aux::edr edr
        {
            .arrival_time_ms = pr.arrival_time_ms,
            .tcp_resp_dur_us = response_time_us,
            .client_addr = pr.client_addr,
            .server_addr = pr.server_addr,
            .client_port = pr.client_port,
            .server_port = pr.server_port
        };
        
        send_back_evt.invoke(pr.listener_id, pr.client_addr, pr.client_port, resp.payload);
        edr_report_evt.invoke(edr);
        m_responses.pop_front();
    }
}

void rr_forwarder::main_loop()
{
    for(;;)
    {
        if(m_is_stopped.load())
            break;
        
        forward_requests();
        send_responses();
        
        std::this_thread::yield();
    }

    for(const auto& cl : m_clients)
    {
        cl->resp_giveaway_evt.unsubscribe(this, &rr_forwarder::accept_response);
    }
}

}
}
