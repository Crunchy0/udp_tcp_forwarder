#pragma once

#include "client_request.h"

namespace utf
{
namespace scheduling
{

class forwarder
{
public:
    virtual ~forwarder() = default;

    virtual void schedule(const client_request& req) = 0;
    virtual void schedule(client_request&& req) = 0;
};

}
}
