#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <limits>
#include <iostream>

#include "json_parser.h"

#include <boost/asio/ip/address_v4.hpp>

#include <spdlog/spdlog.h>

namespace utf
{
namespace aux
{

struct tcp_client_config
{
    boost::asio::ip::address_v4 ipv4;
    uint16_t port;
};

struct config
{
    std::vector<uint16_t> udp_ports;
    std::vector<tcp_client_config> tcp_clients;

    uint32_t response_timeout_ms = 2000;
    uint32_t connection_timeout_ms = 5000;

    std::string log_file_path;
};

std::ostream& operator<<(std::ostream& os, const config& cfg)
{
    os << "Configuration:\n";
    os << "UDP ports:\n";
    for(const auto& elem : cfg.udp_ports)
    {
        os << elem << "\n";
    }

    os << "TCP clients:\n";
    for(const auto& elem : cfg.tcp_clients)
    {
        os << elem.ipv4 << ":" << elem.port << "\n";
    }

    os << "Response timeout (ms): " << cfg.response_timeout_ms << "\n";
    os << "Connection timeout (ms): " << cfg.connection_timeout_ms << "\n";

    os << "ERD log: " << (cfg.log_file_path.empty() ? "not provided" : cfg.log_file_path) << std::endl;

    return os;
}

config read_config(const boost::json::value& json_cfg)
{
    config cfg{};

    if(!json_cfg.is_object())
        return cfg;
    auto json_obj = json_cfg.as_object();

    auto udp_p = json_obj.find("udp_ports");
    auto tcp_c = json_obj.find("tcp_clients");
    auto log_p = json_obj.find("edr_log");
    auto rsp_t = json_obj.find("response_timeout_ms");
    auto cnn_t = json_obj.find("connection_timeout_ms");

    if(udp_p != json_obj.end() && udp_p->value().is_array())
    {
        for(const auto& elem : udp_p->value().as_array())
        {
            if(!elem.is_int64() || elem.as_int64() <= 0)
                continue;
            
            cfg.udp_ports.push_back(elem.as_int64());
        }
    }

    if(tcp_c != json_obj.end() && tcp_c->value().is_array())
    {
        for(const auto& elem : tcp_c->value().as_array())
        {
            if(!elem.is_object())
                continue;
            
            const auto& elem_obj = elem.as_object();
            auto addr = elem_obj.find("ipv4");
            auto port = elem_obj.find("port");

            if(addr == elem_obj.end() || port == elem_obj.end() ||
                !addr->value().is_string() || !port->value().is_int64())
                continue;

            const auto& addr_val = addr->value();
            const auto& port_val = port->value();

            tcp_client_config client_candidate;

            boost::system::error_code ec;
            client_candidate.ipv4 = boost::asio::ip::make_address_v4(addr_val.as_string(), ec);
            if(ec || port_val.as_int64() <= 0)
                continue;
            client_candidate.port = port_val.as_int64();
            
            cfg.tcp_clients.emplace_back(std::move(client_candidate));
        }
    }

    if(log_p != json_obj.end() && log_p->value().is_string())
    {
        const auto& log_p_str = log_p->value().as_string();
        cfg.log_file_path = std::string(log_p_str.begin(), log_p_str.end());
    }

    using rsp_t_lim = std::numeric_limits<decltype(cfg.response_timeout_ms)>;
    using cnn_t_lim = std::numeric_limits<decltype(cfg.connection_timeout_ms)>;

    if(rsp_t != json_obj.end() && rsp_t->value().is_int64())
    {
        const auto& rsp_t_val = rsp_t->value().as_int64();
        if(rsp_t_val > 0)
            cfg.response_timeout_ms = rsp_t_val > rsp_t_lim::max() ? rsp_t_lim::max() : rsp_t_val;
    }

    if(cnn_t != json_obj.end() && cnn_t->value().is_int64())
    {
        const auto& cnn_t_val = cnn_t->value().as_int64();
        if(cnn_t_val > 0)
            cfg.connection_timeout_ms = cnn_t_val > cnn_t_lim::max() ? cnn_t_lim::max() : cnn_t_val;
    }

    return cfg;
}

bool validate_config(const config& cfg)
{
    if(cfg.udp_ports.empty())
    {
        spdlog::error("UDP ports list is empty");
        return false;
    }
    if(cfg.tcp_clients.empty())
    {
        spdlog::error("TCP clients list is empty");
        return false;
    }
    return true;
}

}
}
