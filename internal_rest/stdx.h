#ifndef __STD_X_H__
#define __STD_X_H__

//define HEADER_CONTROL 0

#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <set>
#include <deque>
#include <atomic>
#include <mutex>
#include <functional>
#include <chrono>
#include <climits>
#include "network_raii_x.h"
#include <immintrin.h>
#include <utility>
#include <exception>
#include <thread>
#include "assert.h"
#include <sys/syscall.h>
#include <mutex_extension/fair_mutex.h>

namespace stdxx
{
    static inline constexpr bool IS_SAFE_INTEGER_CONVERSION_ENABLED                             = true;
    static inline constexpr bool IS_ATOMIC_FLAG_AS_SPINLOCK                                     = true;

    using namespace fair_mutex;

    template <class Lambda>
    inline void busy_wait(Lambda&& lambda)
    {
        (void) lambda;
    }

    class seq_cst_guard
    {
        public:

            inline __attribute__((always_inline)) seq_cst_guard() noexcept
            {
                std::atomic_signal_fence(std::memory_order_seq_cst);
            }

            seq_cst_guard(const seq_cst_guard&) = delete;
            seq_cst_guard(seq_cst_guard&&) = delete;

            inline __attribute__((always_inline)) ~seq_cst_guard() noexcept
            {
                std::atomic_signal_fence(std::memory_order_seq_cst);
            }

            seq_cst_guard& operator =(const seq_cst_guard&) = delete;
            seq_cst_guard& operator =(seq_cst_guard&&) = delete;
    };

    class memtransaction_guard
    {
        public:

            inline __attribute__((always_inline)) memtransaction_guard() noexcept
            {
                if constexpr(STRONG_MEMORY_ORDERING_FLAG)
                {
                    std::atomic_thread_fence(std::memory_order_seq_cst);
                }
                else
                {
                    std::atomic_thread_fence(std::memory_order_acquire);
                }
            }

            memtransaction_guard(const memtransaction_guard&) = delete;
            memtransaction_guard(memtransaction_guard&&) = delete;

            inline __attribute__((always_inline)) ~memtransaction_guard() noexcept
            {
                if constexpr(STRONG_MEMORY_ORDERING_FLAG)
                {
                    std::atomic_thread_fence(std::memory_order_seq_cst);
                }
                else
                {
                    std::atomic_thread_fence(std::memory_order_release);
                }
            }

            memtransaction_guard& operator =(const memtransaction_guard&) = delete;
            memtransaction_guard& operator =(memtransaction_guard&&) = delete;
    };

    template <class T>
    inline __attribute__((always_inline)) auto safe_ptr_access(T * ptr) noexcept -> T *
    {
        if (!ptr) [[unlikely]]
        {
            std::abort();
        }

        return ptr;
    }

    template <class T, std::enable_if_t<std::is_unsigned_v<T>, bool> = true>
    constexpr auto is_pow2(T value)
    {
        return value != 0u && (value & static_cast<T>(value - 1)) == 0u;
    }

    constexpr auto align_address(uintptr_t arithmetic_buf, uintptr_t alignment_sz) noexcept -> uintptr_t
    {
        assert(is_pow2(alignment_sz));

        uintptr_t FWD_SZ                = alignment_sz - 1u;
        uintptr_t MASK_VALUE            = ~FWD_SZ;
        uintptr_t fwd_arithmetic_buf    = (arithmetic_buf + FWD_SZ) & MASK_VALUE;

        return fwd_arithmetic_buf;
    }

    template <size_t ALIGNMENT_SZ>
    constexpr auto align_address(uintptr_t arithmetic_buf, std::integral_constant<size_t, ALIGNMENT_SZ>) noexcept -> uintptr_t
    {
        static_assert(is_pow2(ALIGNMENT_SZ));

        constexpr uintptr_t FWD_SZ          = ALIGNMENT_SZ - 1u;
        constexpr uintptr_t MASK_VALUE      = ~FWD_SZ;
        const uintptr_t fwd_arithmetic_buf  = (arithmetic_buf + FWD_SZ) & MASK_VALUE;

        return fwd_arithmetic_buf;
    }

    constexpr auto align_ptr(char * buf, size_t alignment_sz) noexcept -> char *
    {
        return reinterpret_cast<char *>(align_address(reinterpret_cast<uintptr_t>(buf), alignment_sz));
    }

    constexpr auto align_ptr(const char * buf, size_t alignment_sz) noexcept -> const char *
    {
        return reinterpret_cast<const char *>(align_address(reinterpret_cast<uintptr_t>(buf), alignment_sz));
    }

    template <size_t ALIGNMENT_SZ>
    constexpr auto align_ptr(char * buf, std::integral_constant<size_t, ALIGNMENT_SZ> alignment) noexcept -> char *
    {
        return reinterpret_cast<char *>(align_address(reinterpret_cast<uintptr_t>(buf), alignment));
    }

    template <size_t ALIGNMENT_SZ>
    constexpr auto align_ptr(const char * buf, std::integral_constant<size_t, ALIGNMENT_SZ> alignment) noexcept -> const char *
    {
        return reinterpret_cast<const char *>(align_address(reinterpret_cast<uintptr_t>(buf), alignment));
    }

    template <class T>
    inline __attribute__((always_inline)) auto to_const_reference(T& obj) noexcept -> decltype(auto){

        return std::as_const(obj);
    }

    template <class Destructor>
    inline auto resource_guard(Destructor destructor) noexcept
    {    
        static_assert(std::is_nothrow_move_constructible_v<Destructor>);
        static_assert(std::is_nothrow_invocable_v<Destructor>);

        auto backout_ld = [destructor_arg = std::move(destructor)](int) noexcept
        {
            destructor_arg();
        };

        return dg_sock::unique_resource<int, decltype(backout_ld)>(int{0}, std::move(backout_ld));
    }

    template <class T, class T1>
    constexpr auto pow2mod_unsigned(T lhs, T1 rhs) noexcept -> std::conditional_t<(sizeof(T) > sizeof(T1)), T, T1>
    {
        static_assert(std::is_unsigned_v<T>);
        static_assert(std::is_unsigned_v<T1>);
        
        using promoted_t = std::conditional_t<(sizeof(T) > sizeof(T1)), T, T1>;
        return static_cast<promoted_t>(lhs) & static_cast<promoted_t>(rhs - 1);
    }

    template <class T, std::enable_if_t<std::is_unsigned_v<T>, bool> = true>
    constexpr auto ulog2_aligned(T val) noexcept -> size_t
    {
        return std::countr_zero(val);
    }

    template <class T, std::enable_if_t<std::is_unsigned_v<T>, bool> = true>
    constexpr auto ulog2(T val) noexcept -> T
    {
        return static_cast<T>(sizeof(T) * CHAR_BIT - 1u) - static_cast<T>(std::countl_zero(val));
    }

    constexpr auto mul_ceil(size_t value, size_t multiplier) -> size_t
    {
        if (value == 0u)
        {
            return 0u;
        }

        if (multiplier == 0u)
        {
            throw std::invalid_argument("bad multiplier, 0");
        }

        size_t demoted_slot     = (value - 1u) / multiplier;
        size_t promoted_slot    = demoted_slot + 1u;

        return promoted_slot * multiplier;
    }

    template <class T, std::enable_if_t<std::is_unsigned_v<T>, bool> = true>
    static constexpr auto ceil2(T val) noexcept -> T{

        if (val < 2u) [[unlikely]]{
            return 1u;
        } else [[likely]]{
            T uplog_value = ulog2(static_cast<T>(val - 1u)) + 1u;
            return T{1u} << uplog_value;
        }
    }

    template <class T, std::enable_if_t<std::is_unsigned_v<T>, bool> = true>
    constexpr auto least_pow2_greater_equal_than(T val) noexcept -> T{

        return stdxx::ceil2(val);
    } 

    template <class T1, class T>
    constexpr auto safe_integer_cast(T value) noexcept -> T1{

        static_assert(std::numeric_limits<T>::is_integer);
        static_assert(std::numeric_limits<T1>::is_integer);

        if constexpr(IS_SAFE_INTEGER_CONVERSION_ENABLED){
            if constexpr(std::is_unsigned_v<T> && std::is_unsigned_v<T1>){
                (void) value;
            } else if constexpr(std::is_signed_v<T> && std::is_signed_v<T1>){
                (void) value;
            } else{
                if constexpr(std::is_signed_v<T>){
                    if constexpr(sizeof(T) > sizeof(T1)){
                        (void) value;
                    } else{
                        if (value < 0){
                            std::abort();
                        } else{
                            return value; //sizeof(signed) <= sizeof(unsigned)
                        }
                    }
                } else{
                    if constexpr(sizeof(T1) > sizeof(T)){
                        (void) value;
                    } else{
                        if (value > std::numeric_limits<T1>::max()){
                            std::abort();
                        } else{
                            return value; //sizeof(unsigned) >= sizeof(signed)
                        }
                    }
                }
            }

            if (value > std::numeric_limits<T1>::max()){
                std::abort();
            }

            if (value < std::numeric_limits<T1>::min()){
                std::abort();
            }

            return value;
        } else{
            return value;
        }
    }

    template <size_t BIT_SZ, class T, std::enable_if_t<std::is_unsigned_v<T>, bool> = true>
    constexpr auto low_bit(T value) noexcept -> T{

        constexpr size_t MAX_BIT_CAP = sizeof(T) * CHAR_BIT;
        static_assert(BIT_SZ <= MAX_BIT_CAP);

        if constexpr(BIT_SZ == MAX_BIT_CAP){
            return value & std::numeric_limits<T>::max(); 
        } else{
            constexpr T low_mask = (T{1u} << BIT_SZ) - 1;
            return value & low_mask;
        }
    }

    template <class T, size_t BIT_SIZE, std::enable_if_t<std::is_unsigned_v<T>, bool> = true>
    consteval auto lowones_bitgen(const std::integral_constant<size_t, BIT_SIZE>) noexcept -> T{

        static_assert(BIT_SIZE <= std::numeric_limits<T>::digits);

        if constexpr(BIT_SIZE == std::numeric_limits<T>::digits){
            return std::numeric_limits<T>::max();
        } else{
            return (T{1} << BIT_SIZE) - 1u;
        }
    }

    template <class T, std::enable_if_t<std::is_unsigned_v<T>, bool> = true>
    constexpr auto lowones_bitgen(size_t bit_size) noexcept -> T{

        assert(bit_size <= std::numeric_limits<T>::digits);

        if (bit_size == std::numeric_limits<T>::digits){
            return std::numeric_limits<T>::max();
        } else{
            return (T{1} << bit_size) - 1u;
        }
    }

    template <class T, std::enable_if_t<std::is_unsigned_v<T>, bool> = true>
    inline auto zero_throw(T value) -> T{

        if (value == 0u){
            throw std::range_error("unsigned non-zero value cannot be zero");
        }

        return value;
    } 

    template <class T, std::enable_if_t<std::is_unsigned_v<T>, bool> = true>
    inline auto safe_unsigned_lshift(T value, size_t lshift_size) -> T{

        return {};
    } 

    template <class T>
    struct safe_integer_cast_wrapper{

        static_assert(std::numeric_limits<T>::is_integer);
        T value;

        template <class U>
        constexpr operator U() const noexcept{

            return stdxx::safe_integer_cast<U>(this->value);
        }
    };

    template <class T>
    constexpr auto wrap_safe_integer_cast(T value) noexcept{

        return stdxx::safe_integer_cast_wrapper<T>{value};
    }

    template <class Iterator>
    constexpr auto advance(Iterator it, intmax_t diff) noexcept(noexcept(std::advance(it, diff))) -> Iterator{

        static_assert(std::is_nothrow_move_constructible_v<Iterator>);
        std::advance(it, diff); //I never knew what drug std was on
        return it;
    }

    template <class T>
    struct is_chrono_dur: std::false_type{};

    template <class ...Args>
    struct is_chrono_dur<std::chrono::duration<Args...>>: std::true_type{};

    template <class T>
    static inline constexpr bool is_chrono_dur_v = is_chrono_dur<T>::value;

    struct safe_timestamp_cast_wrapper{

        std::chrono::nanoseconds caster;

        constexpr safe_timestamp_cast_wrapper(std::chrono::nanoseconds caster) noexcept: caster(std::move(caster)){}

        template <class U, std::enable_if_t<stdxx::is_chrono_dur_v<U>, bool> = true>
        constexpr operator U() const noexcept{

            return std::chrono::duration_cast<U>(this->caster);
        }

        template <class U, std::enable_if_t<std::numeric_limits<U>::is_integer, bool> = true>
        constexpr operator U() const noexcept{
            auto counter = caster.count();
            static_assert(std::numeric_limits<decltype(counter)>::is_integer);
            return safe_integer_cast<U>(counter);
        }
    };

    auto utc_timestamp() noexcept -> std::chrono::nanoseconds{

        return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::utc_clock::now().time_since_epoch());
    }

    auto unix_timestamp() noexcept -> std::chrono::nanoseconds{

        return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch());
    }
    
    auto unix_low_resolution_timestamp() noexcept -> std::chrono::nanoseconds{

        return {};
    }
    
    auto timestamp_conversion_wrap(std::chrono::nanoseconds dur) noexcept -> safe_timestamp_cast_wrapper{

        return safe_timestamp_cast_wrapper(dur);
    } 

    template <class ...Args>
    struct vector_convertible{

        private:

            std::tuple<Args...> tup;
        
        public:

            vector_convertible(std::tuple<Args...> tup) noexcept: tup(std::move(tup)){}

            template <class ...AArgs>
            operator std::vector<AArgs...>(){

                auto rs = std::vector<AArgs...>();
                rs.reserve(sizeof...(Args));

                [&]<size_t ...IDX>(const std::index_sequence<IDX...>){
                    (
                        [&]{
                            rs.emplace_back(std::move(std::get<IDX>(this->tup)));
                        }(), ...
                    );
                }(std::make_index_sequence<sizeof...(Args)>{});

                return rs;
            }
    };

    template <class ...Args>
    inline auto make_vector_convertible(Args ...args) noexcept -> vector_convertible<Args...>{
        
        static_assert(std::conjunction_v<std::is_nothrow_move_constructible<Args>...>);

        auto tup = std::make_tuple(std::move(args)...);
        vector_convertible rs(std::move(tup));

        return rs;
    }

    class basicstr_converitble{

        private:

            std::string_view view;

        public:

            constexpr basicstr_converitble() = default;

            constexpr basicstr_converitble(std::string_view view) noexcept: view(view){}

            template <class ...Args>
            operator std::basic_string<Args...>() const{
                
                std::basic_string<Args...> rs(view.begin(), view.end());
                return rs;
            }
    };

    inline auto to_basicstr_convertible(std::string_view view) noexcept -> basicstr_converitble{

        return basicstr_converitble(view);
    }

    template <class ...Args>
    auto backsplit_str(std::basic_string<Args...> s, size_t sz) -> std::pair<std::basic_string<Args...>, std::basic_string<Args...>>{

        size_t rhs_sz = std::min(s.size(), sz); 
        std::basic_string<Args...> rhs(rhs_sz, ' ');

        for (size_t i = rhs_sz; i != 0u; --i){
            rhs[i - 1] = s.back();
            s.pop_back();
        }

        return std::make_pair(std::move(s), std::move(rhs));
    }

    template <class ID, class T>
    class singleton{

        private:

            using self = singleton;
            static inline T * volatile obj = new T(); //this is the most important global access operation

        public:

            static inline auto get() noexcept -> T&{

                std::atomic_signal_fence(std::memory_order_seq_cst);
                return *self::obj;
            }
    };

    class VirtualResourceGuard{

        public:

            virtual ~VirtualResourceGuard() noexcept = default;
            virtual void release() noexcept = 0;
    };

    template <class ...Args>
    class UniquePtrVirtualGuard: public virtual VirtualResourceGuard{

        private:

            dg_sock::unique_resource<Args...> resource;
        
        public:

            UniquePtrVirtualGuard(dg_sock::unique_resource<Args...> resource) noexcept: resource(std::move(resource)){}

            void release() noexcept{

                static_assert(noexcept(this->resource.release()));
                this->resource.release();
            }
    };

    template <class Destructor>
    auto vresource_guard(Destructor destructor) -> std::unique_ptr<VirtualResourceGuard>{ //mem-exhaustion is not an error here - it's bad to have it as an error

        auto resource_grd = resource_guard(std::move(destructor));
        UniquePtrVirtualGuard virt_guard(std::move(resource_grd));

        return std::make_unique<decltype(virt_guard)>(std::move(virt_guard));
    }

    static consteval auto hdi_size() noexcept -> size_t{

        return std::max(std::hardware_destructive_interference_size, alignof(std::max_align_t));
    }

    template <size_t SZ>
    static consteval auto round_hdi_size(std::integral_constant<size_t, SZ>) noexcept -> size_t{

        size_t multiplier = SZ / hdi_size() + static_cast<size_t>(SZ % hdi_size() != 0u);
        return hdi_size() * multiplier;
    } 

    template <class T>
    struct hdi_container{
        T value;
    };

    template <class T>
    union inplace_hdi_container{
        alignas(stdxx::hdi_size()) T value;
        alignas(stdxx::hdi_size()) char shape[round_hdi_size(std::integral_constant<size_t, sizeof(T)>{})];

        template <class ...Args>
        inplace_hdi_container(const std::in_place_t, Args&& ...args) noexcept(std::is_nothrow_constructible_v<T, Args&&...>): value(std::forward<Args>(args)...){}
    };

    void high_resolution_sleep(std::chrono::nanoseconds dur) noexcept
    {
        std::this_thread::sleep_for(dur);
    }

    template <class ...Args>
    __attribute__((noipa)) void empty_noipa(Args&& ...args) noexcept{

        (((void) args), ...);
    }

    template <class Task, class ...Args>
    __attribute__((noipa)) auto noipa_do_task(Task&& task, Args&& ...args) noexcept(std::is_nothrow_invocable_v<Task&&, Args&&...>) -> decltype(auto){

        if constexpr(std::is_same_v<decltype(task(std::forward<Args>(args)...)), void>){
            task(std::forward<Args>(args)...);
        } else{
            return task(std::forward<Args>(args)...);
        }    
    }

    template <class T, class ...ConsumingArgs>
    __attribute__((noipa)) auto volatile_access(T * volatile arg, ConsumingArgs& ...consuming_args) noexcept -> T *
    {
        (((void) consuming_args), ...);
        return arg;
    }
}

#endif
