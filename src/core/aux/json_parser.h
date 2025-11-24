#pragma once

#include <boost/json/src.hpp>

#include <fstream>

namespace json = boost::json;

namespace utf
{
namespace aux
{

std::optional<json::value> parse_json(const std::string& fn)
{
    static json::stream_parser parser;

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
    
    json::value config = parser.release();
    if(!config.is_object())
    {
        return std::nullopt;
    }
    return config;
}

}
}
