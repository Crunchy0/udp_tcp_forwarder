#pragma once

// #include <some_logging_lib>

#include <functional>
#include <memory>
#include <mutex>
#include <type_traits>
#include <vector>
#include <unordered_map>

namespace utf
{
namespace scheduling
{

template<class ... Args>
class event
{
    template <class Owner>
    using handler_member = void (Owner::*)(Args ...);    // Non-static methods
    using handler_raw = void (*)(Args ...);              // Static methods and plain functions
    using handler_func = std::function<void(Args ...)>;  // Functors

public:
    event() = default;
    ~event() = default;

    void invoke(Args... args)
    {
        std::vector<size_t> junktown;

        for(auto& cb : m_cb)
        {
            // This is for automatically erasing expired owners
            auto erase_request = [&cb, &junktown] () {junktown.push_back(cb.first);};

            cb.second(erase_request, args...);
        }

        for(auto& cb : m_hashless_cb)
        {
            cb.second(args...);
        }

        for(const size_t& hash : junktown)
        {
            // some logging info on erasing callback
            m_cb.erase(hash);
        }
    }

    template<class Owner>
    void subscribe(std::shared_ptr<Owner> optr, handler_member<Owner> hdlr)
    {
        auto h1 = std::hash<decltype(optr.get())>{};
        auto h2 = std::hash<unsigned long>{};

        size_t hash = h1(optr.get()) ^ h2(*reinterpret_cast<unsigned long*>(&hdlr));

        std::weak_ptr<Owner> weak = optr;

        auto wrapper =
        [weak, hdlr](std::function<void()> del_req, Args&&... args)
        {
            auto shared = weak.lock();
            if(!shared)
            {
                del_req();
                return;
            }

            (shared.get()->*hdlr)(args...);
        };

        auto emp = m_cb.emplace(hash, wrapper);
        // some logging info on emplacement
    }

    template<class Owner>
    void unsubscribe(std::shared_ptr<Owner> optr, handler_member<Owner> hdlr)
    {
        auto h1 = std::hash<decltype(optr.get())>{};
        auto h2 = std::hash<unsigned long>{};

        size_t hash = h1(optr.get()) ^ h2(*reinterpret_cast<unsigned long*>(&hdlr));

        auto it = m_cb.find(hash);
        if(it != m_cb.end())
        {
            // some logging info on erasing callback
            m_cb.erase(it);
        }
    }

    template<class Owner>
    void subscribe(Owner* optr, handler_member<Owner> hdlr)
    {
        auto h1 = std::hash<decltype(optr)>{};
        auto h2 = std::hash<unsigned long>{};

        size_t hash = h1(optr) ^ h2(*reinterpret_cast<unsigned long*>(&hdlr));

        auto wrapper =
        [optr, hdlr]([[maybe_unused]] std::function<void()> del_req, Args... args)
        {
            // (optr->*hdlr)(std::forward<Args...>(args...));
            (optr->*hdlr)(args...);
        };

        auto emp = m_cb.emplace(hash, wrapper);
        // some logging info on emplacement
    }

    template<class Owner>
    void unsubscribe(Owner* optr, handler_member<Owner> hdlr)
    {
        auto h1 = std::hash<decltype(optr)>{};
        auto h2 = std::hash<unsigned long>{};

        size_t hash = h1(optr) ^ h2(*reinterpret_cast<unsigned long*>(&hdlr));

        auto it = m_cb.find(hash);
        if(it != m_cb.end())
        {
            // some logging info on erasing callback
            m_cb.erase(it);
        }
    }

    void subscribe(handler_raw hdlr)
    {
        auto h = std::hash<unsigned long>{};
        size_t hash = h(*reinterpret_cast<unsigned long*>(&hdlr));

        auto wrapper =
        [hdlr]([[maybe_unused]] std::function<void()> del_req, Args&&... args)
        {
            // hdlr(std::forward<Args...>(args...));
            hdlr(args...);
        };

        auto emp = m_cb.emplace(hash, wrapper);
        // some logging info on emplacement
    }

    void unsubscribe(handler_raw hdlr)
    {
        auto h = std::hash<unsigned long>{};
        size_t hash = h(*reinterpret_cast<unsigned long*>(&hdlr));

        auto it = m_cb.find(hash);
        if(it != m_cb.end())
        {
            // some logging info on erasing callback
            m_cb.erase(it);
        }
    }

    // Hashless part V V V

    void subscribe(size_t id, handler_func hdlr)
    {
        auto emp = m_hashless_cb.emplace(id, hdlr);
        // some logging info on emplacement
    }

    void unsubscribe(size_t id)
    {
        auto it = m_hashless_cb.find(id);
        if(it != m_hashless_cb.end())
        {
            // some logging info on erasing callback
            m_hashless_cb.erase(it);
        }
    }

private:
    // Hashable functions are wrapped for convenience and loopback control (if possible)
    std::unordered_map<size_t, std::function<void(std::function<void()>, Args...)>> m_cb;

    // Functions that cannot be hashed (lambdas, etc.) are stored with a user-given id
    std::unordered_map<size_t, handler_func> m_hashless_cb;
};

}
}