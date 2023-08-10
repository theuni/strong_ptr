// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_STRONGPTR_H
#define BITCOIN_STRONGPTR_H

#include <memory>
#include <cassert>
#include <condition_variable>
#include <mutex>

template <typename T>
class decay_ptr;

struct wake_type
{
    std::condition_variable m_cond{};
    std::mutex m_mut{};
};

template <typename T>
class strong_ptr
{
    struct shared_deleter {
        void operator()(T* /* unused */)
        {
            assert(m_wake);
            m_data.reset();
            {
                // Necessary dummy lock
                std::lock_guard<std::mutex> lock(m_wake->m_mut);
            }
            m_wake->m_cond.notify_all();
        }
        std::shared_ptr<void> m_data;
        std::shared_ptr<wake_type> m_wake;
    };

    friend class decay_ptr<T>;

    template <typename U, typename... Args>
    friend strong_ptr<U> make_strong(Args&&... args);

    template <typename U>
    explicit strong_ptr(std::shared_ptr<U>&& rhs) : m_data{std::move(rhs)}
    {
    }

public:
    using element_type = T;

    constexpr strong_ptr() : strong_ptr(nullptr) {}

    constexpr strong_ptr(std::nullptr_t) : m_shared{nullptr}, m_wake{std::make_shared<wake_type>()} {}

    template <typename U>
    explicit strong_ptr(U* ptr) : m_data(ptr), m_wake{std::make_shared<wake_type>()}
    {
    }

    template <typename Deleter>
    strong_ptr(std::nullptr_t ptr, Deleter deleter) : m_data{ptr, std::move(deleter)}, m_wake{std::make_shared<wake_type>()}
    {
    }

    template <typename U, typename Deleter>
    strong_ptr(U* ptr, Deleter deleter) : m_data{ptr, std::move(deleter)}, m_wake{std::make_shared<wake_type>()}
    {
    }

    template <typename U, typename Deleter>
    strong_ptr(std::unique_ptr<U, Deleter>&& rhs) : m_data{std::move(rhs)}, m_wake{std::make_shared<wake_type>()}
    {
        assert(m_wake);
    }

    template <typename U>
    strong_ptr(strong_ptr<U>&& rhs) : m_data{std::move(rhs.m_data)}, m_wake{std::move(rhs.m_wake)}, m_shared{std::move(rhs.m_shared)}
    {
    }
    strong_ptr(strong_ptr&& rhs) noexcept : m_data{std::move(rhs.m_data)}, m_wake{std::move(rhs.m_wake)}, m_shared{std::move(rhs.m_shared)} {}

    ~strong_ptr() = default;

    strong_ptr(const strong_ptr& rhs) = delete;
    strong_ptr& operator=(const strong_ptr& rhs) = delete;

    strong_ptr& operator=(strong_ptr&& rhs) noexcept
    {
        m_data = std::move(rhs.m_data);
        m_wake = std::move(rhs.m_wake);
        m_shared = std::move(rhs.m_shared);
        return *this;
    }

    template <typename U>
    strong_ptr& operator=(strong_ptr<U>&& rhs)
    {
        m_data = std::move(rhs.m_data);
        m_wake = std::move(rhs.m_wake);
        m_shared = std::move(rhs.m_shared);
        return *this;
    }

    template <typename U>
    strong_ptr& operator=(U* rhs)
    {
        reset(rhs);
        return *this;
    }

    template <typename U, typename Deleter>
    strong_ptr& operator=(std::unique_ptr<U, Deleter>&& rhs)
    {
        m_data = std::move(rhs);
        m_wake = std::make_shared<wake_type>();
        m_shared.reset(nullptr, shared_deleter{m_data, m_wake});
        return *this;
    }

    void reset()
    {
        m_data.reset();
        m_shared.reset();
        m_wake = std::make_shared<wake_type>();
    }
    void reset(std::nullptr_t)
    {
        reset();
    }
    template <typename U>
    void reset(U* ptr)
    {
        m_data.reset(ptr);
        m_wake = std::make_shared<wake_type>();
        m_shared.reset(nullptr, shared_deleter{m_data, m_wake});
    }
    template <typename U, typename Deleter>
    void reset(U* ptr, Deleter deleter)
    {
        m_data.reset(ptr, std::move(deleter));
        m_wake = std::make_shared<wake_type>();
        m_shared.reset(nullptr, shared_deleter{m_data, m_wake});
    }
    std::shared_ptr<T> get_shared() const
    {
        return std::shared_ptr<T>(m_shared, m_data.get());
    }
    T* operator*()
    {
        return m_data.operator*();
    }
    const T* operator*() const
    {
        return m_data.operator*();
    }
    T* operator->()
    {
        return m_data.operator->();
    }
    const T* operator->() const
    {
        return m_data.operator->();
    }
    const T* get() const
    {
        return m_data.get();
    }
    T* get()
    {
        return m_data.get();
    }
    explicit operator bool() const
    {
        return m_data.operator bool();
    }

private:
    std::shared_ptr<T> m_data;
    std::shared_ptr<wake_type> m_wake;
    std::shared_ptr<void> m_shared{nullptr, shared_deleter{m_data, m_wake}};
};

template <typename T>
class decay_ptr
{
public:
    constexpr decay_ptr() = default;
    constexpr decay_ptr(std::nullptr_t) : decay_ptr{} {}
    decay_ptr(decay_ptr&& rhs) noexcept : m_data{std::move(rhs.m_data)}, m_decaying{std::move(rhs.m_decaying)}, m_wake{std::move(rhs.m_wake)}
    {
    }

    template <typename U>
    decay_ptr(decay_ptr<U>&& rhs) : m_data{std::move(rhs.m_data)}, m_decaying{std::move(rhs.m_decaying)}, m_wake{std::move(rhs.m_wake)}
    {
    }

    decay_ptr(const decay_ptr&) = delete;
    decay_ptr& operator=(const decay_ptr&) = delete;

    template <typename U>
    decay_ptr(strong_ptr<U>&& ptr) : m_data{std::move(ptr.m_data)}, m_decaying{ptr.m_shared}, m_wake{std::move(ptr.m_wake)}
    {
        assert(m_wake);
        ptr.m_shared.reset();
    }

    ~decay_ptr() = default;

    template <typename U>
    decay_ptr& operator=(decay_ptr<U>&& rhs)
    {
        m_data = std::move(rhs.m_data);
        m_wake = std::move(rhs.m_wake);
        m_decaying = std::move(rhs.m_decaying);
        return *this;
    }
    decay_ptr& operator=(decay_ptr&& rhs) noexcept
    {
        m_data = std::move(rhs.m_data);
        m_wake = std::move(rhs.m_wake);
        m_decaying = std::move(rhs.m_decaying);
        return *this;
    }
    template <typename U>
    decay_ptr& operator=(strong_ptr<U>&& rhs)
    {
        m_data = std::move(rhs.m_data);
        m_wake = std::move(rhs.m_wake);
        m_decaying = rhs.m_shared;
        rhs.m_shared.reset();
        rhs.m_wake.reset();
        return *this;
    }
    bool decayed() const
    {
        return m_decaying.expired();
    }

    void wait()
    {
        assert(m_wake);
        std::unique_lock<std::mutex> lock(m_wake->m_mut);
        m_wake->m_cond.wait(lock);
    }

    template<class Predicate>
    void wait(Predicate stop_waiting)
    {
        assert(m_wake);
        std::unique_lock<std::mutex> lock(m_wake->m_mut);
        m_wake->m_cond.wait(lock, stop_waiting);
    }

    template<class Rep, class Period, class Predicate>
    bool wait_for(const std::chrono::duration<Rep, Period>& rel_time, Predicate stop_waiting)
    {
        assert(m_wake);
        std::unique_lock<std::mutex> lock(m_wake->m_mut);
        return m_wake->m_cond.wait_for(lock, rel_time, stop_waiting);
    }

    template<class Rep, class Period>
    std::cv_status wait_for(const std::chrono::duration<Rep, Period>& rel_time)
    {
        assert(m_wake);
        std::unique_lock<std::mutex> lock(m_wake->m_mut);
        return m_wake->m_cond.wait_for(lock, rel_time);
    }

    template<class Clock, class Duration>
    std::cv_status wait_until(const std::chrono::time_point<Clock, Duration>& timeout_time)
    {
        assert(m_wake);
        std::unique_lock<std::mutex> lock(m_wake->m_mut);
        return m_wake->m_cond.wait_until(lock, timeout_time);
    }

    template<class Clock, class Duration, class Predicate>
    bool wait_until(const std::chrono::time_point<Clock, Duration>& timeout_time, Predicate stop_waiting)
    {
        assert(m_wake);
        std::unique_lock<std::mutex> lock(m_wake->m_mut);
        return m_wake->m_cond.wait_until(lock, timeout_time, stop_waiting);
    }

    void reset()
    {
        m_decaying.reset();
        m_data.reset();
        m_wake.reset();
    }
    T* operator*()
    {
        return m_data.operator*();
    }
    const T* operator*() const
    {
        return m_data.operator*();
    }
    T* operator->()
    {
        return m_data.operator->();
    }
    const T* operator->() const
    {
        return m_data.operator->();
    }
    const T* get() const
    {
        return m_data.get();
    }
    T* get()
    {
        return m_data.get();
    }
    explicit operator bool() const
    {
        return m_data.operator bool();
    }

private:
    std::shared_ptr<T> m_data;
    std::weak_ptr<void> m_decaying;
    std::shared_ptr<wake_type> m_wake;
};

template <typename T, typename... Args>
inline strong_ptr<T> make_strong(Args&&... args)
{
    return strong_ptr<T>(std::make_unique<T>(std::forward<Args>(args)...));
}


#endif // BITCOIN_STRONGPTR_H
