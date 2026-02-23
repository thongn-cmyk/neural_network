#ifndef __NETWORK_RAII_X_H__
#define __NETWORK_RAII_X_H__

//define HEADER_CONTROL -1

#include <type_traits> 
#include <utility>

namespace dg_sock{

    template <class T>
    struct base_type: std::enable_if<true, T>{};

    template <class T>
    struct base_type<const T>: base_type<T>{};

    template <class T>
    struct base_type<volatile T>: base_type<T>{};

    template <class T>
    struct base_type<T&>: base_type<T>{};

    template <class T>
    struct base_type<T&&>: base_type<T>{};

    template <class T>
    using base_type_t = typename base_type<T>::type;
    
    template <class T, class = void>
    struct is_base_type: std::false_type{}; 

    template <class T>
    struct is_base_type<T, std::void_t<std::enable_if_t<std::is_same_v<T, base_type_t<T>>>>>: std::true_type{};

    template <class T>
    static inline constexpr bool is_base_type_v = is_base_type<T>::value;

    template <class ResourceType, class ResourceDeallocator>
    class unique_resource{
    
        private:

            ResourceType resource;
            ResourceDeallocator deallocator;
            bool responsibility_flag;

        public:

            static_assert(!std::is_same_v<ResourceType, bool>);
            static_assert(std::is_trivial_v<ResourceType>);
            static_assert(std::is_nothrow_invocable_v<ResourceDeallocator, std::add_lvalue_reference_t<ResourceType>>);
            static_assert(std::is_nothrow_move_constructible_v<ResourceDeallocator>);
            static_assert(dg_sock::is_base_type_v<ResourceDeallocator>);

            using self = unique_resource;

            template <class DelArg = ResourceDeallocator, std::enable_if_t<std::is_nothrow_default_constructible_v<DelArg>, bool> = true>
            unique_resource() noexcept: resource(), 
                                        deallocator(), 
                                        responsibility_flag(false){}

            unique_resource(ResourceType resource, ResourceDeallocator deallocator) noexcept: resource(resource),
                                                                                              deallocator(std::move(deallocator)),
                                                                                              responsibility_flag(true){}

            unique_resource(const self&) = delete;
            unique_resource(self&& other) noexcept: resource(other.resource),
                                                    deallocator(std::move(other.deallocator)),
                                                    responsibility_flag(std::exchange(other.responsibility_flag, false)){}

            ~unique_resource() noexcept{

                if (this->responsibility_flag){
                    this->deallocator(this->resource);
                }            
            }

            self& operator =(const self&) = delete;

            self& operator =(self&& other) noexcept{

                if (this != std::addressof(other)){
                    if (this->responsibility_flag){
                        this->deallocator(this->resource);
                    }

                    this->resource              = other.resource;
                    this->deallocator           = std::move(other.deallocator);
                    this->responsibility_flag   = std::exchange(other.responsibility_flag, false);
                }

                return *this;
            }

            auto value() const noexcept -> ResourceType{

                if constexpr(DEBUG_MODE_FLAG){
                    if (!this->responsibility_flag){
                        std::abort();
                    }
                }

                return this->resource;
            }

            operator ResourceType() const noexcept{

                return this->value();
            }

            operator bool() const noexcept{

                return this->responsibility_flag;
            }

            auto has_value() const noexcept -> bool{

                return this->responsibility_flag;
            }

            void release() noexcept{

                this->responsibility_flag = false;
            }
    };
}

#endif