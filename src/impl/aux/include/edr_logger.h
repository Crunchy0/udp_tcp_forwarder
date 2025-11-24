#pragma once

#include "json_parser.h"
#include "formatted_logger.h"

#include <boost/asio/ip/address.hpp>

#include <cstdint>
#include <ctime>
#include <fstream>

namespace utf
{
namespace aux
{

using namespace boost::asio;

struct edr
{
    uint64_t arrival_time_ms;
    uint64_t tcp_resp_dur_ms;

    ip::address_v4 client_addr;
    ip::address_v4 server_addr;
    
    uint16_t client_port;
    uint16_t server_port;
};

class edr_logger : public utf::aux::formatted_logger<edr>
{
public:
    edr_logger() = delete;
    edr_logger(const std::string& file_name);
    edr_logger(std::ofstream&& os) noexcept;

    edr_logger(const edr_logger& other) = delete;
    edr_logger(edr_logger&& other) = delete;
    edr_logger& operator=(const edr_logger& other) = delete;
    edr_logger& operator=(edr_logger&& other) = delete;

    ~edr_logger() override = default;

private:
    void write(const edr& edr_rep) override;

    std::ofstream m_dest;
};

}
}