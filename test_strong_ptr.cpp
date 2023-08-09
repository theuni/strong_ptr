// Copyright (c) 2017-2023 Cory Fields
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "strong_ptr.h"
#include <cassert>

class my_struct
{
public:
    my_struct(int){}
    my_struct() = default;
    ~my_struct()
    {
        m_valid = false;
    }
    bool valid() const
    {
        return m_valid;
    }
private:
    bool m_valid = true;
};

class Deleter
{
public:
    explicit Deleter(bool& deleted) : m_deleted{deleted}{}

    void operator()(std::nullptr_t)
    {
        m_deleted = true;
    }
    template <typename T>
    void operator()(T* ptr)
    {
        delete ptr;
        m_deleted = true;
    }
private:
    bool& m_deleted;
};


static void test_construction()
{
    // typical construction
    strong_ptr<my_struct> strong(new my_struct());
    assert(strong);
    assert(strong->valid());
    decay_ptr<my_struct> degraded(std::move(strong));
    assert(!strong);
    assert(degraded);
    assert(degraded.decayed());
    assert(degraded->valid());
}

static void test_deletion()
{
    // test typical deletion
    {
        bool deleted = false;
        {
            strong_ptr<my_struct> strong(new my_struct(), Deleter(deleted));
        }
        assert(deleted);
    }
    {
        bool deleted = false;
        strong_ptr<my_struct> strong(new my_struct(), Deleter(deleted));
        strong.reset();
        assert(deleted);
    }
    {
        bool deleted = false;
        strong_ptr<my_struct> strong(new my_struct(), Deleter(deleted));
        decay_ptr<my_struct> degraded(std::move(strong));
        assert(!deleted);
        degraded.reset();
        assert(deleted);
    }
    {
        bool deleted = false;
        strong_ptr<my_struct> strong(new my_struct(), Deleter(deleted));
        {
            auto shared = strong.get_shared();
        }
        decay_ptr<my_struct> degraded(std::move(strong));
        assert(!deleted);
        degraded.reset();
        assert(deleted);
    }
}

static void test_shared_outlives_strong()
{
    {
        bool deleted = false;
        strong_ptr<my_struct> strong(new my_struct(), Deleter(deleted));
        auto shared = strong.get_shared();
        {
            decay_ptr<my_struct> degraded(std::move(strong));
            assert(!degraded.decayed());
        }
        assert(!deleted);
        shared.reset();
        assert(deleted);
    }
}

static void test_use_count()
{
        strong_ptr<my_struct> strong(new my_struct());
        auto shared = strong.get_shared();
        assert(shared.use_count() == 2);
        auto strong2(std::move(strong));
        assert(shared.use_count() == 2);
        assert(strong2);
        assert(!strong);
        strong = std::move(strong2);
        assert(shared.use_count() == 2);
        assert(strong);
        assert(!strong2);
        decay_ptr<my_struct> degraded(std::move(strong));
        assert(shared.use_count() == 1);
        assert(!strong);
        assert(!degraded.decayed());
        shared.reset();
        assert(degraded.decayed());
}

static void test_wait()
{
    //TODO
}

int main()
{
    test_construction();
    test_deletion();
    test_shared_outlives_strong();
    test_use_count();
}
