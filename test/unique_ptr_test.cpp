#define STRONG_MEMORY_ORDERING_FLAG true
#define DEBUG_MODE_FLAG true

#include <smart_pointer/unique_ptr.h>
#include <iostream>

class NonDefaultCharMemoryManager
{
    public:

        NonDefaultCharMemoryManager(int){}

        constexpr void deallocate(void * memblk) noexcept
        {
            std::free(memblk);
        }
};

class CharMemoryManager
{
    public:

        constexpr CharMemoryManager() = default;
        constexpr CharMemoryManager(const NonDefaultCharMemoryManager&){}

        constexpr void deallocate(void * memblk) noexcept
        {
            std::free(memblk);
        }
};

static inline intmax_t lifetime_counter{};

class Foo
{
    private:

        int value;
    
    public:

        Foo(int value): value(value)
        {
            lifetime_counter++;
        }

        virtual ~Foo() noexcept
        {
            lifetime_counter--;
        }

        auto get_foo_value() -> int
        {
            return this->value;
        }
};

class Bar: public Foo
{
    private:

        int value;
    
    public:

        Bar(int foo_value, int bar_value): Foo(foo_value), value(bar_value){}

        auto get_bar_value() -> int
        {
            return this->value;
        }
};

template <class T, class ...Args>
auto allocate(Args&& ...args) -> T *
{
    size_t mem_sz   = sizeof(T);
    void * memblk   = std::malloc(mem_sz);

    return new (memblk) T(std::forward<Args>(args)...);
}

template <class T>
void deallocate(T * obj) noexcept
{
    std::destroy_at(obj);
    std::free(obj);
}

template <class T, class T1>
using unique_ptr = smart_pointer::unique_ptr_implementation::unique_ptr<T, T1>;

void test_polymorphic_default_manager()
{
    unique_ptr<Foo, CharMemoryManager> foo_obj(allocate<Bar>(1, 2));

    if (foo_obj->get_foo_value() != 1)
    {
        std::cout << "mayday, mismatched foo value\n";
        std::abort();
    }

}

void test_normal_default_manager()
{
    unique_ptr<Bar, CharMemoryManager> bar_obj(allocate<Bar>(1, 2));

    if (bar_obj->get_foo_value() != 1)
    {
        std::cout << "mayday, mismatched foo value\n";
        std::abort();
    }

    if (bar_obj->get_bar_value() != 2)
    {
        std::cout << "mayday, mismatched bar value\n";
        std::abort();
    }
}

void test_polymorphic_nondefault_manager()
{
    unique_ptr<Foo, NonDefaultCharMemoryManager> foo_obj(allocate<Bar>(1, 2), NonDefaultCharMemoryManager(int{}));

    if (foo_obj->get_foo_value() != 1)
    {
        std::cout << "mayday, mismatched foo value\n";
        std::abort();
    }
}

void test_normal_nondefault_manager()
{
    unique_ptr<Bar, NonDefaultCharMemoryManager> bar_obj(allocate<Bar>(1, 2), NonDefaultCharMemoryManager(int{}));

    if (bar_obj->get_foo_value() != 1)
    {
        std::cout << "mayday, mismatched foo value\n";
        std::abort();
    }

    if (bar_obj->get_bar_value() != 2)
    {
        std::cout << "mayday, mismatched bar value\n";
        std::abort();
    }
}

void test_polymorphic_different_allocator()
{ 
    unique_ptr<Foo, CharMemoryManager> obj_1;

    {
        unique_ptr<Foo, CharMemoryManager> foo_obj(allocate<Bar>(1, 2));

        if (foo_obj->get_foo_value() != 1)
        {
            std::cout << "mayday, mismatched foo value\n";
            std::abort();
        }

        obj_1 = std::move(foo_obj);
    }

    {
        unique_ptr<Bar, NonDefaultCharMemoryManager> bar_obj(allocate<Bar>(1, 2), NonDefaultCharMemoryManager(int{}));

        if (bar_obj->get_foo_value() != 1)
        {
            std::cout << "mayday, mismatched foo value\n";
            std::abort();
        }

        if (bar_obj->get_bar_value() != 2)
        {
            std::cout << "mayday, mismatched bar value\n";
            std::abort();
        }

        obj_1 = std::move(bar_obj);
        unique_ptr<Foo, CharMemoryManager> foo_obj = std::move(obj_1);

        if (foo_obj->get_foo_value() != 1)
        {
            std::cout << "mayday, mismatched foo value\n";
            std::abort();
        }
    }
}

void test_polymorphic_same_allocator()
{ 
    unique_ptr<Foo, CharMemoryManager> obj_1;

    {
        unique_ptr<Foo, CharMemoryManager> foo_obj(allocate<Bar>(1, 2));

        if (foo_obj->get_foo_value() != 1)
        {
            std::cout << "mayday, mismatched foo value\n";
            std::abort();
        }

        obj_1 = std::move(foo_obj);
    }

    unique_ptr<Bar, CharMemoryManager> obj_2;

    {
        unique_ptr<Bar, CharMemoryManager> bar_obj(allocate<Bar>(1, 2));

        if (bar_obj->get_foo_value() != 1)
        {
            std::cout << "mayday, mismatched foo value\n";
            std::abort();
        }

        if (bar_obj->get_bar_value() != 2)
        {
            std::cout << "mayday, mismatched bar value\n";
            std::abort();
        }

        obj_2 = std::move(bar_obj);
    }

    {
        obj_1 = std::move(obj_2);
        unique_ptr<Foo, CharMemoryManager> foo_obj = std::move(obj_1);

        if (foo_obj->get_foo_value() != 1)
        {
            std::cout << "mayday, mismatched foo value\n";
            std::abort();
        }
    }
}

void test_normal_different_allocator()
{
    unique_ptr<Foo, CharMemoryManager> obj_1;

    {
        unique_ptr<Foo, CharMemoryManager> foo_obj(allocate<Bar>(1, 2));

        if (foo_obj->get_foo_value() != 1)
        {
            std::cout << "mayday, mismatched foo value\n";
            std::abort();
        }

        obj_1 = std::move(foo_obj);
    }

    {
        unique_ptr<Foo, NonDefaultCharMemoryManager> bar_obj(allocate<Bar>(1, 2), NonDefaultCharMemoryManager(int{}));

        if (bar_obj->get_foo_value() != 1)
        {
            std::cout << "mayday, mismatched foo value\n";
            std::abort();
        }

        obj_1 = std::move(bar_obj);
        unique_ptr<Foo, CharMemoryManager> foo_obj = std::move(obj_1);

        if (foo_obj->get_foo_value() != 1)
        {
            std::cout << "mayday, mismatched foo value\n";
            std::abort();
        }
    }
}

void test_normal_same_allocator()
{
    unique_ptr<Foo, CharMemoryManager> obj_1;

    {
        unique_ptr<Foo, CharMemoryManager> foo_obj(allocate<Bar>(1, 2));

        if (foo_obj->get_foo_value() != 1)
        {
            std::cout << "mayday, mismatched foo value\n";
            std::abort();
        }

        obj_1 = std::move(foo_obj);
    }

    unique_ptr<Foo, CharMemoryManager> obj_2;

    {
        unique_ptr<Foo, CharMemoryManager> bar_obj(allocate<Bar>(1, 2));

        if (bar_obj->get_foo_value() != 1)
        {
            std::cout << "mayday, mismatched foo value\n";
            std::abort();
        }

        obj_2 = std::move(bar_obj);
    }

    {
        obj_1 = std::move(obj_2);
        unique_ptr<Foo, CharMemoryManager> foo_obj = std::move(obj_1);

        if (foo_obj->get_foo_value() != 1)
        {
            std::cout << "mayday, mismatched foo value\n";
            std::abort();
        }

        obj_1 = nullptr;
    }
}


int main()
{
    std::cout << "testing test_polymorphic_default_manager\n";
    test_polymorphic_default_manager();
    std::cout << "testing test_normal_default_manager\n";
    test_normal_default_manager();
    std::cout << "testing test_polymorphic_nondefault_manager\n";
    test_polymorphic_nondefault_manager();
    std::cout << "testing test_normal_nondefault_manager\n";
    test_normal_nondefault_manager();
    std::cout << "testing test_polymorphic_different_allocator\n";
    test_polymorphic_different_allocator();
    std::cout << "testing test_polymorphic_same_allocator\n";
    test_polymorphic_same_allocator();
    std::cout << "testing test_normal_different_allocator\n";
    test_normal_different_allocator();
    std::cout << "testing test_normal_same_allocator\n";
    test_normal_same_allocator();
    std::cout << "lifetime_counter > " << lifetime_counter << "\n";

}