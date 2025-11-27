#include "rr_forwarder.h"

#include "spdlog/spdlog.h"

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
    
    // Subscribe our acceptor to every client's giveaway event
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

    // Wait until all the operations are
    {
        std::scoped_lock l(m_pend_mx, m_req_mx, m_resp_mx);
    }

    // Write reports for remaining requests (with timeout message)
    for(const auto& pr : m_pending_reqs)
    {
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
    if(m_curr_client == m_clients.end())
        m_curr_client = m_clients.begin();

    // Start search from next
    auto it = m_curr_client + 1;
    for(;it != m_curr_client;)
    {
        if(it == m_clients.end())
        {
            it = m_clients.begin();
            continue;
        }

        // Skip inactive servers
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
        auto& req = m_requests.front();

        auto it = get_next_client();
        if(it == m_clients.end())
            return;
        
        uint64_t rid;
        {
            // Generate random request id
            std::lock_guard l2(m_pend_mx);
            do
            {
                rid = id_distr(rand_eng);
            } while (m_pending_reqs.contains(rid));

            // Get timestamp and fill pending request info, then store the latter
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


            spdlog::trace("Scheduled request #{0:x}: {1}:{2} -> {3}:{4}",
                rid,
                pr.client_addr.to_string(), pr.client_port,
                pr.server_addr.to_string(), pr.server_port
            );
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

        // Find the associated entry and remove it if it exists
        pending_request pr;
        {
            std::lock_guard l(m_pend_mx);
            if(!m_pending_reqs.contains(resp.request_id))
            {
                spdlog::warn("Unknown request #{0:x}", resp.request_id);

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

        // Build EDR report and notify listeners
        aux::edr edr
        {
            .arrival_time_ms = pr.arrival_time_ms,
            .tcp_resp_dur_us = response_time_us,
            .client_addr = pr.client_addr,
            .server_addr = pr.server_addr,
            .client_port = pr.client_port,
            .server_port = pr.server_port
        };
        edr_report_evt.invoke(edr);

        if(resp.resp_timestamp_us != TIMESTAMP_TIMEOUT)
        {
            spdlog::trace("Sending request #{0:x} back from {1}:{2} to {3}:{4}",
                pr.request_id,
                pr.server_addr.to_string(), pr.server_port,
                pr.client_addr.to_string(), pr.client_port
            );
            send_back_evt.invoke(pr.listener_id, pr.client_addr, pr.client_port, resp.payload);
        }
        else
        {
            spdlog::warn("Request #{0:x} has expired", pr.request_id);
        }
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

    spdlog::debug("Cleaning up TCP clients' response handlers");
    for(const auto& cl : m_clients)
    {
        cl->resp_giveaway_evt.unsubscribe(this, &rr_forwarder::accept_response);
    }
}

}
}
