#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
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
        // For erasing expired owners (weak pointers)
        std::vector<size_t> junktown;

        // Callbacks with hash
        {
        std::shared_lock l(m_cb_access_mx);
        for(auto& cb : m_cb)
        {
            auto erase_request = [&cb, &junktown] () {junktown.push_back(cb.first);};

            cb.second(erase_request, args...);
        }
        }

        // Callbacks without hash
        {
        std::shared_lock l(m_cb_access_mx);
        for(auto& cb : m_hashless_cb)
        {
            cb.second(args...);
        }
        }

        // Dispose of expired owners
        {
        std::shared_lock l(m_cb_access_mx);
        for(const size_t& hash : junktown)
        {
            m_cb.erase(hash);
        }
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
            // Dispose of this owner if it no longer exists
            auto shared = weak.lock();
            if(!shared)
            {
                del_req();
                return;
            }

            // Otherwise, call its member function
            (shared.get()->*hdlr)(args...);
        };

        std::unique_lock l(m_cb_access_mx);
        auto emp = m_cb.emplace(hash, wrapper);
    }

    template<class Owner>
    void unsubscribe(std::shared_ptr<Owner> optr, handler_member<Owner> hdlr)
    {
        auto h1 = std::hash<decltype(optr.get())>{};
        auto h2 = std::hash<unsigned long>{};

        size_t hash = h1(optr.get()) ^ h2(*reinterpret_cast<unsigned long*>(&hdlr));

        std::unique_lock l(m_cb_access_mx);
        auto it = m_cb.find(hash);
        if(it != m_cb.end())
        {
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
            // Do nothing to this object, since we don't own it
            (optr->*hdlr)(args...);
        };

        std::unique_lock l(m_cb_access_mx);
        auto emp = m_cb.emplace(hash, wrapper);
    }

    template<class Owner>
    void unsubscribe(Owner* optr, handler_member<Owner> hdlr)
    {
        auto h1 = std::hash<decltype(optr)>{};
        auto h2 = std::hash<unsigned long>{};

        size_t hash = h1(optr) ^ h2(*reinterpret_cast<unsigned long*>(&hdlr));

        std::unique_lock l(m_cb_access_mx);
        auto it = m_cb.find(hash);
        if(it != m_cb.end())
        {
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
            hdlr(args...);
        };

        std::unique_lock l(m_cb_access_mx);
        auto emp = m_cb.emplace(hash, wrapper);
    }

    void unsubscribe(handler_raw hdlr)
    {
        auto h = std::hash<unsigned long>{};
        size_t hash = h(*reinterpret_cast<unsigned long*>(&hdlr));

        std::unique_lock l(m_cb_access_mx);
        auto it = m_cb.find(hash);
        if(it != m_cb.end())
        {
            m_cb.erase(it);
        }
    }

    // Hashless part V V V

    void subscribe(size_t id, handler_func hdlr)
    {
        std::unique_lock l(m_cb_access_mx);
        auto emp = m_hashless_cb.emplace(id, hdlr);
    }

    void unsubscribe(size_t id)
    {
        std::unique_lock l(m_cb_access_mx);
        auto it = m_hashless_cb.find(id);
        if(it != m_hashless_cb.end())
        {
            m_hashless_cb.erase(it);
        }
    }

private:
    // Hashable functions are wrapped for convenience and loopback control (if possible)
    std::unordered_map<size_t, std::function<void(std::function<void()>, Args...)>> m_cb;

    // Functions that cannot be hashed (lambdas, etc.) are stored with a user-given id
    std::unordered_map<size_t, handler_func> m_hashless_cb;

    std::shared_mutex m_cb_access_mx;
};

}
}