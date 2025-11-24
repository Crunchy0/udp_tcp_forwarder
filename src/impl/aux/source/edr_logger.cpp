#include "edr_logger.h"

#include <exception>

namespace utf
{
namespace aux
{

edr_logger::edr_logger(const std::string& file_name)
{
    m_dest = std::ofstream(file_name);
    if(!m_dest.is_open())
    {
        throw std::runtime_error("Unable to open " + file_name);
    }
}

edr_logger::edr_logger(std::ofstream&& os) noexcept
{
    m_dest = std::move(os);
}

void edr_logger::write(const edr& edr_rep)
{
    m_dest <<
            edr_rep.arrival_time_ms << " " <<
            edr_rep.client_addr << ":" << edr_rep.client_port << " " <<
            edr_rep.server_addr << ":" << edr_rep.server_port << " " <<
            edr_rep.tcp_resp_dur_ms <<
            std::endl;
}

}
}