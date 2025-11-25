#pragma once

#include <boost/json.hpp>

#include <fstream>
#include <optional>

namespace utf
{
namespace aux
{

static boost::json::stream_parser parser;

std::optional<boost::json::value> parse_json(const std::string& fn);

}
}
