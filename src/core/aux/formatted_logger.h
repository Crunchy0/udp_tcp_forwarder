#pragma once

#include <type_traits>

namespace utf
{
namespace core
{
namespace aux
{

template<typename T>
class formatted_logger
{
public:
    using fmt_t = T;

    virtual ~formatted_logger() = default;

private:
    template<typename LgrT, typename FmtT>
    friend std::enable_if_t<std::is_base_of_v<formatted_logger<FmtT>, LgrT>, LgrT&>
    operator<<(LgrT& l, const FmtT& data);

    virtual void write(const fmt_t& data) = 0;
};

template <typename LgrT, typename FmtT>
std::enable_if_t<std::is_base_of_v<formatted_logger<FmtT>, LgrT>, LgrT&>
operator<<(LgrT& l, const FmtT& data)
{
    static_cast<formatted_logger<FmtT>&>(l).write(data);
    return l;
}

}
}
}
