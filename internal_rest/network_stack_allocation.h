#ifndef __DG_NETWORK_STACK_ALLOCATION_H__
#define __DG_NETWORK_STACK_ALLOCATION_H__

#include <stdint.h>
#include <stdlib.h> 
#include <memory>
#include <vector>
#include "network_concurrency.h"
#include "network_exception.h"

namespace dg_sock::network_stack_allocation{

    class StackAllocator{

        private:

            std::vector<size_t> stack_cursor_vec;
            std::unique_ptr<char[]> buf; //we aren't using vector - either string or raw allocation to make sure this is pointer arithmetic compatible
            size_t buf_sz;
            size_t cursor;

        public:

            StackAllocator(std::vector<size_t> stack_cursor_vec,
                           std::unique_ptr<char[]> buf,
                           size_t buf_sz,
                           size_t cursor) noexcept: stack_cursor_vec(std::move(stack_cursor_vec)),
                                                    buf(std::move(buf)),
                                                    buf_sz(buf_sz),
                                                    cursor(cursor){}

            inline auto enter_scope() noexcept -> exception_t{

                if (this->stack_cursor_vec.size() == this->stack_cursor_vec.capacity()){
                    return dg_sock::network_exception::RESOURCE_EXHAUSTION;
                }

                this->stack_cursor_vec.push_back(this->cursor);

                return dg_sock::network_exception::SUCCESS;
            }

            inline auto allocate(size_t blk_sz) noexcept -> std::expected<void *, exception_t>{

                if (blk_sz == 0u){
                    return static_cast<void *>(nullptr);
                }

                if (this->cursor + blk_sz > this->buf_sz){
                    return std::unexpected(dg_sock::network_exception::RESOURCE_EXHAUSTION);
                }

                void * rs       = std::next(this->buf.get(), this->cursor);
                this->cursor   += blk_sz;

                return rs;
            }

            inline void exit_scope() noexcept{

                this->cursor = this->stack_cursor_vec.back();
                this->stack_cursor_vec.pop_back();
            }
    };

    class SplitStackAllocator
    {
        private:

            struct MemorySegment
            {
                std::unique_ptr<char[]> buf;
                size_t buf_sz;
            };

            struct MemoryPoint
            {
                size_t slot;
                size_t offset;
            };

            std::vector<MemorySegment> stack_buffer_vec;
            std::vector<std::optional<MemoryPoint>> saved_point_vec;
            std::optional<MemoryPoint> valid_point;
        
        public:
            
            SplitStackAllocator(): stack_buffer_vec(),
                                   saved_point_vec(),
                                   valid_point(std::nullopt){}

            inline auto enter_scope() noexcept -> exception_t
            {
                MemoryPoint next_point;

                if (this->valid_point.has_value())
                {
                    next_point = this->valid_point.value();
                }
                else
                {
                    next_point = MemoryPoint
                    {
                        .slot   = 0u,
                        .offset = 0u
                    };

                    exception_t err = this->reserve_for_vector_size_of(1);

                    if (dg_sock::network_exception::is_failed(err))
                    {
                        return err;
                    }
                }

                try
                {
                    this->saved_point_vec.push_back(this->valid_point);
                }
                catch (...)
                {
                    return dg_sock::network_exception::wrap_std_exception(std::current_exception());
                }

                this->valid_point = next_point;

                return dg_sock::network_exception::SUCCESS;
            }

            inline auto allocate(size_t blk_sz) noexcept -> std::expected<void *, exception_t>
            {
                if (!this->valid_point.has_value())
                {
                    return std::unexpected(dg_sock::network_exception::BAD_STATE);
                }

                MemoryPoint cur_point = this->valid_point.value();

                while (true)
                {
                    size_t point_sz = this->stack_buffer_vec[cur_point.slot].buf_sz - cur_point.offset;

                    if (point_sz >= blk_sz)
                    {
                        void * rs   = std::next(this->stack_buffer_vec[cur_point.slot].buf.get(), cur_point.offset); 
                        cur_point   = MemoryPoint
                        {
                            .slot   = cur_point.slot,
                            .offset = cur_point.offset + blk_sz
                        };

                        this->valid_point   = cur_point;

                        return rs;
                    }

                    exception_t err = this->reserve_for_vector_size_of(cur_point.slot + 2u);

                    if (dg_sock::network_exception::is_failed(err))
                    {
                        return std::unexpected(err);
                    }

                    cur_point = MemoryPoint
                    {
                        .slot   = cur_point.slot + 1u,
                        .offset = 0u
                    };
                }
            }

            inline void exit_scope() noexcept
            {
                if (this->saved_point_vec.empty())
                {
                    std::abort();
                }

                this->valid_point = this->saved_point_vec.back();
                this->saved_point_vec.pop_back();
            }

        private:

            auto reserve_for_vector_size_of(size_t sz) noexcept -> exception_t
            {
                size_t new_sz   = std::max(sz, static_cast<size_t>(this->stack_buffer_vec.size()));
                size_t old_sz   = this->stack_buffer_vec.size();
                size_t diff_sz  = new_sz - old_sz; 

                try
                {
                    for (size_t i = 0u; i < diff_sz; ++i)
                    {
                        size_t offset                   = old_sz + i; 
                        size_t buf_vec_sz               = size_t{1} << offset;
                        std::unique_ptr<char[]> new_buf = std::make_unique<char[]>(buf_vec_sz);

                        this->stack_buffer_vec.push_back(MemorySegment
                        {
                            .buf    = std::move(new_buf),
                            .buf_sz = buf_vec_sz
                        });
                    }
                }
                catch (...)
                {
                    this->stack_buffer_vec.resize(old_sz);
                    return dg_sock::network_exception::wrap_std_exception(std::current_exception());
                }

                return dg_sock::network_exception::SUCCESS;
            }            
    };

    class ConcurrentAllocator{

        private:

            std::vector<std::unique_ptr<SplitStackAllocator>> allocator_vec;

        public:

            ConcurrentAllocator(std::vector<std::unique_ptr<SplitStackAllocator>> allocator_vec) noexcept: allocator_vec(std::move(allocator_vec)){}

            inline auto enter_scope() noexcept -> exception_t{

                size_t thr_idx = dg_sock::network_concurrency::this_thread_idx();
                return this->allocator_vec[thr_idx]->enter_scope();
            } 

            inline auto allocate(size_t buf_sz) noexcept -> std::expected<void *, exception_t>{

                size_t thr_idx = dg_sock::network_concurrency::this_thread_idx();
                return this->allocator_vec[thr_idx]->allocate(buf_sz);
            }

            inline void exit_scope() noexcept{

                size_t thr_idx = dg_sock::network_concurrency::this_thread_idx();
                this->allocator_vec[thr_idx]->exit_scope();
            }
    };

    struct ComponentFactory{

        static inline auto spawn_stack_allocator(size_t scope_size, size_t buf_sz) -> std::unique_ptr<StackAllocator>{

            const size_t MIN_SCOPE_SIZE = 0u;
            const size_t MAX_SCOPE_SIZE = size_t{1} << 20;
            const size_t MIN_BUF_SIZE   = 0u;
            const size_t MAX_BUF_SIZE   = size_t{1} << 40;

            if (std::clamp(scope_size, MIN_SCOPE_SIZE, MAX_SCOPE_SIZE) != scope_size){
                dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
            } 

            if (std::clamp(buf_sz, MIN_BUF_SIZE, MAX_BUF_SIZE) != buf_sz){
                dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
            }

            std::vector<size_t> stack_cursor_vec{};
            stack_cursor_vec.reserve(scope_size);
            std::unique_ptr<char[]> buf = std::make_unique<char[]>(buf_sz);
            size_t cursor = 0u; 

            return std::make_unique<StackAllocator>(std::move(stack_cursor_vec), std::move(buf), buf_sz, cursor);
        }

        static inline auto spawn_split_stack_allocator() -> std::unique_ptr<SplitStackAllocator>
        {
            return std::make_unique<SplitStackAllocator>();
        }

        static inline auto spawn_concurrent_allocator() -> std::unique_ptr<ConcurrentAllocator>{

            std::vector<std::unique_ptr<SplitStackAllocator>> stack_allocator_vec{};

            for (size_t i = 0u; i < dg_sock::network_concurrency::get_thread_count(); ++i){
                stack_allocator_vec.push_back(spawn_split_stack_allocator());
            }

            return std::make_unique<ConcurrentAllocator>(std::move(stack_allocator_vec));
        }
    };
    
    inline ConcurrentAllocator * volatile allocator;

    void init(){

        stdxx::memtransaction_guard tx_grd;

        auto tmp_allocator  = ComponentFactory::spawn_concurrent_allocator();
        allocator           = tmp_allocator.get();
        tmp_allocator.release();
    }

    void deinit() noexcept{

        stdxx::memtransaction_guard tx_grd;
        // allocator = nullptr;
        delete allocator;
    }

    auto get_allocator() noexcept -> ConcurrentAllocator *{

        std::atomic_signal_fence(std::memory_order_acquire);
        return allocator; //should've been volatile - but volatile is deprecating - so we must issue a memory flushes for concurrent devices - at the very beginning - and memory_order_acquire signal here 
    }

    struct internal_init_tag{};

    //we assume people are rational and only use stack allocations

    template <class T>
    class Allocation{

        private:

            T * obj;

            template <class ...Args>
            Allocation(const internal_init_tag, Args&& ...args){

                ConcurrentAllocator * allocator_ins     = get_allocator();
                exception_t err                         = allocator_ins->enter_scope();

                if (dg_sock::network_exception::is_failed(err)){
                    dg_sock::network_exception::throw_exception(err);
                }

                size_t allocation_sz                    = sizeof(T) + alignof(T) - 1u;
                std::expected<void *, exception_t> buf  = allocator_ins->allocate(allocation_sz);

                if (!buf.has_value()){
                    allocator_ins->exit_scope();
                    dg_sock::network_exception::throw_exception(buf.error());
                }

                void * head = stdxx::align_ptr(buf.value(), std::integral_constant<size_t, alignof(T)>{});

                if constexpr(std::is_nothrow_constructible_v<T, Args&&...>){
                    this->obj = new (head) T(std::forward<Args>(args)...);
                } else{
                    try{
                        this->obj = new (head) T(std::forward<Args>(args)...);
                    } catch (...){
                        allocator_ins->exit_scope();
                        std::rethrow_exception(std::current_exception());
                    }
                }
            }

        public:

            using self = Allocation;

            static_assert(std::is_nothrow_destructible_v<T>);
            static_assert(sizeof(T) != 0u);
            static_assert(alignof(T) != 0u);

            Allocation(): Allocation(internal_init_tag{}){}

            template <class ...Args>
            Allocation(const std::in_place_t, Args&& ...args): Allocation(internal_init_tag{}, std::forward<Args>(args)...){}

            Allocation(const self&) = delete;
            Allocation(self&&) = delete;
            self& operator =(const self&) = delete;
            self& operator =(self&&) = delete;

            ~Allocation() noexcept{

                std::destroy_at(this->obj);
                get_allocator()->exit_scope();
            }

            auto data() const noexcept -> T *{

                return this->obj;
            }

            auto get() const noexcept -> T *{

                return this->obj;
            }
    };

    template <class T>
    class Allocation<T[]>{

        private:

            T * arr;
            size_t arr_sz;

        public:

            static_assert(std::is_nothrow_destructible_v<T>);
            static_assert(sizeof(T) != 0u);
            static_assert(alignof(T) != 0u);

            using self = Allocation;

            Allocation(size_t sz){

                if (sz == 0u){
                    this->arr       = nullptr;
                    this->arr_sz    = 0u;
                    return;
                }

                ConcurrentAllocator * allocator_ins     = get_allocator();
                exception_t err                         = allocator_ins->enter_scope();

                if (dg_sock::network_exception::is_failed(err)){
                    dg_sock::network_exception::throw_exception(err);
                }

                size_t allocation_sz                    = sz * sizeof(T) + (alignof(T) - 1u);
                std::expected<void *, exception_t> buf  = allocator_ins->allocate(allocation_sz);

                if (!buf.has_value()){
                    allocator_ins->exit_scope();
                    dg_sock::network_exception::throw_exception(buf.error());
                }

                void * head = stdxx::align_ptr(buf.value(), std::integral_constant<size_t, alignof(T)>{}); 

                if constexpr(std::is_nothrow_default_constructible_v<T>){
                    this->arr       = new (head) T[sz];
                    this->arr_sz    = sz;
                } else{
                    try{
                        this->arr       = new (head) T[sz];
                        this->arr_sz    = sz;
                    } catch (...){
                        allocator_ins->exit_scope();
                        std::rethrow_exception(std::current_exception());
                    }
                }
            }

            Allocation(const self&) = delete;
            Allocation(self&&) = delete;
            self& operator =(const self&) = delete;
            self& operator =(self&&) = delete;

            ~Allocation() noexcept{

                if (this->arr == nullptr){
                    return;
                }

                std::destroy(this->arr, std::next(this->arr, this->arr_sz));
                get_allocator()->exit_scope();
            }

            auto data() const noexcept -> T *{

                return this->arr;
            }

            auto get() const noexcept -> T *{

                return this->arr;
            }

            auto operator[](size_t idx) const noexcept -> T&{

                return this->arr[idx];
            }
    };

    template <class T>
    class RawAllocation{};

    template <>
    class RawAllocation<char[]>{ //we are adding char specialization for raw_allocation

        private:

            char * arr;

        public:

            using self = RawAllocation;

            RawAllocation(size_t sz){

                if (sz == 0u){
                    this->arr = nullptr;
                    return;
                }

                ConcurrentAllocator * allocator_ins     = get_allocator();
                exception_t err                         = allocator_ins->enter_scope();

                if (dg_sock::network_exception::is_failed(err)){
                    dg_sock::network_exception::throw_exception(err);
                }

                std::expected<void *, exception_t> buf  = allocator_ins->allocate(sz);

                if (!buf.has_value()){
                    allocator_ins->exit_scope();
                    dg_sock::network_exception::throw_exception(buf.error());
                }

                this->arr = static_cast<char *>(buf.value());
            }

            RawAllocation(const self&) = delete;
            RawAllocation(self&&) = delete;

            self& operator =(const self&) = delete;
            self& operator =(self&&) = delete;

            __attribute__((noipa)) ~RawAllocation() noexcept{
                
                if (this->arr == nullptr){ //!arr
                    return;
                }

                get_allocator()->exit_scope();
            }

            auto data() const noexcept -> char *{

                return this->arr;
            }

            auto get() const noexcept -> char *{

                return this->arr;
            }

            auto operator[](size_t idx) const noexcept -> char&{

                return this->arr[idx];
            }
    };

    template <class T>
    class NoExceptAllocation: public Allocation<T>{

        public:

            NoExceptAllocation() noexcept: Allocation<T>(){}
            
            template <class ...Args>
            NoExceptAllocation(const std::in_place_t, Args&& ...args) noexcept: Allocation<T>(std::in_place_t{}, std::forward<Args>(args)...){}
    };

    template <class T>
    class NoExceptAllocation<T[]>: public Allocation<T[]>{

        public:

            NoExceptAllocation(size_t sz) noexcept: Allocation<T[]>(sz){}
    };

    template <class T>
    class NoExceptRawAllocation: public RawAllocation<T>{

        public:

            NoExceptRawAllocation() noexcept: RawAllocation<T>(){}

            template <class ...Args>
            NoExceptRawAllocation(const std::in_place_t, Args&& ...args) noexcept: RawAllocation<T>(std::in_place_t{}, std::forward<Args>(args)...){}
    };

    template <class T>
    class NoExceptRawAllocation<T[]>: public RawAllocation<T[]>{

        public:

            NoExceptRawAllocation(size_t sz) noexcept: RawAllocation<T[]>(sz){}
    };

} 

#endif