#include "edr_logger.h"
#include "utf_core.h"

#include <exception>
#include <iomanip>

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
            edr_rep.server_addr << ":" << edr_rep.server_port << " ";
        
    if(edr_rep.tcp_resp_dur_us == TIMESTAMP_TIMEOUT)
    {
        m_dest << "timed_out";
    }
    else
    {
        m_dest << edr_rep.tcp_resp_dur_us / 1000 << "." <<
            std::setw(3) << std::setfill('0') << edr_rep.tcp_resp_dur_us % 1000 << "_ms";
    }
    m_dest << std::endl;
}

}
}