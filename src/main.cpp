#include "endpoints/include/endpoint_impl.h"
#include "rr_forwarder.h"
#include "edr_logger.h"
#include "configuration.h"

#include <boost/thread.hpp>
#include <boost/program_options.hpp>

#include <spdlog/spdlog.h>

#include <csignal>
#include <iostream>
#include <functional>
#include <future>
#include <thread>
#include <vector>

enum cb_id
{
    send_back,
    log_edr
};

using namespace boost::asio;
using namespace utf::endpoints;

namespace po = boost::program_options;

std::function<void()> destroyer;

void sig_handler(int sig)
{
    spdlog::warn("Received signal {0}", strsignal(sig));
    destroyer();
}

int main(int argc, char** argv)
{
    po::options_description desc("Allowed options");
    desc.add_options()
        ("config", po::value<std::string>()->default_value("./cfg.json"), "Path to configuration file");
    
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);

    auto json_val = utf::aux::parse_json(vm.at("config").as<std::string>());
    if(!json_val)
    {
        spdlog::critical("Can't read configuration from {0}", vm.at("config").as<std::string>());
        std::cout << desc;
        return -1;
    }
    auto config = utf::aux::read_config(*json_val);

    std::cout << config;
    if(!utf::aux::validate_config(config))
    {
        spdlog::critical("Confiiguration is invalid");
        return -1;
    }

    spdlog::set_level(config.logging_lvl);

    io_context ioc_tcp;
    io_context ioc_udp;

    std::vector<std::shared_ptr<tcp_client>> tcp_clients;
    tcp_clients.reserve(config.tcp_clients.size());
    for(const auto& client : config.tcp_clients)
    {
        tcp_clients.push_back(std::make_shared<tcp_client>(
            ioc_tcp,
            ip::tcp::endpoint(client.ipv4, client.port),
            config.connection_timeout_ms,
            config.response_timeout_ms
        ));
    }

    std::vector<std::shared_ptr<udp_server>> udp_servers;
    udp_servers.reserve(config.udp_ports.size());
    for(uint32_t i = 0; i < config.udp_ports.size(); ++i)
    {
        udp_servers.push_back(std::make_shared<udp_server>(
            ioc_udp, config.udp_ports.at(i), i
        ));
    }

    auto fwdr = std::make_shared<utf::scheduling::rr_forwarder>(std::move(tcp_clients));

    std::shared_ptr<utf::aux::edr_logger> edr_logger = nullptr;
    if(!config.log_file_path.empty())
    {
        std::ofstream ofs(config.log_file_path);
        if(ofs.is_open())
        {
            edr_logger = std::make_shared<utf::aux::edr_logger>(std::move(ofs));
            fwdr->edr_report_evt.subscribe(
                cb_id::log_edr, 
                [&edr_logger](const utf::aux::edr& edr)
                {
                    *edr_logger << edr;
                }
            );
        }
    }
    
    fwdr->send_back_evt.subscribe(
        cb_id::send_back,
        [&udp_servers](uint32_t id, boost::asio::ip::address_v4 addr, uint16_t port, const std::vector<char>& payload)
        {
            udp_servers.at(id)->send(boost::asio::ip::udp::endpoint(addr, port), payload.begin(), payload.end());
        }
    );

    for(const auto& server: udp_servers)
    {
        server->incoming_req_evt.subscribe(fwdr, &utf::scheduling::rr_forwarder::schedule);
    }

    destroyer =
    [&]()
    {
        spdlog::info("Finalizing execution");

        ioc_udp.stop();
        ioc_tcp.stop();
        fwdr.reset();
    };

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    auto conc = std::thread::hardware_concurrency();
    decltype(conc) num_threads = 1;
    num_threads += conc > 4 ? conc / 4 : 0;
    boost::thread_group tg;
    for (decltype(conc) i = 0; i < num_threads; ++i)
        tg.create_thread(boost::bind(&io_context::run, &ioc_tcp));
    
    ioc_udp.run();
    tg.join_all();

    spdlog::info("Exiting");
    return 0;
}