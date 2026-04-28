#define STRONG_MEMORY_ORDERING_FLAG true
#define DEBUG_MODE_FLAG true

#include <iostream>
#include <smart_pointer/shared_ptr.h>
#include <stl_extension/stdx.h>
// class NonDefaultCharMemoryManager
// {
//     public:

//         NonDefaultCharMemoryManager(int){}

//         constexpr void deallocate(void * memblk) noexcept
//         {
//             std::free(memblk);
//         }
// };

static inline intmax_t allocation_counter = 0;

class CharMemoryManager
{
    public:

        constexpr CharMemoryManager() = default;
        constexpr CharMemoryManager(int){}

        template <class T>
        constexpr auto allocate(size_t sz) -> void *
        {
            allocation_counter++;
            return std::malloc(sz * sizeof(T));
        }

        constexpr void deallocate(void * memblk) noexcept
        {
            allocation_counter--;
            std::free(memblk);
        }
};

using NonDefaultCharMemoryManager = CharMemoryManager;

static inline intmax_t lifetime_counter{};

class Foo
{
    private:

        int value;
    
    public:

        Foo(int value): value(value)
        {
            std::cout << "initialization invoked\n";
            lifetime_counter++;
        }

        virtual ~Foo() noexcept
        {
            std::cout << "destroy invoked\n";
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
    void * rs = CharMemoryManager{}.template allocate<T>(1);

    return new (rs) T(std::forward<Args>(args)...);
}

template <class T, class T1>
using shared_ptr = smart_pointer::shared_ptr_implementation::shared_ptr<T, T1>;

void test_polymorphic_default_manager()
{
    shared_ptr<Foo, CharMemoryManager> foo_obj = smart_pointer::shared_ptr_implementation::allocate_shared<Foo>(CharMemoryManager{}, 1);

    if (foo_obj->get_foo_value() != 1)
    {
        std::cout << "mayday, mismatched foo value\n";
        std::abort();
    }

}

void test_normal_default_manager()
{
    shared_ptr<Bar, CharMemoryManager> bar_obj(allocate<Bar>(1, 2));

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
    shared_ptr<Foo, NonDefaultCharMemoryManager> foo_obj(allocate<Bar>(1, 2), NonDefaultCharMemoryManager(int{}));

    if (foo_obj->get_foo_value() != 1)
    {
        std::cout << "mayday, mismatched foo value\n";
        std::abort();
    }
}

void test_normal_nondefault_manager()
{
    shared_ptr<Bar, NonDefaultCharMemoryManager> bar_obj(allocate<Bar>(1, 2), NonDefaultCharMemoryManager(int{}));

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
    shared_ptr<Foo, CharMemoryManager> obj_1;

    {
        shared_ptr<Foo, CharMemoryManager> foo_obj(allocate<Bar>(1, 2));

        if (foo_obj->get_foo_value() != 1)
        {
            std::cout << "mayday, mismatched foo value\n";
            std::abort();
        }

        obj_1 = std::move(foo_obj);
    }

    {
        shared_ptr<Bar, NonDefaultCharMemoryManager> bar_obj(allocate<Bar>(1, 2), NonDefaultCharMemoryManager(int{}));

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
        shared_ptr<Foo, CharMemoryManager> foo_obj = std::move(obj_1);

        if (foo_obj->get_foo_value() != 1)
        {
            std::cout << "mayday, mismatched foo value\n";
            std::abort();
        }
    }
}

void test_polymorphic_same_allocator()
{ 
    shared_ptr<Foo, CharMemoryManager> obj_1;

    {
        shared_ptr<Foo, CharMemoryManager> foo_obj(allocate<Bar>(1, 2));

        if (foo_obj->get_foo_value() != 1)
        {
            std::cout << "mayday, mismatched foo value\n";
            std::abort();
        }

        obj_1 = std::move(foo_obj);
    }

    shared_ptr<Bar, CharMemoryManager> obj_2;

    {
        shared_ptr<Bar, CharMemoryManager> bar_obj(allocate<Bar>(1, 2));

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
        shared_ptr<Foo, CharMemoryManager> foo_obj = std::move(obj_1);

        if (foo_obj->get_foo_value() != 1)
        {
            std::cout << "mayday, mismatched foo value\n";
            std::abort();
        }
    }
}

void test_normal_different_allocator()
{
    shared_ptr<Foo, CharMemoryManager> obj_1;

    {
        shared_ptr<Foo, CharMemoryManager> foo_obj(allocate<Bar>(1, 2));

        if (foo_obj->get_foo_value() != 1)
        {
            std::cout << "mayday, mismatched foo value\n";
            std::abort();
        }

        obj_1 = std::move(foo_obj);
    }

    {
        shared_ptr<Foo, NonDefaultCharMemoryManager> bar_obj(allocate<Bar>(1, 2), NonDefaultCharMemoryManager(int{}));

        if (bar_obj->get_foo_value() != 1)
        {
            std::cout << "mayday, mismatched foo value\n";
            std::abort();
        }

        obj_1 = std::move(bar_obj);
        shared_ptr<Foo, CharMemoryManager> foo_obj = std::move(obj_1);

        if (foo_obj->get_foo_value() != 1)
        {
            std::cout << "mayday, mismatched foo value\n";
            std::abort();
        }
    }
}

void test_normal_same_allocator()
{
    shared_ptr<Foo, CharMemoryManager> obj_1;

    {
        shared_ptr<Foo, CharMemoryManager> foo_obj(allocate<Bar>(1, 2));

        if (foo_obj->get_foo_value() != 1)
        {
            std::cout << "mayday, mismatched foo value\n";
            std::abort();
        }

        obj_1 = std::move(foo_obj);
    }

    shared_ptr<Foo, CharMemoryManager> obj_2;

    {
        shared_ptr<Foo, CharMemoryManager> bar_obj(allocate<Bar>(1, 2));

        if (bar_obj->get_foo_value() != 1)
        {
            std::cout << "mayday, mismatched foo value\n";
            std::abort();
        }

        obj_2 = std::move(bar_obj);
    }

    {
        obj_1 = std::move(obj_2);
        shared_ptr<Foo, CharMemoryManager> foo_obj = std::move(obj_1);

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
    std::cout << "allocation_counter > " << allocation_counter << "\n";

}