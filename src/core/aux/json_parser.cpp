#include "json_parser.h"

namespace utf
{
namespace aux
{

std::optional<boost::json::value> parse_json(const std::string& fn)
{
    std::ifstream ifs(fn);
    if(!ifs.is_open())
    {
        return std::nullopt;
    }

    boost::system::error_code ec;
    parser.reset();

    std::string line;
    while(std::getline(ifs, line))
    {
        parser.write(line, ec);
        if(ec)
        {
            return std::nullopt;
        }
    }
    
    parser.finish(ec);
    if(ec)
    {
        return std::nullopt;
    }
    
    boost::json::value config = parser.release();
    if(!config.is_object())
    {
        return std::nullopt;
    }
    return config;
}

}
}
