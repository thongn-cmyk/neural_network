#ifndef __NETWORK_PRODUCER_CONSUMER_H__
#define __NETWORK_PRODUCER_CONSUMER_H__

//define HEADER_CONTROL 6

#include <stdint.h>
#include <stddef.h>
#include <array>
#include <memory>
#include <atomic> 
#include "network_concurrency_x.h"
#include "stdx.h"
#include <utility>
#include <iterator>
// #include "network_std_container.h"
// #include "network_log.h"
#include "network_exception.h"
#include "assert.h"
#include "network_datastructure.h"
#include "network_exception_handler.h"
#include "network_type_traits_x.h"
#include "network_hash_factory.h"

namespace dg_sock::network_producer_consumer{

    template <class EventType>
    static inline constexpr bool meets_event_precond_v  = std::is_same_v<EventType, std::decay_t<EventType>>;

    template <class EventType>
    static inline constexpr bool meets_key_precond_v    = std::is_same_v<EventType, std::decay_t<EventType>>;

    template <class EventType>
    struct ProducerInterface{
        using event_t = EventType;
        
        static_assert(meets_event_precond_v<event_t>);

        virtual ~ProducerInterface() noexcept = default;
        virtual void get(event_t * event_arr, size_t& event_arr_sz, size_t event_arr_cap) noexcept = 0;
    };

    template <class EventType>
    struct ConsumerInterface{
        using event_t = EventType;

        static_assert(meets_event_precond_v<event_t>);        

        virtual ~ConsumerInterface() noexcept = default;
        virtual void push(std::move_iterator<event_t *> event_arr, size_t event_arr_sz) noexcept = 0;
    };

    template <class KeyType, class EventType>
    struct KVConsumerInterface{

        using key_t     = KeyType;
        using event_t   = EventType;

        static_assert(meets_key_precond_v<key_t>);
        static_assert(meets_event_precond_v<event_t>);

        virtual ~KVConsumerInterface() noexcept = default;
        virtual void push(const KeyType& key, std::move_iterator<EventType *> event_arr, size_t event_arr_sz) noexcept = 0;
    };

    template <class EventType, class Lambda>
    class LambdaWrappedConsumer: public virtual ConsumerInterface<EventType>{

        private:

            Lambda lambda;

        public:

            static_assert(std::is_nothrow_destructible_v<Lambda>);

            LambdaWrappedConsumer(Lambda lambda) noexcept(std::is_nothrow_move_constructible_v<Lambda>): lambda(std::move(lambda)){}

            void push(std::move_iterator<EventType *> event_arr, size_t event_arr_sz) noexcept(std::is_nothrow_invocable_v<Lambda, std::move_iterator<EventType *>, size_t>){

                this->lambda(event_arr, event_arr_sz);
            }
    };

    constexpr auto is_pow2(size_t val) noexcept -> bool{

        return val != 0u && (val & (val - 1u)) == 0u; 
    }

    template <size_t ALIGNMENT_SZ>
    inline auto dg_memalign(char * buf, const std::integral_constant<size_t, ALIGNMENT_SZ>) noexcept -> char *{

        assert(is_pow2(ALIGNMENT_SZ));

        constexpr uintptr_t fwd     = ALIGNMENT_SZ - 1u;
        constexpr uintptr_t bitmask = ~fwd;
        uintptr_t arithmetic_buf    = reinterpret_cast<uintptr_t>(buf); 
        uintptr_t aligned_buf       = (arithmetic_buf + fwd) & bitmask;

        return reinterpret_cast<char *>(aligned_buf);
    }

    template <class T, class ...Args>
    inline auto inplace_construct_object(char * buf, Args&& ...args) -> T *{

        char * aligned_buf = dg_memalign(buf, std::integral_constant<size_t, alignof(T)>{});
        return new (aligned_buf) T(std::forward<Args>(args)...);
    }

    template <class T, class ...Args>
    inline auto inplace_construct_object_noalign(char * buf, Args&& ...args) -> T *{

        return new (buf) T(std::forward<Args>(args)...);
    }

    template <class T>
    inline auto inplace_construct_array(char * buf, size_t sz) -> T *{

        char * aligned_buf = dg_memalign(buf, std::integral_constant<size_t, alignof(T)>{});
        return new (aligned_buf) T[sz];
    }

    template <class T>
    inline auto inplace_construct_array_noalign(char * buf, size_t sz) -> T *{

        return new (buf) T[sz];
    }

    template <class T, class ...Args>
    inline auto inplace_construct(char * buf, Args&& ...args) -> std::remove_extent_t<T> *{

        if constexpr(std::is_array_v<T>){
            return inplace_construct_array<std::remove_extent_t<T>>(buf, std::forward<Args>(args)...);
        } else{
            return inplace_construct_object<T>(buf, std::forward<Args>(args)...);
        }
    }

    template <class T, class ...Args>
    inline auto inplace_construct_noalign(char * buf, Args&& ...args) -> std::remove_extent_t<T> *{

        if constexpr(std::is_array_v<T>){
            return inplace_construct_array_noalign<std::remove_extent_t<T>>(buf, std::forward<Args>(args)...);
        } else{
            return inplace_construct_object_noalign<T>(buf, std::forward<Args>(args)...);
        }
    }

    template <class T, class ...Args>
    constexpr auto inplace_construct_object_size(Args&& ...) noexcept -> size_t{

        return sizeof(T) + alignof(T);
    }

    template <class T, class ...Args>
    constexpr auto inplace_construct_object_noalign_size(Args&& ...) noexcept -> size_t{

        return sizeof(T);
    }

    template <class T>
    constexpr auto inplace_construct_array_size(size_t sz) noexcept -> size_t{

        return sz * sizeof(T) + alignof(T);
    }

    template <class T>
    constexpr auto inplace_construct_array_noalign_size(size_t sz) noexcept -> size_t{

        return sz * sizeof(T);
    }

    template <class T, class ...Args>
    constexpr auto inplace_construct_size(Args&& ...args) noexcept -> size_t{

        if constexpr(std::is_array_v<T>){
            return inplace_construct_array_size<std::remove_extent_t<T>>(std::forward<Args>(args)...);
        } else{
            return inplace_construct_object_size<T>(std::forward<Args>(args)...);
        }
    }

    template <class T, class ...Args>
    constexpr auto inplace_construct_noalign_size(Args&& ...args) noexcept -> size_t{

        if constexpr(std::is_array_v<T>){
            return inplace_construct_array_noalign_size<std::remove_extent_t<T>>(std::forward<Args>(args)...);
        } else{
            return inplace_construct_object_noalign_size<T>(std::forward<Args>(args)...);
        }
    }

    template <class T>
    inline auto inplace_destruct_object(T * obj) noexcept(std::is_nothrow_destructible_v<T>){

        std::destroy_at(obj);
    }

    template <class T>
    inline auto inplace_destruct_array(T * arr, size_t arr_sz) noexcept(std::is_nothrow_destructible_v<T>){

        std::destroy(arr, std::next(arr, arr_sz));
    }

    template <class T, class ...Args>
    inline auto raii_inplace_construct_object(char * buf, Args&& ...args){

        auto destructor = [](T * obj) noexcept(noexcept(inplace_destruct_object(std::declval<T *>()))){
            inplace_destruct_object(obj);
        };

        T * obj = inplace_construct_object<T>(buf, std::forward<Args>(args)...);
        return std::unique_ptr<T, decltype(destructor)>(obj, destructor);
    }

    template <class T>
    inline auto raii_inplace_construct_array(char * buf, size_t sz){

        auto destructor = [sz](T * arr) noexcept(noexcept(inplace_destruct_array(std::declval<T *>(), std::declval<size_t>()))){
            inplace_destruct_array(arr, sz);
        };

        T * obj = inplace_construct_array<T>(buf, sz);
        return std::unique_ptr<T[], decltype(destructor)>(obj, destructor); //this might be buggy
    }

    template <class T, class ...Args>
    inline auto raii_inplace_construct(char * buf, Args&& ...args){

        if constexpr(std::is_array_v<T>){
            return raii_inplace_construct_array<std::remove_extent_t<T>>(buf, std::forward<Args>(args)...);
        } else{
            return raii_inplace_construct_object<T>(buf, std::forward<Args>(args)...);
        }
    } 

    //^^^
    //fine, pass code review 

    template <class EventType>
    struct DeliveryHandle{
        EventType * deliverable_arr;
        size_t deliverable_sz;
        size_t deliverable_cap;
        ConsumerInterface<EventType> * consumer;
    };

    template <class event_t>
    auto delvrsrv_open_handle(ConsumerInterface<event_t> * consumer, size_t deliverable_cap) noexcept -> std::expected<DeliveryHandle<event_t> *, exception_t>{

        constexpr size_t MIN_DELIVERABLE_CAP = 0u;
        constexpr size_t MAX_DELIVERABLE_CAP = size_t{1} << 30;

        if (consumer == nullptr){
            return std::unexpected(dg_sock::network_exception::INVALID_ARGUMENT);
        }

        if (std::clamp(deliverable_cap, MIN_DELIVERABLE_CAP, MAX_DELIVERABLE_CAP) != deliverable_cap){
            return std::unexpected(dg_sock::network_exception::INVALID_ARGUMENT);
        }

        try{
            auto container              = std::unique_ptr<event_t[]>(new event_t[deliverable_cap]);
            size_t container_sz         = 0u;
            size_t container_cap        = deliverable_cap;
            auto delivery_handle        = std::unique_ptr<DeliveryHandle<event_t>>(new DeliveryHandle<event_t>{container.get(), container_sz, container_cap, consumer});
            auto rs                     = delivery_handle.get();

            container.release();
            delivery_handle.release();

            return rs;
        } catch (...){
            return std::unexpected(dg_sock::network_exception::wrap_std_exception(std::current_exception()));
        }
    }

    template <class event_t>
    auto delvrsrv_open_preallocated_handle(ConsumerInterface<event_t> * consumer, size_t deliverable_cap, char * preallocated_buf) noexcept -> std::expected<DeliveryHandle<event_t> *, exception_t>{

        constexpr size_t MIN_DELIVERABLE_CAP = 0u;
        constexpr size_t MAX_DELIVERABLE_CAP = size_t{1} << 30;

        if (consumer == nullptr){
            return std::unexpected(dg_sock::network_exception::INVALID_ARGUMENT);
        }

        if (std::clamp(deliverable_cap, MIN_DELIVERABLE_CAP, MAX_DELIVERABLE_CAP) != deliverable_cap){
            return std::unexpected(dg_sock::network_exception::INVALID_ARGUMENT);
        }

        if (preallocated_buf == nullptr){
            return std::unexpected(dg_sock::network_exception::INVALID_ARGUMENT);
        }

        try{
            auto raii_event_ptr     = network_producer_consumer::raii_inplace_construct<event_t[]>(preallocated_buf, deliverable_cap);
            size_t container_sz     = 0u;
            size_t container_cap    = deliverable_cap;
            auto raii_handle_ptr    = network_producer_consumer::raii_inplace_construct<DeliveryHandle<event_t>>(std::next(preallocated_buf, network_producer_consumer::inplace_construct_size<event_t[]>(deliverable_cap)),
                                                                                                                 DeliveryHandle<event_t>{raii_event_ptr.get(), container_sz, container_cap, consumer});
            auto rs                 = raii_handle_ptr.get();

            raii_event_ptr.release();
            raii_handle_ptr.release();

            return rs;
        } catch (...){
            return std::unexpected(dg_sock::network_exception::wrap_std_exception(std::current_exception()));
        }
    }

    template <class event_t>
    auto delvrsrv_allocation_cost(ConsumerInterface<event_t> * consumer, size_t deliverable_cap) noexcept -> size_t{

        return network_producer_consumer::inplace_construct_size<event_t[]>(deliverable_cap) 
               + network_producer_consumer::inplace_construct_size<DeliveryHandle<event_t>>();
    }

    template <class event_t>
    void delvrsrv_deliver(DeliveryHandle<event_t> * handle, event_t event) noexcept{

        static_assert(std::is_nothrow_move_assignable_v<event_t>);
        handle = stdxx::safe_ptr_access(handle); 

        if (handle->deliverable_sz == handle->deliverable_cap){            
            if (handle->deliverable_cap == 0u) [[unlikely]]{
                handle->consumer->push(std::make_move_iterator(&event), 1u);
                return;
            } else [[likely]]{
                handle->consumer->push(std::make_move_iterator(handle->deliverable_arr), handle->deliverable_sz);
                handle->deliverable_sz = 0u;
            }
        }

        handle->deliverable_arr[handle->deliverable_sz++] = std::move(event);
    }

    static inline auto delvrsrv_deliver_lambda = []<class event_t>(DeliveryHandle<event_t> * handle, event_t event) noexcept{
        delvrsrv_deliver(handle, std::move(event));
    };

    template <class event_t>
    void delvrsrv_clear(DeliveryHandle<event_t> * handle) noexcept{

        handle = stdxx::safe_ptr_access(handle);

        if (handle->deliverable_sz != 0u){
            handle->consumer->push(std::make_move_iterator(handle->deliverable_arr), handle->deliverable_sz);
            handle->deliverable_sz = 0u;
        }
    }

    template <class event_t>
    auto delvrsrv_close_handle(DeliveryHandle<event_t> * handle) noexcept{

        delvrsrv_clear(handle);

        delete[] handle->deliverable_arr;
        delete handle; //TODO: internalize delete
    }

    template <class event_t>
    auto delvrsrv_close_preallocated_handle(DeliveryHandle<event_t> * handle) noexcept{

        delvrsrv_clear(handle);

        network_producer_consumer::inplace_destruct_array(handle->deliverable_arr, handle->deliverable_cap);
        network_producer_consumer::inplace_destruct_object(handle);
    }

    static inline auto delvrsrv_close_handle_lambda = []<class event_t>(DeliveryHandle<event_t> * handle) noexcept{
        delvrsrv_close_handle(handle);
    };

    static inline auto delvrsrv_close_preallocated_handle_lambda = []<class event_t>(DeliveryHandle<event_t> * handle) noexcept{
        delvrsrv_close_preallocated_handle(handle);
    };

    template <class event_t>
    auto delvrsrv_open_raiihandle(ConsumerInterface<event_t> * consumer, size_t deliverable_cap) noexcept -> std::expected<std::unique_ptr<DeliveryHandle<event_t>, decltype(delvrsrv_close_handle_lambda)>, exception_t>{

        std::expected<DeliveryHandle<event_t> *, exception_t> handle = delvrsrv_open_handle(consumer, deliverable_cap);

        if (!handle.has_value()){
            return std::unexpected(handle.error());
        }

        return std::unique_ptr<DeliveryHandle<event_t>, decltype(delvrsrv_close_handle_lambda)>(handle.value(), delvrsrv_close_handle_lambda);
    }

    template <class event_t>
    auto delvrsrv_open_preallocated_raiihandle(ConsumerInterface<event_t> * consumer, size_t deliverable_cap, char * preallocated_buf) noexcept -> std::expected<std::unique_ptr<DeliveryHandle<event_t>, decltype(delvrsrv_close_preallocated_handle_lambda)>, exception_t>{

        std::expected<DeliveryHandle<event_t> *, exception_t> handle = delvrsrv_open_preallocated_handle(consumer, deliverable_cap, preallocated_buf);

        if (!handle.has_value()){
            return std::unexpected(handle.error());
        }

        return std::unique_ptr<DeliveryHandle<event_t>, decltype(delvrsrv_close_preallocated_handle_lambda)>(handle.value(), delvrsrv_close_preallocated_handle_lambda);
    }    

    //vvv
    //-----

    //^^^
    //fine, minimum viable product

    struct BumpAllocatorResource{
        char * head;
        size_t sz;
        size_t cap;
    };

    auto bump_allocator_initialize(size_t buf_sz) -> std::expected<BumpAllocatorResource *, exception_t>{

        try{
            size_t sz       = 0u;
            size_t cap      = buf_sz;
            auto safe_head  = std::unique_ptr<char[]>(new char[buf_sz]); //this is harder than expected
            auto resource   = std::unique_ptr<BumpAllocatorResource>(new BumpAllocatorResource{safe_head.get(), sz, cap}); 
            auto rs         = resource.get();
            safe_head.release();
            resource.release();

            return rs;
        } catch (...){
            return std::unexpected(dg_sock::network_exception::wrap_std_exception(std::current_exception()));
        }
    }

    auto bump_allocator_preallocated_initialize(size_t buf_sz, char * buf) -> std::expected<BumpAllocatorResource *, exception_t>{

        size_t sz       = 0u;
        size_t cap      = buf_sz;        
        auto resource   = BumpAllocatorResource{.head   = (buf_sz != 0u) ? buf : std::add_pointer_t<char>(nullptr),
                                                .sz     = sz,
                                                .cap    = cap};

        return network_producer_consumer::inplace_construct<BumpAllocatorResource>(std::next(buf, buf_sz), resource);
    }

    void bump_allocator_deinitialize(BumpAllocatorResource * obj) noexcept{

        delete[] stdxx::safe_ptr_access(obj)->head;
        delete obj;
    }

    void bump_allocator_preallocated_deinitialize(BumpAllocatorResource * obj) noexcept{

        network_producer_consumer::inplace_destruct_object(stdxx::safe_ptr_access(obj));
    }    

    auto bump_allocator_allocation_cost(size_t buf_sz) noexcept -> size_t{

        return buf_sz + inplace_construct_size<BumpAllocatorResource>();
    }

    void bump_allocator_reset(BumpAllocatorResource * bump_allocator) noexcept{

        stdxx::safe_ptr_access(bump_allocator)->sz = 0u;
    }

    auto bump_allocator_allocate(BumpAllocatorResource * bump_allocator, size_t sz) noexcept -> std::expected<char *, exception_t>{

        size_t nxt_sz = stdxx::safe_ptr_access(bump_allocator)->sz + sz;

        if (nxt_sz > bump_allocator->cap){
            return std::unexpected(dg_sock::network_exception::RESOURCE_EXHAUSTION);
        }

        char * current      = std::next(bump_allocator->head, bump_allocator->sz);
        bump_allocator->sz  += sz; 

        return current;
    }

    auto bump_allocator_align(BumpAllocatorResource * bump_allocator, size_t alignment_sz) noexcept -> exception_t{

        assert(stdxx::is_pow2(alignment_sz));

        bump_allocator                  = stdxx::safe_ptr_access(bump_allocator); 

        size_t fwd_sz                   = alignment_sz - 1u;
        size_t arithmetic_buf           = reinterpret_cast<size_t>(bump_allocator->head);
        size_t fwd_arithmetic_buf       = arithmetic_buf + fwd_sz;
        size_t MASK                     = ~fwd_sz;
        size_t aligned_arithmetic_buf   = fwd_arithmetic_buf & MASK; 
        size_t displacement_sz          = aligned_arithmetic_buf - arithmetic_buf;

        if (bump_allocator->sz + displacement_sz > bump_allocator->cap){
            return dg_sock::network_exception::RESOURCE_EXHAUSTION;
        }

        bump_allocator->sz              += displacement_sz;

        return dg_sock::network_exception::SUCCESS; 
    }

    //vvv
    //-----------------------

    //we'll move on for now
    //let's do another code review
    //code is clear
    //alright we are to increase the delivery (feed) -> 1 << 28
    //alright its impossible to do so fellas
    //we have more important tasks to do

    template <class EventType>
    struct KVEventContainer{
        EventType * ptr;
        size_t sz;
        size_t cap;
    };

    //this is wrong? how why ?
    //assume a + b + c + d == sz

    //a <= ceil2(a), ...
    //actual_storage_size(a) <= ceil2(a) * 2 - 1, ...
    //ceil2(a) * 2 - 1 <= 4 * a

    //actual_storage_size(a) + ... <= 4 * (a + b + ...) == 4sz

    //x^0 + x^1 + x^2 + x^n = f(x)
    //f(x)*(x - 1) = x^(n + 1) - 1 / (x - 1)
    //x == 2
    //=> f(x) = x^(n + 1) - 1

    //we are definitely wasting a lot of memory YET we only need the memory during the delvrsrv_kv_deliver phase (the accumulation phase)
    //we'll try to reduce the memory consumption later
    //this is the most optimized version of unordered_map keyvalue feed that we could write
    //we can actually converge the delvrsrv_kv_deliver() -> delvrsrv_deliver() time by increasing the GROWTH_FACTOR -> 4
    //there are probably some optimizables here and there that we could further do, we'll be sitting comfortably within the 2x the delvrsrv_deliver() range 

    static inline constexpr size_t DELVRSRV_KV_EVENT_CONTAINER_INITIAL_CAP      = size_t{1}; 
    static inline constexpr size_t DELVRSRV_KV_EVENT_CONTAINER_GROWTH_FACTOR    = size_t{2};

    template <class = void>
    static constexpr bool FALSE_VAL = false;

    template <class T>
    class KeyValueConventionalAllocator
    {
        private:

            char ** preallocated_buf;

            template <class U>
            friend class KeyValueConventionalAllocator;

        public:

            using value_type = T;

            constexpr KeyValueConventionalAllocator() noexcept: preallocated_buf(nullptr){}

            constexpr KeyValueConventionalAllocator(char ** buf) noexcept: preallocated_buf(buf){}

            template <class ...Args>
            constexpr KeyValueConventionalAllocator(const KeyValueConventionalAllocator<Args...>& other): preallocated_buf(other.preallocated_buf){}

            auto allocate(std::size_t n) -> T *
            {
                if (n == 0u || sizeof(T) == 0u)
                {
                    return nullptr;
                }

                char * raw_buf; 

                if (this->preallocated_buf == nullptr)
                {
                    raw_buf = static_cast<char *>(std::aligned_alloc(alignof(T), n * sizeof(T)));

                    if (raw_buf == nullptr)
                    {
                        throw std::bad_alloc();
                    }
                }
                else
                {
                    size_t allocation_sz    = n * sizeof(T) + alignof(T); 
                    char *& char_reference  = *this->preallocated_buf;
                    raw_buf                 = stdxx::align_ptr(std::add_pointer_t<char>(char_reference), std::integral_constant<size_t, alignof(T)>{});

                    std::advance(char_reference, allocation_sz);
                }

                return static_cast<T *>(static_cast<void *>(raw_buf));
            }

            void deallocate(T * p, std::size_t n)
            {
                if (p == nullptr)
                {
                    return;
                }

                if (this->preallocated_buf == nullptr)
                {
                    std::free(static_cast<void *>(p));
                }
            }

            template <class U, class ...Args>
            void construct(U * p, Args&& ...args)
            {
                new (p) U(std::forward<Args>(args)...);
            }

            template <class U>
            void destroy(U * p)
            {
                std::destroy_at(p);
            }
    };


    template <typename ...Args, typename ...Args1>
    bool operator==(const KeyValueConventionalAllocator<Args...>&, const KeyValueConventionalAllocator<Args1...>&)
    {
        return true;
    }

    template <typename ...Args, typename ...Args1>
    bool operator!=(const KeyValueConventionalAllocator<Args...>&, const KeyValueConventionalAllocator<Args1...>&)
    {
        return false;
    }

    template <class KeyType, class MappedType>
    using kv_feed_unordered_map_t = dg_sock::network_datastructure::unordered_map_variants::unordered_node_map<KeyType,
                                                                                                          MappedType,
                                                                                                          std::size_t,
                                                                                                          std::integral_constant<bool, true>,
                                                                                                          dg_sock::network_hash_factory::default_hasher<KeyType>,
                                                                                                          dg_sock::network_hash_factory::default_equal_to<KeyType>,
                                                                                                          KeyValueConventionalAllocator<std::pair<const KeyType, MappedType>>>;

    template<class KeyType, class EventType>
    struct KVDeliveryHandle{
        BumpAllocatorResource * bump_allocator;
        size_t deliverable_sz;
        size_t deliverable_cap;
        KVConsumerInterface<KeyType, EventType> * consumer;
        kv_feed_unordered_map_t<KeyType, KVEventContainer<EventType>> key_event_map; //we dont want to reinvent the wheel to confuse our peers, we'll leverage advanced inplace allocator + throw, the complexity + debuggability + maintainability of the keyvalue feed are already reaching the unacceptable threshold  
    };

    //this is precisely why I said this is very super complicated to solve the alignment issues 

    //clear
    template <class EventType>
    auto delvrsrv_kv_get_event_container_memory_total_bcap(size_t deliverable_cap) noexcept -> size_t{

        if (deliverable_cap == 0u){
            return 0u;
        }

        static_assert(DELVRSRV_KV_EVENT_CONTAINER_GROWTH_FACTOR == 2u);

        size_t worst_case_cap   = deliverable_cap * (DELVRSRV_KV_EVENT_CONTAINER_GROWTH_FACTOR * DELVRSRV_KV_EVENT_CONTAINER_GROWTH_FACTOR);
        size_t minimum_cap      = DELVRSRV_KV_EVENT_CONTAINER_INITIAL_CAP;
        size_t cap              = std::max(worst_case_cap, minimum_cap);  
        size_t bsz              = alignof(EventType) + inplace_construct_noalign_size<EventType[]>(cap);

        return bsz;
    }

    //clear
    template <class EventType>
    auto delvrsrv_kv_get_event_container_bsize(size_t capacity) noexcept -> size_t{

        return inplace_construct_noalign_size<EventType[]>(capacity);
    }

    //clear
    template <class EventType>
    auto delvrsrv_kv_make_preallocated_event_container(size_t capacity, char * buf) noexcept -> KVEventContainer<EventType>{

        // static_assert(std::is_nothrow_default_constructible_v<EventType>);

        EventType * event_arr   = inplace_construct_noalign<EventType[]>(buf, capacity);
        auto container          = KVEventContainer<EventType>{event_arr, 0u, capacity};

        return container;        
    }

    //clear
    template <class EventType>
    void delvrsrv_kv_destroy_event_container(KVEventContainer<EventType> event_container) noexcept{

        static_assert(std::is_nothrow_destructible_v<EventType>);
        dg_sock::network_producer_consumer::inplace_destruct_array(event_container.ptr, event_container.cap);
    }

    //clear
    //compiler is to see this to apply correct branching skews, nothrow log with set branching of container.sz == container_cap -> 10%
    template <class EventType, class EventLike>
    inline auto delvrsrv_kv_push_event_container(KVEventContainer<EventType>& container, EventLike&& event) noexcept -> exception_t{

        if constexpr(std::is_nothrow_assignable_v<EventType&, EventLike&&>){
            if (container.sz == container.cap){
                return dg_sock::network_exception::RESOURCE_EXHAUSTION;
            }

            container.ptr[container.sz++] = std::forward<EventLike>(event);
            return dg_sock::network_exception::SUCCESS;
        } else{
            static_assert(FALSE_VAL<>);
        }
    }

    //clear
    template <class key_t, class event_t>
    auto delvrsrv_kv_open_handle(KVConsumerInterface<key_t, event_t> * consumer, size_t deliverable_cap) noexcept -> std::expected<KVDeliveryHandle<key_t, event_t> *, exception_t>{

        const size_t MIN_DELIVERABLE_CAP    = 0u;
        const size_t MAX_DELIVERABLE_CAP    = size_t{1} << 30; 

        if (consumer == nullptr){
            return std::unexpected(dg_sock::network_exception::INVALID_ARGUMENT);
        }

        if (std::clamp(deliverable_cap, MIN_DELIVERABLE_CAP, MAX_DELIVERABLE_CAP) != deliverable_cap){
            return std::unexpected(dg_sock::network_exception::INVALID_ARGUMENT);
        }

        try{
            auto bump_allocator     = bump_allocator_initialize(delvrsrv_kv_get_event_container_memory_total_bcap<event_t>(deliverable_cap));

            if (!bump_allocator.has_value()){
                return std::unexpected(bump_allocator.error());
            }

            dg_sock::network_exception_handler::nothrow_log(bump_allocator_align(bump_allocator.value(), alignof(event_t)));

            auto resource_guard     = stdxx::resource_guard([&]() noexcept{bump_allocator_deinitialize(bump_allocator.value());});

            size_t bucket_count     = kv_feed_unordered_map_t<key_t, KVEventContainer<event_t>>::bucket_count_for_size_of(deliverable_cap + 1u);
            auto key_event_map      = kv_feed_unordered_map_t<key_t, KVEventContainer<event_t>>(bucket_count);
            size_t deliverable_sz   = 0u;
            auto delivery_handle    = std::unique_ptr<KVDeliveryHandle<key_t, event_t>>(new KVDeliveryHandle<key_t, event_t>{bump_allocator.value(), deliverable_sz, deliverable_cap, consumer, std::move(key_event_map)});
            auto rs                 = delivery_handle.get();

            delivery_handle.release();
            resource_guard.release();

            return rs;
        } catch (...){
            return std::unexpected(dg_sock::network_exception::wrap_std_exception(std::current_exception()));
        }
    }

    //clear
    template <class key_t, class event_t>
    auto delvrsrv_kv_open_preallocated_handle(KVConsumerInterface<key_t, event_t> * consumer,
                                              size_t deliverable_cap,
                                              char * preallocated_buf) noexcept -> std::expected<KVDeliveryHandle<key_t, event_t> *, exception_t>{

        const size_t MIN_DELIVERABLE_CAP    = 0u;
        const size_t MAX_DELIVERABLE_CAP    = size_t{1} << 30;

        if (consumer == nullptr){
            return std::unexpected(dg_sock::network_exception::INVALID_ARGUMENT);
        }

        if (std::clamp(deliverable_cap, MIN_DELIVERABLE_CAP, MAX_DELIVERABLE_CAP) != deliverable_cap){
            return std::unexpected(dg_sock::network_exception::INVALID_ARGUMENT);
        }

        size_t bucket_count = kv_feed_unordered_map_t<key_t, KVEventContainer<event_t>>::bucket_count_for_size_of(deliverable_cap + 1u);
        char * map_buf      = preallocated_buf;
        char * ptr_buf      = std::next(map_buf, kv_feed_unordered_map_t<key_t, KVEventContainer<event_t>>::allocation_size(bucket_count));

        try
        {
            BumpAllocatorResource * bump_allocator;

            {
                size_t bump_allocator_mem_sz = delvrsrv_kv_get_event_container_memory_total_bcap<event_t>(deliverable_cap);
                std::expected<BumpAllocatorResource *, exception_t> decoy_bump_allocator = bump_allocator_preallocated_initialize(bump_allocator_mem_sz, ptr_buf);

                if (!decoy_bump_allocator.has_value())
                {
                    return std::unexpected(decoy_bump_allocator.error());
                }

                bump_allocator = decoy_bump_allocator.value();
                dg_sock::network_exception_handler::nothrow_log(bump_allocator_align(bump_allocator, alignof(event_t)));

                std::advance(ptr_buf, bump_allocator_allocation_cost(bump_allocator_mem_sz));
            }

            auto resource_guard = stdxx::resource_guard([&]() noexcept{bump_allocator_preallocated_deinitialize(bump_allocator);});

            std::optional<kv_feed_unordered_map_t<key_t, KVEventContainer<event_t>>> key_event_map;

            {
                char ** preallocated_buffer = inplace_construct<std::add_pointer_t<char>>(ptr_buf, std::add_pointer_t<char>(map_buf));
                key_event_map               = kv_feed_unordered_map_t<key_t, KVEventContainer<event_t>>(bucket_count,
                                                                                                        KeyValueConventionalAllocator<void *>(preallocated_buffer));

                std::advance(ptr_buf, inplace_construct_size<std::add_pointer_t<char>>());
            }

            KVDeliveryHandle<key_t, event_t> * delivery_handle;

            {
                delivery_handle =  inplace_construct<KVDeliveryHandle<key_t, event_t>>(ptr_buf,
                                                                                       KVDeliveryHandle<key_t, event_t>{.bump_allocator     = bump_allocator,
                                                                                                                        .deliverable_sz     = 0u,
                                                                                                                        .deliverable_cap    = deliverable_cap,
                                                                                                                        .consumer           = consumer,
                                                                                                                        .key_event_map      = std::move(key_event_map.value())});

                std::advance(ptr_buf, inplace_construct_size<KVDeliveryHandle<key_t, event_t>>());
            }

            resource_guard.release();

            return delivery_handle;
        } catch (...){
            return std::unexpected(dg_sock::network_exception::wrap_std_exception(std::current_exception()));
        }
    }

    //clear
    template <class key_t, class event_t>
    auto delvrsrv_kv_allocation_cost(KVConsumerInterface<key_t, event_t> * consumer, size_t deliverable_cap) noexcept -> size_t{
 
        return bump_allocator_allocation_cost(delvrsrv_kv_get_event_container_memory_total_bcap<event_t>(deliverable_cap))
                + kv_feed_unordered_map_t<key_t, KVEventContainer<event_t>>::allocation_size(kv_feed_unordered_map_t<key_t, KVEventContainer<event_t>>::bucket_count_for_size_of(deliverable_cap + 1u))
                + inplace_construct_size<std::add_pointer_t<char>>()
                + inplace_construct_size<KVDeliveryHandle<key_t, event_t>>();
    }

    //clear
    template <class key_t, class event_t>
    void delvrsrv_kv_clear(KVDeliveryHandle<key_t, event_t> * handle) noexcept{

        for (auto it = handle->key_event_map.begin(); it != handle->key_event_map.end(); ++it){
            const auto& key     = it->first;
            auto& value         = it->second;
            handle->consumer->push(key, std::make_move_iterator(value.ptr), value.sz);
            delvrsrv_kv_destroy_event_container(value);
        }

        handle->key_event_map.clear();
        bump_allocator_reset(handle->bump_allocator);
        dg_sock::network_exception_handler::nothrow_log(bump_allocator_align(handle->bump_allocator, alignof(event_t)));

        handle->deliverable_sz = 0u;
    }

    //clear
    
    //

    template <class key_t, class event_t>
    __attribute__((noinline)) void delvrsrv_kv_deliver_slow_path(KVDeliveryHandle<key_t, event_t> * handle, const key_t& key, event_t event) noexcept{

        handle = stdxx::safe_ptr_access(handle);

        if (handle->deliverable_sz == handle->deliverable_cap){
            if (handle->deliverable_cap == 0u) [[unlikely]]{
                handle->consumer->push(key, std::make_move_iterator(&event), 1u);
                return;
            } else [[likely]]{
                delvrsrv_kv_clear(handle);
            }
        }

        auto map_ptr = handle->key_event_map.find(key);  

        if (map_ptr == handle->key_event_map.end()){
            std::expected<char *, exception_t> buf = bump_allocator_allocate(handle->bump_allocator, delvrsrv_kv_get_event_container_bsize<event_t>(DELVRSRV_KV_EVENT_CONTAINER_INITIAL_CAP));

            if (!buf.has_value()) [[unlikely]]{
                if (buf.error() == dg_sock::network_exception::RESOURCE_EXHAUSTION){
                    delvrsrv_kv_clear(handle);
                    buf = dg_sock::network_exception_handler::nothrow_log(bump_allocator_allocate(handle->bump_allocator, delvrsrv_kv_get_event_container_bsize<event_t>(DELVRSRV_KV_EVENT_CONTAINER_INITIAL_CAP)));
                } else{
                    if constexpr(DEBUG_MODE_FLAG){
                        // dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    } else{
                        std::unreachable();
                    }
                }
            }

            auto req_container = delvrsrv_kv_make_preallocated_event_container<event_t>(DELVRSRV_KV_EVENT_CONTAINER_INITIAL_CAP, buf.value());

            try{
                auto [emplace_ptr, emplace_status]  = handle->key_event_map.try_emplace(key, req_container);
                dg_sock::network_exception_handler::dg_assert(emplace_status);
                map_ptr                             = emplace_ptr;
            } catch (std::bad_alloc& e){ //bad alloc might or might not use dynamic allocations, we are to trust std to do their affinity job correctly
                delvrsrv_kv_destroy_event_container(req_container);
                delvrsrv_kv_clear(handle);
                char * nofail_buf                   = dg_sock::network_exception_handler::nothrow_log(bump_allocator_allocate(handle->bump_allocator, delvrsrv_kv_get_event_container_bsize<event_t>(DELVRSRV_KV_EVENT_CONTAINER_INITIAL_CAP)));
                req_container                       = delvrsrv_kv_make_preallocated_event_container<event_t>(DELVRSRV_KV_EVENT_CONTAINER_INITIAL_CAP, nofail_buf);
                auto [emplace_ptr, emplace_status]  = handle->key_event_map.try_emplace(key, req_container);
                dg_sock::network_exception_handler::dg_assert(emplace_status);
                map_ptr                             = emplace_ptr;
            }
        }

        if (map_ptr->second.sz == map_ptr->second.cap){
            size_t new_sz                           = map_ptr->second.cap * DELVRSRV_KV_EVENT_CONTAINER_GROWTH_FACTOR;
            std::expected<char *, exception_t> buf  = bump_allocator_allocate(handle->bump_allocator, delvrsrv_kv_get_event_container_bsize<event_t>(new_sz));

            if (!buf.has_value()) [[unlikely]]{
                if (buf.error() == dg_sock::network_exception::RESOURCE_EXHAUSTION){
                    delvrsrv_kv_clear(handle);
                    char * nofail_buf                   = dg_sock::network_exception_handler::nothrow_log(bump_allocator_allocate(handle->bump_allocator, delvrsrv_kv_get_event_container_bsize<event_t>(DELVRSRV_KV_EVENT_CONTAINER_INITIAL_CAP)));
                    auto req_container                  = delvrsrv_kv_make_preallocated_event_container<event_t>(DELVRSRV_KV_EVENT_CONTAINER_INITIAL_CAP, nofail_buf);
                    auto [emplace_ptr, emplace_status]  = handle->key_event_map.try_emplace(key, req_container);

                    dg_sock::network_exception_handler::dg_assert(emplace_status);
                    map_ptr                             = emplace_ptr;
                } else{                    
                    if constexpr(DEBUG_MODE_FLAG){
                        // dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    } else{
                        std::unreachable();
                    }
                }
            } else [[likely]]{
                auto req_container          = delvrsrv_kv_make_preallocated_event_container<event_t>(new_sz, buf.value());

                static_assert(std::is_nothrow_move_assignable_v<event_t>);
                std::copy(std::make_move_iterator(map_ptr->second.ptr), std::make_move_iterator(std::next(map_ptr->second.ptr, map_ptr->second.sz)), req_container.ptr);

                req_container.sz            = map_ptr->second.sz;
                delvrsrv_kv_destroy_event_container(map_ptr->second);
                map_ptr->second             = req_container;
            }
        }

        dg_sock::network_exception_handler::nothrow_log(delvrsrv_kv_push_event_container(map_ptr->second, std::move(event)));
        handle->deliverable_sz += 1;
    }

    template <class key_t, class event_t>
    inline void delvrsrv_kv_deliver(KVDeliveryHandle<key_t, event_t> * handle, const key_t& key, event_t event) noexcept{

        static_assert(std::is_nothrow_move_constructible_v<event_t>);

        handle          = stdxx::safe_ptr_access(handle);
        auto map_ptr    = handle->key_event_map.find(key);

        if (handle->deliverable_sz == handle->deliverable_cap || map_ptr == handle->key_event_map.end() || map_ptr->second.sz == map_ptr->second.cap) [[unlikely]]{
            delvrsrv_kv_deliver_slow_path(handle, key, std::move(event));
        } else [[likely]]{
            dg_sock::network_exception_handler::nothrow_log(delvrsrv_kv_push_event_container(map_ptr->second, std::move(event)));
            handle->deliverable_sz += 1;
        }
    }

    //clear
    template <class key_t, class event_t>
    void delvrsrv_kv_close_handle(KVDeliveryHandle<key_t, event_t> * handle) noexcept{

        static_assert(std::is_nothrow_destructible_v<KVDeliveryHandle<key_t, event_t>>);

        handle = stdxx::safe_ptr_access(handle);
        delvrsrv_kv_clear(handle);
        bump_allocator_deinitialize(handle->bump_allocator);

        delete handle;
    }

    //clear
    template <class key_t, class event_t>
    void delvrsrv_kv_close_preallocated_handle(KVDeliveryHandle<key_t, event_t> * handle) noexcept{

        static_assert(noexcept(inplace_destruct_object(handle)));

        handle = stdxx::safe_ptr_access(handle);
        delvrsrv_kv_clear(handle);
        bump_allocator_preallocated_deinitialize(handle->bump_allocator);
        inplace_destruct_object(handle);
    }

    //clear
    static inline auto delvrsrv_kv_close_handle_lambda = []<class key_t, class event_t>(KVDeliveryHandle<key_t, event_t> * handle) noexcept{
        delvrsrv_kv_close_handle(handle);
    };

    //clear
    static inline auto delvrsrv_kv_close_preallocated_handle_lambda = []<class key_t, class event_t>(KVDeliveryHandle<key_t, event_t> * handle) noexcept{
        delvrsrv_kv_close_preallocated_handle(handle);
    };

    //clear
    template <class key_t, class event_t>
    auto delvrsrv_kv_open_raiihandle(KVConsumerInterface<key_t, event_t> * consumer, size_t deliverable_cap) noexcept -> std::expected<std::unique_ptr<KVDeliveryHandle<key_t, event_t>, decltype(delvrsrv_kv_close_handle_lambda)>, exception_t>{

        std::expected<KVDeliveryHandle<key_t, event_t> *, exception_t> handle = delvrsrv_kv_open_handle(consumer, deliverable_cap);

        if (!handle.has_value()){
            return std::unexpected(handle.error());
        }

        return std::unique_ptr<KVDeliveryHandle<key_t, event_t>, decltype(delvrsrv_kv_close_handle_lambda)>(handle.value(), delvrsrv_kv_close_handle_lambda);
    }

    //clear
    template <class key_t, class event_t>
    auto delvrsrv_kv_open_preallocated_raiihandle(KVConsumerInterface<key_t, event_t> * consumer, size_t deliverable_cap, char * preallocated_buf) noexcept -> std::expected<std::unique_ptr<KVDeliveryHandle<key_t, event_t>, decltype(delvrsrv_kv_close_preallocated_handle_lambda)>, exception_t>{

        std::expected<KVDeliveryHandle<key_t, event_t> *, exception_t> handle = delvrsrv_kv_open_preallocated_handle(consumer, deliverable_cap, preallocated_buf);

        if (!handle.has_value()){
            return std::unexpected(handle.error());
        }

        return std::unique_ptr<KVDeliveryHandle<key_t, event_t>, decltype(delvrsrv_kv_close_preallocated_handle_lambda)>(handle.value(), delvrsrv_kv_close_preallocated_handle_lambda);
    }
}

#endif