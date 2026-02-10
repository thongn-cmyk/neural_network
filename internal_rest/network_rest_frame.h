#ifndef __DG_NETWORK_REST_FRAME_H__
#define __DG_NETWORK_REST_FRAME_H__ 

//define HEADER_CONTROL 12

#include <stdint.h>
#include <stdlib.h>
#include "network_std_container.h"
#include <chrono>
#include "network_exception.h"
#include "stdx.h"
#include "network_log.h"
#include "network_compact_serializer.h"
#include <variant>
#include "network_kernel_mailbox.h"
 
namespace dg::network_rest_frame::model{

    using ticket_id_t   = uint64_t; //I've thought long and hard, it's better to do bitshift, because the otherwise would be breaking single responsibilities, breach of extensions
    using clock_id_t    = uint64_t; 

    static inline constexpr uint32_t INTERNAL_REQUEST_SERIALIZATION_SECRET  = 3312354321ULL;
    static inline constexpr uint32_t INTERNAL_RESPONSE_SERIALIZATION_SECRET = 3554488158ULL;
    static inline constexpr std::string_view REST_FRAME_VERSION_SUFFX       = std::string_view("REST_FRAME_V1"); //this is to actually solve the serialization problem, we can weed out the version problems, for the bad packets, we are guaranteed to filter that using the hashed value 

    using ipv6_storage_t        = std::array<char, 8u>;
    using ipv4_storage_t        = std::array<char, 4u>;
    using ip_storage_t          = std::variant<ipv4_storage_t, ipv6_storage_t>; 
    using native_id_storage_t   = std::array<char, 8u>; 
    using MailBoxArgument       = dg::network_kernel_mailbox::MailBoxArgument;
    using Address               = dg::network_kernel_mailbox::Address;

    struct CacheID{
        ip_storage_t ip;
        native_id_storage_t native_cache_id; 

        template <class Reflector>
        constexpr void dg_reflect(const Reflector& reflector) const noexcept{
            reflector(ip, native_cache_id);
        }

        template <class Reflector>
        constexpr void dg_reflect(const Reflector& reflector) noexcept{
            reflector(ip, native_cache_id);
        }
    };

    using cache_id_t    = CacheID; 

    //
    struct RequestID{
        ip_storage_t ip;
        native_id_storage_t native_request_id;

        template <class Reflector>
        constexpr void dg_reflect(const Reflector& reflector) const noexcept{
            reflector(ip, native_request_id);
        }

        template <class Reflector>
        constexpr void dg_reflect(const Reflector& reflector) noexcept{
            reflector(ip, native_request_id);
        }
    };

    using request_id_t = RequestID;

    struct ResourceAddress{
        Address remote_addr;
        dg::string resource_addr;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const{
            reflector(remote_addr, dg::network_compact_serializer::wrap_container<uint16_t>(resource_addr));
        }
        
        template <class Reflector>
        void dg_reflect(const Reflector& reflector){
            reflector(remote_addr, dg::network_compact_serializer::wrap_container<uint16_t>(resource_addr));
        }
    };

    struct ClientRequest{
        dg::string requestee_uri;
        dg::string requestor;
        dg::string payload;
        dg::string payload_serialization_format;

        std::chrono::nanoseconds client_timeout_dur;
        std::optional<std::chrono::time_point<std::chrono::utc_clock>> server_abs_timeout; //this is hard to solve, we can be stucked in a pipe and actually stay there forever, abs_timeout only works for post the transaction, which is already too late, I dont know of the way to do this correctly
        std::optional<request_id_t> designated_request_id; 

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const{
            reflector(requestee_uri, requestor, payload, 
                      payload_serialization_format, client_timeout_dur,
                      server_abs_timeout, designated_request_id);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector){
            reflector(requestee_uri, requestor, payload,
                      payload_serialization_format, client_timeout_dur,
                      server_abs_timeout, designated_request_id);
        }
    };

    struct Request{
        dg::string requestee_uri;
        dg::string requestor;
        dg::string payload;
        dg::string payload_serialization_format;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const{
            reflector(requestee_uri, requestor, payload, payload_serialization_format);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector){
            reflector(requestee_uri, requestor, payload, payload_serialization_format);
        }
    };

    struct Response{
        dg::string response;
        dg::string response_serialization_format;
        exception_t err_code;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const{
            reflector(response, response_serialization_format, err_code);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector){
            reflector(response, response_serialization_format, err_code);
        }
    };

    struct InternalRequest{
        Request request;
        ticket_id_t ticket_id;

        bool has_unique_response;
        std::optional<cache_id_t> client_request_cache_id;
        std::optional<std::chrono::time_point<std::chrono::utc_clock>> server_abs_timeout;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const{
            reflector(request, ticket_id, has_unique_response,
                      client_request_cache_id, server_abs_timeout);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector){
            reflector(request, ticket_id, has_unique_response,
                      client_request_cache_id, server_abs_timeout);
        }
    };

    struct InternalResponse{
        std::expected<Response, exception_t> response;
        ticket_id_t ticket_id;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const{
            reflector(response, ticket_id);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector){
            reflector(response, ticket_id);
        }
    };

    struct ClockInArgument{
        ticket_id_t clocked_in_ticket;
        std::chrono::nanoseconds expiry_dur;
    };
}

namespace dg::network_rest_frame::utility{

    using namespace dg::network_rest_frame::model;

    auto serialize_address(const Address& addr) noexcept -> std::expected<dg::string, exception_t>{

        return dg::network_exception::to_cstyle_function(dg::network_compact_serializer::dgstd_serialize<dg::string, Address>)(addr, 0u);
    }

    auto deserialize_address(const dg::string& addr) noexcept -> std::expected<Address, exception_t>{

        return dg::network_exception::to_cstyle_function(dg::network_compact_serializer::dgstd_deserialize<Address, dg::string>)(addr, 0u);
    }

    auto serialize_resource_address(const ResourceAddress& addr) noexcept -> std::expected<dg::string, exception_t>{

        return dg::network_exception::to_cstyle_function(dg::network_compact_serializer::dgstd_serialize<dg::string, ResourceAddress>)(addr, 0u);
    }

    auto deserialize_resource_address(const dg::string& addr) noexcept -> std::expected<ResourceAddress, exception_t>{

        return dg::network_exception::to_cstyle_function(dg::network_compact_serializer::dgstd_deserialize<ResourceAddress, dg::string>)(addr, 0u);
    }
}

namespace dg::network_rest_frame::server{

    using namespace dg::network_rest_frame::model;

    struct CacheControllerInterface{
        virtual ~CacheControllerInterface() noexcept = default;
        virtual void get_cache(cache_id_t * cache_id_arr, size_t sz, std::expected<std::optional<Response>, exception_t> * response_arr) noexcept = 0;
        virtual void insert_cache(cache_id_t * cache_id_arr, std::move_iterator<Response *> response_arr, size_t sz, std::expected<bool, exception_t> * rs_arr) noexcept = 0;
        virtual void contains(cache_id_t * cache_id_arr, size_t sz, bool * rs_arr) noexcept = 0;        
        virtual auto max_response_size() const noexcept -> size_t = 0;
        virtual void clear() noexcept = 0;
        virtual auto size() const noexcept -> size_t = 0;
        virtual auto capacity() const noexcept -> size_t = 0;
        virtual auto max_consume_size() noexcept -> size_t = 0; 
    };

    struct InfiniteCacheControllerInterface{
        virtual ~InfiniteCacheControllerInterface() noexcept = default;
        virtual void get_cache(cache_id_t * cache_id_arr, size_t sz, std::expected<std::optional<Response>, exception_t> * response_arr) noexcept = 0;
        virtual void insert_cache(cache_id_t * cache_id_arr, std::move_iterator<Response *> response_arr, size_t sz, std::expected<bool, exception_t> * rs_arr) noexcept = 0; //this is probably the most debatable in the C++ world, yet it has unique applications in this particular component, that's why we want to reimplement our containers literally every time
        virtual auto max_response_size() const noexcept -> size_t = 0;
        virtual auto max_consume_size() noexcept -> size_t = 0;
    };

    struct CacheUniqueWriteControllerInterface{
        virtual ~CacheUniqueWriteControllerInterface() noexcept = default;
        virtual void thru(cache_id_t * cache_id_arr, size_t sz, std::expected<bool, exception_t> * rs_arr) noexcept = 0;
        virtual void contains(cache_id_t * cache_id_arr, size_t sz, bool * rs_arr) noexcept = 0;
        virtual void clear() noexcept = 0;
        virtual auto size() const noexcept -> size_t = 0;
        virtual auto capacity() const noexcept -> size_t = 0;
        virtual auto max_consume_size() noexcept -> size_t = 0;
    };

    struct InfiniteCacheUniqueWriteControllerInterface{
        virtual ~InfiniteCacheUniqueWriteControllerInterface() noexcept = default;
        virtual void thru(cache_id_t * cache_id_arr, size_t sz, std::expected<bool, exception_t> * rs_arr) noexcept = 0;
        virtual auto max_consume_size() noexcept -> size_t = 0;
    };

    struct CacheUniqueWriteTrafficControllerInterface{
        virtual ~CacheUniqueWriteTrafficControllerInterface() noexcept = default;
        virtual auto thru(size_t) noexcept -> std::expected<bool, exception_t> = 0;
        virtual void reset() noexcept = 0;
    };

    struct DistributedCacheUniqueWriteTrafficControllerInterface: virtual CacheUniqueWriteTrafficControllerInterface{
        virtual ~DistributedCacheUniqueWriteTrafficControllerInterface() noexcept = default;
        virtual auto max_thru_size() const noexcept -> size_t = 0;
    };

    struct UpdatableInterface{
        virtual ~UpdatableInterface() noexcept = default;
        virtual void update() noexcept = 0;
    };

    struct RequestHandlerInterface{
        using Request   = model::Request;
        using Response  = model::Response; 

        virtual ~RequestHandlerInterface() noexcept = default;
        virtual void handle(std::move_iterator<Request *>, size_t, Response *) noexcept = 0;
        virtual auto max_consume_size() noexcept -> size_t = 0;
    };
}

namespace dg::network_rest_frame::client{

    using namespace dg::network_rest_frame::model;

    struct RequestIDGeneratorInterface{
        virtual ~RequestIDGeneratorInterface() noexcept = default;
        virtual auto get(size_t, RequestID *) noexcept -> exception_t = 0;
    };

    struct ResponseOnReadyObserverInterface{
        virtual ~ResponseOnReadyObserverInterface() = default;
        virtual void notify() noexcept = 0;
    };

    struct ResponseOnReadyNotifierInterface{
        virtual ~ResponseOnReadyNotifierInterface() = default;
        virtual auto test_or_subscribe(std::unique_ptr<ResponseOnReadyObserverInterface>) noexcept -> std::expected<bool, exception_t> = 0;
        virtual auto test_or_subscribe_sp(std::shared_ptr<ResponseOnReadyObserverInterface>) noexcept -> std::expected<bool, exception_t> = 0;
    };

    struct ResponseObserverInterface{
        virtual ~ResponseObserverInterface() noexcept = default;
        virtual void update(std::expected<Response, exception_t>) noexcept = 0;
        virtual void deferred_memory_ordering_fetch(std::expected<Response, exception_t>) noexcept = 0;
        virtual void deferred_memory_ordering_fetch_close() noexcept = 0;
    };

    struct BatchResponseInterface: virtual ResponseOnReadyNotifierInterface{
        virtual ~BatchResponseInterface() noexcept = default;
        virtual auto response() noexcept -> std::expected<dg::vector<std::expected<Response, exception_t>>, exception_t> = 0;
        virtual auto response_size() const noexcept -> size_t = 0;
    };

    struct ResponseInterface: virtual ResponseOnReadyNotifierInterface{
        virtual ~ResponseInterface() noexcept = default;
        virtual auto response() noexcept -> std::expected<Response, exception_t> = 0; 
    };

    struct RequestContainerInterface{
        virtual ~RequestContainerInterface() noexcept = default;
        virtual auto push(dg::vector<model::InternalRequest>&&) noexcept -> exception_t = 0;
        virtual auto pop() noexcept -> dg::vector<model::InternalRequest> = 0;
    };

    struct TicketControllerInterface{
        virtual ~TicketControllerInterface() noexcept = default;
        virtual auto open_ticket(size_t sz, model::ticket_id_t * rs) noexcept -> exception_t = 0;
        virtual void assign_observer(model::ticket_id_t * ticket_id_arr, size_t sz, std::add_pointer_t<ResponseObserverInterface> * assigning_observer_arr, std::expected<bool, exception_t> * exception_arr) noexcept = 0;
        virtual void steal_observer(model::ticket_id_t * ticket_id_arr, size_t sz, std::expected<std::add_pointer_t<ResponseObserverInterface>, exception_t> * out_observer_arr) noexcept = 0;
        virtual void close_ticket(model::ticket_id_t * ticket_id_arr, size_t sz) noexcept = 0;
        virtual auto max_consume_size() noexcept -> size_t = 0; 
    };

    struct DrainerPredicateInterface{
        virtual ~DrainerPredicateInterface() noexcept = default;
        virtual auto is_should_drain() noexcept -> bool = 0;
        virtual void reset() noexcept = 0;
    };

    struct TicketTimeoutManagerInterface{
        virtual ~TicketTimeoutManagerInterface() noexcept = default;
        virtual void clock_in(ClockInArgument * clockin_arr, size_t sz, exception_t * exception_arr) noexcept = 0;
        virtual void get_expired_ticket(model::ticket_id_t * output_arr, size_t& output_arr_sz, size_t output_arr_cap) noexcept = 0;
        virtual auto max_clockin_dur() const noexcept -> std::chrono::nanoseconds = 0;
        virtual void clear() noexcept = 0;
        virtual auto max_consume_size() noexcept -> size_t = 0;
    };

    struct RestControllerInterface{
        virtual ~RestControllerInterface() noexcept = default;
        virtual auto request(model::ClientRequest&&) noexcept -> std::expected<std::unique_ptr<ResponseInterface>, exception_t> = 0;
        virtual auto batch_request(std::move_iterator<model::ClientRequest *>, size_t) noexcept -> std::expected<std::unique_ptr<BatchResponseInterface>, exception_t> = 0;
        virtual auto get_designated_request_id(size_t, RequestID *) noexcept -> exception_t = 0;
        virtual auto max_consume_size() noexcept -> size_t = 0;
    };
}

namespace dg::network_rest_frame::server_impl1{

    using namespace dg::network_rest_frame::server; 

    //clear
    class CacheController: public virtual CacheControllerInterface{

        private:

            dg::unordered_unstable_map<cache_id_t, Response> cache_map;
            size_t cache_map_cap;
            size_t max_response_sz;
            size_t max_consume_per_load;

        public:

            CacheController(dg::unordered_unstable_map<cache_id_t, Response> cache_map,
                            size_t cache_map_cap,
                            size_t max_response_sz,
                            size_t max_consume_per_load) noexcept: cache_map(std::move(cache_map)),
                                                                   cache_map_cap(cache_map_cap),
                                                                   max_response_sz(max_response_sz),
                                                                   max_consume_per_load(std::move(max_consume_per_load)){}

            void get_cache(cache_id_t * cache_id_arr, size_t sz, std::expected<std::optional<Response>, exception_t> * rs_arr) noexcept{

                for (size_t i = 0u; i < sz; ++i){
                    auto map_ptr = std::as_const(this->cache_map).find(cache_id_arr[i]);

                    if (map_ptr == this->cache_map.end()){
                        rs_arr[i] = std::optional<Response>(std::nullopt);
                    } else{
                        std::expected<Response, exception_t> cpy_response = dg::network_exception::cstyle_initialize<Response>(map_ptr->second);

                        if (!cpy_response.has_value()){
                            rs_arr[i] = std::unexpected(cpy_response.error());
                        } else{
                            static_assert(std::is_nothrow_move_constructible_v<Response> && std::is_nothrow_move_assignable_v<Response>);
                            rs_arr[i] = std::optional<Response>(std::move(cpy_response.value()));
                        }
                    }
                }
            }

            void insert_cache(cache_id_t * cache_id_arr,
                              std::move_iterator<Response *> response_arr, size_t sz,
                              std::expected<bool, exception_t> * rs_arr) noexcept{

                if constexpr(DEBUG_MODE_FLAG){
                    if (sz > this->max_consume_size()){
                        dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                Response * base_response_arr = response_arr.base(); 

                for (size_t i = 0u; i < sz; ++i){
                    if (this->cache_map.size() == this->cache_map_cap){
                        rs_arr[i] = std::unexpected(dg::network_exception::RESOURCE_EXHAUSTION);
                        continue;
                    }

                    if (base_response_arr[i].response.size() > this->max_response_sz){
                        rs_arr[i] = std::unexpected(dg::network_exception::REST_CACHE_MAX_RESPONSE_SIZE_REACHED);
                        continue;
                    }

                    static_assert(std::is_nothrow_move_constructible_v<Response> && std::is_nothrow_move_assignable_v<Response>);

                    auto insert_token   = std::make_pair(cache_id_arr[i], std::move(base_response_arr[i]));
                    auto [_, status]    = this->cache_map.insert(std::move(insert_token)); //
                    rs_arr[i]           = status;
                }
            }

            void contains(cache_id_t * cache_id_arr, size_t sz, bool * rs_arr) noexcept{

                for (size_t i = 0u; i < sz; ++i){
                    rs_arr[i] = this->cache_map.contains(cache_id_arr[i]);
                }
            }

            auto max_response_size() const noexcept -> size_t{

                return this->max_response_sz;
            }

            void clear() noexcept{

                this->cache_map.clear();
            }

            auto size() const noexcept -> size_t{

                return this->cache_map.size();
            }

            auto capacity() const noexcept -> size_t{

                return this->cache_map_cap;
            }

            auto max_consume_size() noexcept -> size_t{

                return this->max_consume_per_load;
            }
    };

    //clear
    class MutexControlledCacheController: public virtual CacheControllerInterface{

        private:

            std::unique_ptr<CacheControllerInterface> base;
            std::unique_ptr<std::mutex> mtx;

        public:

            MutexControlledCacheController(std::unique_ptr<CacheControllerInterface> base,
                                           std::unique_ptr<std::mutex> mtx) noexcept: base(std::move(base)),
                                                                                      mtx(std::move(mtx)){}

            void get_cache(cache_id_t * cache_id_arr, size_t sz, std::expected<std::optional<Response>, exception_t> * rs_arr) noexcept{

                stdx::xlock_guard<std::mutex> lck_grd(*this->mtx);
                this->base->get_cache(cache_id_arr, sz, rs_arr);
            }

            void insert_cache(cache_id_t * cache_id_arr, std::move_iterator<Response *> response_arr, size_t sz, std::expected<bool, exception_t> * rs_arr) noexcept{

                stdx::xlock_guard<std::mutex> lck_grd(*this->mtx);
                this->base->insert_cache(cache_id_arr, response_arr, sz, rs_arr);
            }

            void clear() noexcept{

                stdx::xlock_guard<std::mutex> lck_grd(*this->mtx);
                this->base->clear();
            }

            void contains(cache_id_t * cache_id_arr, size_t sz, bool * rs_arr) noexcept{

                stdx::xlock_guard<std::mutex> lck_grd(*this->mtx);
                this->base->contains(cache_id_arr, sz, rs_arr);
            }

            auto size() const noexcept -> size_t{

                stdx::xlock_guard<std::mutex> lck_grd(*this->mtx);
                return this->base->size();
            }

            auto capacity() const noexcept -> size_t{

                stdx::xlock_guard<std::mutex> lck_grd(*this->mtx);
                return this->base->capacity();
            }

            auto max_response_size() const noexcept -> size_t{

                stdx::xlock_guard<std::mutex> lck_grd(*this->mtx);
                return this->base->max_response_size(); 
            } 

            auto max_consume_size() noexcept -> size_t{

                //assumptions
                return this->base->max_consume_size();
            }
    };

    //clear
    class SwitchingCacheController: public virtual InfiniteCacheControllerInterface{

        private:

            std::unique_ptr<CacheControllerInterface> left;
            std::unique_ptr<CacheControllerInterface> right;

            bool operating_side;
            size_t switch_population_counter;
            size_t switch_population_threshold; 
            size_t getcache_feed_cap;
            size_t insertcache_feed_cap;

            size_t max_consume_per_load;

        public:

            SwitchingCacheController(std::unique_ptr<CacheControllerInterface> left,
                                     std::unique_ptr<CacheControllerInterface> right,
                                     bool operating_side,
                                     size_t switch_population_counter,
                                     size_t switch_population_threshold,
                                     size_t getcache_feed_cap,
                                     size_t insertcache_feed_cap,
                                     size_t max_consume_per_load) noexcept: left(std::move(left)),
                                                                            right(std::move(right)),
                                                                            operating_side(std::move(operating_side)),
                                                                            switch_population_counter(std::move(switch_population_counter)),
                                                                            switch_population_threshold(std::move(switch_population_threshold)),
                                                                            getcache_feed_cap(getcache_feed_cap),
                                                                            insertcache_feed_cap(insertcache_feed_cap),
                                                                            max_consume_per_load(max_consume_per_load){}

            void get_cache(cache_id_t * cache_id_arr, size_t sz, std::expected<std::optional<Response>, exception_t> * rs_arr) noexcept{

                CacheControllerInterface * major_cache_controller   = nullptr;
                CacheControllerInterface * minor_cache_controller   = nullptr;

                if (this->operating_side == false){
                    major_cache_controller  = this->right.get();
                    minor_cache_controller  = this->left.get();
                }  else{
                    major_cache_controller  = this->left.get();
                    minor_cache_controller  = this->right.get();
                }

                major_cache_controller->get_cache(cache_id_arr, sz, rs_arr);

                auto feed_resolutor                 = InternalGetCacheFeedResolutor{};
                feed_resolutor.dst                  = minor_cache_controller; 

                size_t trimmed_getcache_feed_cap    = std::min(this->getcache_feed_cap, sz);
                size_t feeder_allocation_cost       = dg::network_producer_consumer::delvrsrv_allocation_cost(&feed_resolutor, trimmed_getcache_feed_cap);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> feeder_mem(feeder_allocation_cost);
                auto feeder                         = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_open_preallocated_raiihandle(&feed_resolutor, trimmed_getcache_feed_cap, feeder_mem.get())); 

                for (size_t i = 0u; i < sz; ++i){
                    if (!rs_arr[i].has_value()){
                        continue;
                    }

                    if (rs_arr[i].value().has_value()){
                        continue;
                    }

                    auto feed_arg       = InternalGetCacheFeedArgument{};
                    feed_arg.cache_id   = cache_id_arr[i];
                    feed_arg.rs         = std::next(rs_arr, i);

                    dg::network_producer_consumer::delvrsrv_deliver(feeder.get(), feed_arg);
                }
            }

            void insert_cache(cache_id_t * cache_id_arr, std::move_iterator<Response *> response_arr, size_t sz, std::expected<bool, exception_t> * rs_arr) noexcept{

                //range is the most important thing in this world

                if constexpr(DEBUG_MODE_FLAG){
                    if (sz > this->max_consume_size()){
                        dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }                    
                }

                Response * base_response_arr = response_arr.base();

                if (this->switch_population_counter + sz >= this->switch_population_threshold){
                    this->internal_dispatch_switch();
                }

                //this is the most confusing logic of programming
                //assume internal_dispatch_switch is not triggered
                //we are in a normal state of infinite unordered_map
                
                //assume internal_dispatch_switch is triggered, we guarantee at least (this->switch_population_threshold - sz) of immediate previous records, and snap the state into a correct state upon exit 
                //we also guarantee that post switch capacity should suffice for <sz>

                //we don't worry about what happens next, the two state propagations are the things that we worry about

                CacheControllerInterface * current_cache_controller = nullptr;
                CacheControllerInterface * other_cache_controller   = nullptr;

                if (this->operating_side == false){
                    current_cache_controller    = this->left.get();
                    other_cache_controller      = this->right.get();
                } else{
                    current_cache_controller    = this->right.get();
                    other_cache_controller      = this->left.get();
                }

                auto feed_resolutor                 = InternalInsertCacheFeedResolutor{};
                feed_resolutor.insert_incrementor   = &this->switch_population_counter;
                feed_resolutor.dst                  = current_cache_controller; 

                size_t trimmed_insertcache_feed_cap = std::min(std::min(this->insertcache_feed_cap, current_cache_controller->max_consume_size()), sz);
                size_t feeder_allocation_cost       = dg::network_producer_consumer::delvrsrv_allocation_cost(&feed_resolutor, trimmed_insertcache_feed_cap);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> feeder_mem(feeder_allocation_cost);
                auto feeder                         = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_open_preallocated_raiihandle(&feed_resolutor, trimmed_insertcache_feed_cap, feeder_mem.get())); 

                dg::network_stack_allocation::NoExceptAllocation<bool[]> contain_status_arr(sz);
                other_cache_controller->contains(cache_id_arr, sz, contain_status_arr.get());

                for (size_t i = 0u; i < sz; ++i){
                    if (contain_status_arr[i]){
                        rs_arr[i] = false; 
                        continue;
                    }

                    auto feed_arg   = InternalInsertCacheFeedArgument{.cache_id     = cache_id_arr[i],
                                                                      .response     = std::move(base_response_arr[i]),
                                                                      .rs           = std::next(rs_arr, i),
                                                                      .fallback_ptr = std::next(base_response_arr, i)};

                    dg::network_producer_consumer::delvrsrv_deliver(feeder.get(), std::move(feed_arg));
                }
            }

            auto max_response_size() const noexcept -> size_t{

                return std::min(this->left->max_response_size(), this->right->max_response_size());                
            }

            auto max_consume_size() noexcept -> size_t{

                return this->max_consume_per_load;
            }

        private:

            struct InternalInsertCacheFeedArgument{
                cache_id_t cache_id;
                Response response;
                std::expected<bool, exception_t> * rs;
                Response * fallback_ptr;
            };

            struct InternalInsertCacheFeedResolutor: dg::network_producer_consumer::ConsumerInterface<InternalInsertCacheFeedArgument>{

                size_t * insert_incrementor;
                CacheControllerInterface * dst;

                void push(std::move_iterator<InternalInsertCacheFeedArgument *> data_arr, size_t sz) noexcept{

                    InternalInsertCacheFeedArgument * base_data_arr = data_arr.base();

                    dg::network_stack_allocation::NoExceptAllocation<cache_id_t[]> cache_id_arr(sz);
                    dg::network_stack_allocation::NoExceptAllocation<Response[]> response_arr(sz);
                    dg::network_stack_allocation::NoExceptAllocation<std::expected<bool, exception_t>[]> rs_arr(sz);

                    for (size_t i = 0u; i < sz; ++i){
                        cache_id_arr[i] = base_data_arr[i].cache_id;
                        response_arr[i] = std::move(base_data_arr[i].response);
                    }

                    this->dst->insert_cache(cache_id_arr.get(), std::make_move_iterator(response_arr.get()), sz, rs_arr.get());

                    for (size_t i = 0u; i < sz; ++i){
                        *base_data_arr[i].rs = rs_arr[i];

                        if (!rs_arr[i].has_value()){
                            *base_data_arr[i].fallback_ptr = std::move(response_arr[i]);
                            continue;
                        }

                        if (!rs_arr[i].value()){
                            continue;
                        }

                        *this->insert_incrementor += 1u;
                    }     
                }
            };

            struct InternalGetCacheFeedArgument{
                cache_id_t cache_id;
                std::expected<std::optional<Response>, exception_t> * rs;
            };

            struct InternalGetCacheFeedResolutor: dg::network_producer_consumer::ConsumerInterface<InternalGetCacheFeedArgument>{

                CacheControllerInterface * dst;

                void push(std::move_iterator<InternalGetCacheFeedArgument *> data_arr, size_t sz) noexcept{

                    InternalGetCacheFeedArgument * base_data_arr = data_arr.base();

                    dg::network_stack_allocation::NoExceptAllocation<cache_id_t[]> cache_id_arr(sz);
                    dg::network_stack_allocation::NoExceptAllocation<std::expected<std::optional<Response>, exception_t>[]> rs_arr(sz);

                    for (size_t i = 0u; i < sz; ++i){
                        cache_id_arr[i] = base_data_arr[i].cache_id;
                    }

                    this->dst->get_cache(cache_id_arr.get(), sz, rs_arr.get());

                    for (size_t i = 0u; i < sz; ++i){
                        static_assert(std::is_nothrow_move_assignable_v<std::expected<std::optional<Response>, exception_t>>);
                        *base_data_arr[i].rs = std::move(rs_arr[i]);
                    }
                }
            };

            void internal_dispatch_switch() noexcept{

                if (this->operating_side == false){
                    this->right->clear();
                } else{
                    this->left->clear();
                }

                this->operating_side            = !this->operating_side;
                this->switch_population_counter = 0u;
            }
    };

    //clear
    class AdvancedInfiniteCacheController: public virtual InfiniteCacheControllerInterface{

        private:

            dg::cyclic_unordered_node_map<cache_id_t, Response> cache_map;
            size_t max_response_sz;
            size_t max_consume_per_load;

        public:

            AdvancedInfiniteCacheController(dg::cyclic_unordered_node_map<cache_id_t, Response> cache_map,
                                            size_t max_response_sz,
                                            size_t max_consume_per_load) noexcept: cache_map(std::move(cache_map)),
                                                                                   max_response_sz(max_response_sz),
                                                                                   max_consume_per_load(max_consume_per_load){}

            void get_cache(cache_id_t * cache_id_arr, size_t sz, std::expected<std::optional<Response>, exception_t> * response_arr) noexcept{

                for (size_t i = 0u; i < sz; ++i){
                    auto map_ptr = this->cache_map.find(cache_id_arr[i]);

                    if (map_ptr == this->cache_map.end()){
                        response_arr[i] = std::optional<Response>(std::nullopt);
                        continue;
                    }

                    std::expected<Response, exception_t> response_cpy = dg::network_exception::cstyle_initialize<Response>(map_ptr->second);

                    if (!response_cpy.has_value()){
                        response_arr[i] = std::unexpected(response_cpy.error());
                        continue;
                    }

                    response_arr[i] = std::optional<Response>(std::move(response_cpy.value()));
                }
            }

            void insert_cache(cache_id_t * cache_id_arr,
                              std::move_iterator<Response *> response_arr, size_t sz,
                              std::expected<bool, exception_t> * rs_arr) noexcept{

                if constexpr(DEBUG_MODE_FLAG){
                    if (sz > this->max_consume_size()){
                        dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                Response * base_response_arr = response_arr.base();

                for (size_t i = 0u; i < sz; ++i){
                    if (base_response_arr[i].response.size() > this->max_response_size()){
                        rs_arr[i] = std::unexpected(dg::network_exception::REST_CACHE_MAX_RESPONSE_SIZE_REACHED);
                        continue;
                    }

                    auto insert_token   = std::make_pair(cache_id_arr[i], std::move(base_response_arr[i]));
                    auto [_, status]    = this->cache_map.insert(std::move(insert_token));
                    rs_arr[i]           = status;
                }
            }

            auto max_response_size() const noexcept -> size_t{

                return this->max_response_sz;
            }

            auto max_consume_size() noexcept -> size_t{

                return this->max_consume_per_load;
            }
    };

    //clear
    class MutexControlledInfiniteCacheController: public virtual InfiniteCacheControllerInterface{

        private:

            std::unique_ptr<InfiniteCacheControllerInterface> base;
            std::unique_ptr<std::mutex> mtx;

        public:

            MutexControlledInfiniteCacheController(std::unique_ptr<InfiniteCacheControllerInterface> base,
                                                   std::unique_ptr<std::mutex> mtx) noexcept: base(std::move(base)),
                                                                                              mtx(std::move(mtx)){}

            void get_cache(cache_id_t * cache_id_arr, size_t sz, std::expected<std::optional<Response>, exception_t> * rs_arr) noexcept{

                stdx::xlock_guard<std::mutex> lck_grd(*this->mtx);
                this->base->get_cache(cache_id_arr, sz, rs_arr);
            }

            void insert_cache(cache_id_t * cache_id_arr,
                              std::move_iterator<Response *> response_arr, size_t sz,
                              std::expected<bool, exception_t> * rs_arr) noexcept{

                stdx::xlock_guard<std::mutex> lck_grd(*this->mtx);
                this->base->insert_cache(cache_id_arr, response_arr, sz, rs_arr);
            }

            auto max_response_size() const noexcept -> size_t{

                stdx::xlock_guard<std::mutex> lck_grd(*this->mtx);
                return this->base->max_response_size();
            }

            auto max_consume_size() noexcept -> size_t{

                return this->base->max_consume_size();
            }
    };

    //clear
    template <class Hasher>
    class DistributedCacheController: public virtual InfiniteCacheControllerInterface{

        private:

            std::unique_ptr<std::unique_ptr<InfiniteCacheControllerInterface>[]> cache_controller_arr;
            size_t pow2_cache_controller_arr_sz;
            size_t getcache_keyvalue_feed_cap;
            size_t insertcache_keyvalue_feed_cap;
            size_t max_consume_per_load;
            size_t max_response_sz;
            Hasher hasher;

        public:

            DistributedCacheController(std::unique_ptr<std::unique_ptr<InfiniteCacheControllerInterface>[]> cache_controller_arr,
                                       size_t pow2_cache_controller_arr_sz,
                                       size_t getcache_keyvalue_feed_cap,
                                       size_t insertcache_keyvalue_feed_cap,
                                       size_t max_consume_per_load,
                                       size_t max_response_sz,
                                       Hasher hasher) noexcept: cache_controller_arr(std::move(cache_controller_arr)),
                                                                pow2_cache_controller_arr_sz(pow2_cache_controller_arr_sz),
                                                                getcache_keyvalue_feed_cap(getcache_keyvalue_feed_cap),
                                                                insertcache_keyvalue_feed_cap(insertcache_keyvalue_feed_cap),
                                                                max_consume_per_load(std::move(max_consume_per_load)),
                                                                max_response_sz(max_response_sz),
                                                                hasher(std::move(hasher)){}

            void get_cache(cache_id_t * cache_id_arr, size_t sz, std::expected<std::optional<Response>, exception_t> * rs_arr) noexcept{

                auto feed_resolutor                 = InternalGetCacheFeedResolutor{};
                feed_resolutor.cache_controller_arr = this->cache_controller_arr.get();

                size_t trimmed_keyvalue_feed_cap    = std::min(this->getcache_keyvalue_feed_cap, sz);
                size_t feeder_allocation_cost       = dg::network_producer_consumer::delvrsrv_kv_allocation_cost(&feed_resolutor, trimmed_keyvalue_feed_cap);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> feeder_mem(feeder_allocation_cost);
                auto feeder                         = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_kv_open_preallocated_raiihandle(&feed_resolutor, trimmed_keyvalue_feed_cap, feeder_mem.get()));

                for (size_t i = 0u; i < sz; ++i){
                    size_t hashed_cache_id_value    = stdx::safe_integer_cast<size_t>(this->hasher(cache_id_arr[i]));
                    size_t partitioned_idx          = hashed_cache_id_value & (this->pow2_cache_controller_arr_sz - 1u);
                    auto feed_arg                   = InternalGetCacheFeedArgument{.cache_id    = cache_id_arr[i],
                                                                                   .rs_ptr      = std::next(rs_arr, i)};

                    dg::network_producer_consumer::delvrsrv_kv_deliver(feeder.get(), partitioned_idx, feed_arg);
                }
            }

            void insert_cache(cache_id_t * cache_id_arr,
                              std::move_iterator<Response *> response_arr, size_t sz,
                              std::expected<bool, exception_t> * rs_arr) noexcept{

                if constexpr(DEBUG_MODE_FLAG){
                    if (sz > this->max_consume_size()){
                        dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                Response * base_response_arr        = response_arr.base();

                auto feed_resolutor                 = InternalCacheInsertFeedResolutor{};
                feed_resolutor.cache_controller_arr = this->cache_controller_arr.get();

                size_t trimmed_keyvalue_feed_cap    = std::min(this->insertcache_keyvalue_feed_cap, sz);
                size_t feeder_allocation_cost       = dg::network_producer_consumer::delvrsrv_kv_allocation_cost(&feed_resolutor, trimmed_keyvalue_feed_cap);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> feeder_mem(feeder_allocation_cost);
                auto feeder                         = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_kv_open_preallocated_raiihandle(&feed_resolutor, trimmed_keyvalue_feed_cap, feeder_mem.get()));

                for (size_t i = 0u; i < sz; ++i){
                    size_t hashed_cache_id_value    = stdx::safe_integer_cast<size_t>(this->hasher(cache_id_arr[i]));
                    size_t partitioned_idx          = hashed_cache_id_value & (this->pow2_cache_controller_arr_sz - 1u);
                    auto feed_arg                   = InternalCacheInsertFeedArgument{.cache_id     = cache_id_arr[i],
                                                                                      .response_ptr = std::make_move_iterator(std::next(base_response_arr, i)),
                                                                                      .rs           = std::next(rs_arr, i)};

                    dg::network_producer_consumer::delvrsrv_kv_deliver(feeder.get(), partitioned_idx, std::move(feed_arg));
                }
            }

            auto max_response_size() const noexcept -> size_t{

                return this->max_response_sz;
            }

            auto max_consume_size() noexcept -> size_t{

                return this->max_consume_per_load;
            }

        private:

            struct InternalGetCacheFeedArgument{
                cache_id_t cache_id;
                std::expected<std::optional<Response>, exception_t> * rs_ptr;
            };

            struct InternalGetCacheFeedResolutor: dg::network_producer_consumer::KVConsumerInterface<size_t, InternalGetCacheFeedArgument>{

                std::unique_ptr<InfiniteCacheControllerInterface> * cache_controller_arr;

                void push(const size_t& partitioned_idx, std::move_iterator<InternalGetCacheFeedArgument *> data_arr, size_t sz) noexcept{

                    InternalGetCacheFeedArgument * base_data_arr = data_arr.base();

                    dg::network_stack_allocation::NoExceptAllocation<cache_id_t[]> cache_id_arr(sz);
                    dg::network_stack_allocation::NoExceptAllocation<std::expected<std::optional<Response>, exception_t>[]> rs_arr(sz);

                    for (size_t i = 0u; i < sz; ++i){
                        cache_id_arr[i] = base_data_arr[i].cache_id;
                    }

                    this->cache_controller[partitioned_idx]->get_cache(cache_id_arr.get(), sz, rs_arr.get());

                    for (size_t i = 0u; i < sz; ++i){
                        //we should standardize the move_if_noexcept
                        *base_data_arr[i].rs_ptr = std::move(rs_arr[i]);
                    }
                }
            };

            struct InternalCacheInsertFeedArgument{
                cache_id_t cache_id;
                std::move_iterator<Response *> response_ptr;
                std::expected<bool, exception_t> * rs;
            };

            struct InternalCacheInsertFeedResolutor: dg::network_producer_consumer::KVConsumerInterface<size_t, InternalCacheInsertFeedArgument>{

                std::unique_ptr<InfiniteCacheControllerInterface> * cache_controller_arr;

                void push(const size_t& partitioned_idx, std::move_iterator<InternalCacheInsertFeedArgument *> data_arr, size_t sz) noexcept{

                    InternalCacheInsertFeedArgument * base_data_arr = data_arr.base();

                    dg::network_stack_allocation::NoExceptAllocation<cache_id_t[]> cache_id_arr(sz);
                    dg::network_stack_allocation::NoExceptAllocation<Response[]> response_arr(sz);
                    dg::network_stack_allocation::NoExceptAllocation<std::expected<bool, exception_t>[]> rs_arr(sz);

                    for (size_t i = 0u; i < sz; ++i){
                        cache_id_arr[i]                 = base_data_arr[i].cache_id;
                        Response * base_response_ptr    = base_data_arr[i].response_ptr.base();
                        response_arr[i]                 = std::move(*base_response_ptr);
                    }

                    this->cache_controller_arr[partitioned_idx]->insert_cache(cache_id_arr.get(),
                                                                              std::make_move_iterator(response_arr.get()), sz,
                                                                              rs_arr.get());

                    for (size_t i = 0u; i < sz; ++i){
                        *base_data_arr[i].rs = rs_arr[i];

                        if (!rs_arr[i].has_value()){
                            Response * base_response_ptr    = base_data_arr[i].response_ptr.base();
                            *base_response_ptr              = std::move(response_arr[i]);
                        }
                    }
                }
            };
    };

    //we'll would like to use bloom filters yet we have found a reason to do so, we are micro optimizing
    //clear
    class CacheUniqueWriteController: public virtual CacheUniqueWriteControllerInterface{

        private:

            dg::unordered_unstable_set<cache_id_t> cache_id_set;
            size_t cache_id_set_cap;
            size_t max_consume_per_load;

        public:

            CacheUniqueWriteController(dg::unordered_unstable_set<cache_id_t> cache_id_set,
                                       size_t cache_id_set_cap,
                                       size_t max_consume_per_load) noexcept: cache_id_set(std::move(cache_id_set)),
                                                                              cache_id_set_cap(cache_id_set_cap),
                                                                              max_consume_per_load(max_consume_per_load){}

            void thru(cache_id_t * cache_id_arr, size_t sz, std::expected<bool, exception_t> * rs_arr) noexcept{

                if constexpr(DEBUG_MODE_FLAG){
                    if (sz > this->max_consume_size()){
                        dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                for (size_t i = 0u; i < sz; ++i){
                    auto set_ptr = this->cache_id_set.find(cache_id_arr[i]);

                    if (set_ptr != this->cache_id_set.end()){
                        rs_arr[i] = false; //false, found, already thru
                        continue;
                    }

                    //unique, try to insert

                    if (this->cache_id_set.size() == this->cache_id_set_cap){
                        //cap reached, return cap exception, no_actions 
                        rs_arr[i] = std::unexpected(dg::network_exception::RESOURCE_EXHAUSTION);
                        continue;
                    }

                    auto [_, status]  = this->cache_id_set.insert(cache_id_arr[i]);
                    dg::network_exception_handler::dg_assert(status);
                    rs_arr[i] = true; //thru, uniqueness acknowledged by cache_id_set
                }
            }

            void contains(cache_id_t * cache_id_arr, size_t sz, bool * rs_arr) noexcept{

                for (size_t i = 0u; i < sz; ++i){
                    rs_arr[i] = this->cache_id_set.contains(cache_id_arr[i]);
                }
            }

            void clear() noexcept{

                this->cache_id_set.clear();
            }

            auto size() const noexcept -> size_t{

                return this->cache_id_set.size();
            }

            auto capacity() const noexcept -> size_t{

                return this->cache_id_set_cap;
            }

            auto max_consume_size() noexcept -> size_t{

                return this->max_consume_per_load;
            }
    };

    //clear
    class MutexControlledCacheWriteExclusionController: public virtual CacheUniqueWriteControllerInterface{

        private:

            std::unique_ptr<CacheUniqueWriteControllerInterface> base;
            std::unique_ptr<std::mutex> mtx;

        public:

            MutexControlledCacheWriteExclusionController(std::unique_ptr<CacheUniqueWriteControllerInterface> base,
                                                         std::unique_ptr<std::mutex> mtx) noexcept: base(std::move(base)),
                                                                                                    mtx(std::move(mtx)){}

            void thru(cache_id_t * cache_id_arr, size_t sz, std::expected<bool, exception_t> * rs_arr) noexcept{

                stdx::xlock_guard<std::mutex> lck_grd(*this->mtx);
                this->base->thru(cache_id_arr, sz, rs_arr);
            }

            void contains(cache_id_t * cache_id_arr, size_t sz, bool * rs_arr) noexcept{

                stdx::xlock_guard<std::mutex> lck_grd(*this->mtx);
                this->base->contains(cache_id_arr, sz, rs_arr);
            }

            void clear() noexcept{

                stdx::xlock_guard<std::mutex> lck_grd(*this->mtx);
                this->base->clear();
            }

            auto size() const noexcept -> size_t{

                stdx::xlock_guard<std::mutex> lck_grd(*this->mtx);
                return this->base->size();
            }

            auto capacity() const noexcept -> size_t{

                stdx::xlock_guard<std::mutex> lck_grd(*this->mtx);
                return this->base->capacity();
            }

            auto max_consume_size() noexcept -> size_t{

                return this->base->max_consume_size();
            }
    };

    //clear
    class SwitchingCacheWriteExclusionController: public virtual InfiniteCacheUniqueWriteControllerInterface{

        private:

            std::unique_ptr<CacheUniqueWriteControllerInterface> left_controller;
            std::unique_ptr<CacheUniqueWriteControllerInterface> right_controller;

            bool operating_side;
            size_t switch_population_counter;
            size_t switch_population_threshold;
            size_t thru_feed_cap;
            size_t max_consume_per_load;

        public:

            SwitchingCacheWriteExclusionController(std::unique_ptr<CacheUniqueWriteControllerInterface> left_controller,
                                                   std::unique_ptr<CacheUniqueWriteControllerInterface> right_controller,
                                                   bool operating_side,
                                                   size_t switch_population_counter,
                                                   size_t switch_population_threshold,
                                                   size_t thru_feed_cap,
                                                   size_t max_consume_per_load) noexcept: left_controller(std::move(left_controller)),
                                                                                          right_controller(std::move(right_controller)),
                                                                                          operating_side(operating_side),
                                                                                          switch_population_counter(switch_population_counter),
                                                                                          switch_population_threshold(switch_population_threshold),
                                                                                          thru_feed_cap(thru_feed_cap),
                                                                                          max_consume_per_load(max_consume_per_load){}

            void thru(cache_id_t * cache_id_arr, size_t sz, std::expected<bool, exception_t> * rs_arr) noexcept{

                if constexpr(DEBUG_MODE_FLAG){
                    if (sz > this->max_consume_size()){
                        dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                if (this->switch_population_counter + sz >= this->switch_population_threshold){
                    this->internal_dispatch_switch();
                }

                //if there is no switch being dispatched, we are fine
                //if there is a switch dispatch, we are guaranteed to have at least switch_population_threshold - max_consume_sz previous immediate records
                //we also need to make sure that sz is able to fit the capacity post the switch (or max_consume_sz < either capacity()), this is construction dep injection's responsibiltiy 
                //we dont really care about what happens next, what happens previously, we need to stay on the induction course, this is the most important rule of programming

                CacheUniqueWriteControllerInterface * current_write_controller  = nullptr;
                CacheUniqueWriteControllerInterface * other_write_controller    = nullptr;

                if (this->operating_side == false){
                    current_write_controller    = this->left_controller.get();
                    other_write_controller      = this->right_controller.get();
                } else{
                    current_write_controller    = this->right_controller.get();
                    other_write_controller      = this->left_controller.get();
                }

                auto feed_resolutor                 = InternalThruWriteFeedResolutor{};
                feed_resolutor.thru_incrementor     = &this->switch_population_counter;
                feed_resolutor.dst                  = current_write_controller;

                size_t trimmed_thru_feed_cap        = std::min(std::min(this->thru_feed_cap, current_write_controller->max_consume_size()), sz);
                size_t feeder_allocation_cost       = dg::network_producer_consumer::delvrsrv_allocation_cost(&feed_resolutor, trimmed_thru_feed_cap);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> feeder_mem(feeder_allocation_cost);
                auto feeder                         = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_open_preallocated_raiihandle(&feed_resolutor, trimmed_thru_feed_cap, feeder_mem.get())); 

                dg::network_stack_allocation::NoExceptAllocation<bool[]> contain_status_arr(sz);
                other_write_controller->contains(cache_id_arr, sz, contain_status_arr.get());

                for (size_t i = 0u; i < sz; ++i){
                    if (contain_status_arr[i]){
                        rs_arr[i] = false; //already thru
                        continue;
                    }

                    auto feed_arg   = InternalThruWriteFeedArgument{.cache_id   = cache_id_arr[i],
                                                                    .rs         = std::next(rs_arr, i)};

                    dg::network_producer_consumer::delvrsrv_deliver(feeder.get(), feed_arg);
                }
            }

            auto max_consume_size() noexcept -> size_t{

                return this->max_consume_per_load;
            }

        private:

            struct InternalThruWriteFeedArgument{
                cache_id_t cache_id;
                std::expected<bool, exception_t> * rs;
            };

            struct InternalThruWriteFeedResolutor: dg::network_producer_consumer::ConsumerInterface<InternalThruWriteFeedArgument>{

                size_t * thru_incrementor;
                CacheUniqueWriteControllerInterface * dst;

                void push(std::move_iterator<InternalThruWriteFeedArgument *> data_arr, size_t sz) noexcept{

                    InternalThruWriteFeedArgument * base_data_arr = data_arr.base();

                    dg::network_stack_allocation::NoExceptAllocation<cache_id_t[]> cache_id_arr(sz);
                    dg::network_stack_allocation::NoExceptAllocation<std::expected<bool, exception_t>[]> rs_arr(sz);

                    for (size_t i = 0u; i < sz; ++i){
                        cache_id_arr[i] = base_data_arr[i].cache_id;
                    }

                    this->dst->thru(cache_id_arr.get(), sz, rs_arr.get());

                    for (size_t i = 0u; i < sz; ++i){
                        *base_data_arr[i].rs = rs_arr[i];

                        if (!rs_arr[i].has_value()){
                            continue;
                        }

                        if (!rs_arr[i].value()){
                            continue;
                        }

                        *this->thru_incrementor += 1;
                    }
                }
            };

            void internal_dispatch_switch() noexcept{

                if (this->operating_side == false){
                    this->right_controller->clear();
                } else{
                    this->left_controller->clear();
                }

                this->operating_side            = !this->operating_side;
                this->switch_population_counter = 0u;
            }
    };

    //clear
    class AdvancedInfiniteCacheUniqueWriteController: public virtual InfiniteCacheUniqueWriteControllerInterface{

        private:

            dg::cyclic_unordered_node_set<cache_id_t> cache_id_set;
            size_t max_consume_per_load;

        public:

            AdvancedInfiniteCacheUniqueWriteController(dg::cyclic_unordered_node_set<cache_id_t> cache_id_set,
                                                       size_t max_consume_per_load) noexcept: cache_id_set(std::move(cache_id_set)),
                                                                                              max_consume_per_load(max_consume_per_load){}

            void thru(cache_id_t * cache_id_arr, size_t sz, std::expected<bool, exception_t> * rs_arr) noexcept{

                if constexpr(DEBUG_MODE_FLAG){
                    if (sz > this->max_consume_size()){
                        dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                for (size_t i = 0u; i < sz; ++i){
                    auto set_ptr = this->cache_id_set.find(cache_id_arr[i]);

                    if (set_ptr != this->cache_id_set.end()){
                        rs_arr[i] = false;
                        continue;
                    }

                    auto [_, status] = this->cache_id_set.insert(cache_id_arr[i]);
                    dg::network_exception_handler::dg_assert(status);
                    rs_arr[i] = true;
                }
            }

            auto max_consume_size() noexcept -> size_t{

                return this->max_consume_per_load;
            }
    };

    //clear
    class MutexControlledInfiniteCacheWriteExclusionController: public virtual InfiniteCacheUniqueWriteControllerInterface{

        private:

            std::unique_ptr<InfiniteCacheUniqueWriteControllerInterface> base;
            std::unique_ptr<std::mutex> mtx;

        public:

            MutexControlledInfiniteCacheWriteExclusionController(std::unique_ptr<InfiniteCacheUniqueWriteControllerInterface> base,
                                                                 std::unique_ptr<std::mutex> mtx) noexcept: base(std::move(base)),
                                                                                                            mtx(std::move(mtx)){}

            void thru(cache_id_t * cache_id_arr, size_t sz, std::expected<bool, exception_t> * rs_arr) noexcept{

                stdx::xlock_guard<std::mutex> lck_grd(*this->mtx);
                this->base->thru(cache_id_arr, sz, rs_arr);
            }

            auto max_consume_size() noexcept -> size_t{

                //
                return this->base->max_consume_size();
            }
    };

    //clear
    template <class Hasher>
    class DistributedUniqueCacheWriteController: public virtual InfiniteCacheUniqueWriteControllerInterface{

        private:

            std::unique_ptr<std::unique_ptr<InfiniteCacheUniqueWriteControllerInterface>[]> base_arr;
            size_t pow2_base_arr_sz;
            size_t thru_keyvalue_feed_cap;
            size_t max_consume_per_load;
            Hasher hasher;

        public:

            DistributedUniqueCacheWriteController(std::unique_ptr<std::unique_ptr<InfiniteCacheUniqueWriteControllerInterface>[]> base_arr,
                                                  size_t pow2_base_arr_sz,
                                                  size_t thru_keyvalue_feed_cap,
                                                  size_t max_consume_per_load,
                                                  Hasher hasher) noexcept: base_arr(std::move(base_arr)),
                                                                           pow2_base_arr_sz(pow2_base_arr_sz),
                                                                           thru_keyvalue_feed_cap(thru_keyvalue_feed_cap),
                                                                           max_consume_per_load(max_consume_per_load),
                                                                           hasher(std::move(hasher)){}

            void thru(cache_id_t * cache_id_arr, size_t sz, std::expected<bool, exception_t> * rs_arr) noexcept{

                auto feed_resolutor                     = InternalThruFeedResolutor{};
                feed_resolutor.controller_arr           = this->base_arr.get();

                size_t trimmed_thru_keyvalue_feed_cap   = std::min(this->thru_keyvalue_feed_cap, sz);
                size_t feeder_allocation_cost           = dg::network_producer_consumer::delvrsrv_kv_allocation_cost(&feed_resolutor, trimmed_thru_keyvalue_feed_cap);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> feeder_mem(feeder_allocation_cost);
                auto feeder                             = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_kv_open_preallocated_raiihandle(&feed_resolutor, trimmed_thru_keyvalue_feed_cap, feeder_mem.get()));

                for (size_t i = 0u; i < sz; ++i){
                    size_t hashed_cache_id_value    = stdx::safe_integer_cast<size_t>(this->hasher(cache_id_arr[i])); 
                    size_t partitioned_idx          = hashed_cache_id_value & (this->pow2_base_arr_sz - 1u);
                    auto feed_arg                   = InternalThruFeedArgument{.cache_id    = cache_id_arr[i],
                                                                               .rs_ptr      = std::next(rs_arr, i)};

                    dg::network_producer_consumer::delvrsrv_kv_deliver(feeder.get(), partitioned_idx, feed_arg);
                }
            }

            auto max_consume_size() noexcept -> size_t{

                return this->max_consume_per_load;
            }

        private:

            struct InternalThruFeedArgument{
                cache_id_t cache_id;
                std::expected<bool, exception_t> * rs_ptr;
            };

            struct InternalThruFeedResolutor: dg::network_producer_consumer::KVConsumerInterface<size_t, InternalThruFeedArgument>{

                std::unique_ptr<InfiniteCacheUniqueWriteControllerInterface> * controller_arr;

                void push(const size_t& partitioned_idx, std::move_iterator<InternalThruFeedArgument *> data_arr, size_t sz) noexcept{

                    InternalThruFeedArgument * base_data_arr = data_arr.base();

                    dg::network_stack_allocation::NoExceptAllocation<cache_id_t[]> cache_id_arr(sz);
                    dg::network_stack_allocation::NoExceptAllocation<std::expected<bool, exception_t>[]> rs_arr(sz);

                    for (size_t i = 0u; i < sz; ++i){
                        cache_id_arr[i] = base_data_arr[i].cache_id;
                    }

                    this->controller_arr[partitioned_idx]->thru(cache_id_arr.get(), sz, rs_arr.get());

                    for (size_t i = 0u; i < sz; ++i){
                        *base_data_arr[i].rs_ptr = rs_arr[i];
                    }
                }
            };
    };

    struct AtomicBlock{
        uint32_t thru_counter;
        uint32_t ver_ctrl; //this pattern is called version control update, thru_counter could be a pointer to a new const_read block state, this pattern is equivalent to mutual exclusion stdx::lock_guard<std::mutex> inout transaction, in the sense that every method invoke only is only allowed to hold the mutual exclusion if they read the right ticket number (there is no guy calling at the time ...)

        //assume we are the unique ver_ctrl, nothing happened
        //assume we are sharing the ver_ctrl, the first guy gets the right to update, forfeit all other guys result (as if the other guys never invoke one of the methods by updating the version control, which bring this to we are the unique_ver_ctrl, nothing happened)
        //                                                                                                           proof by contradiction, assume other guy affect the internal state, they must got the ver_ctrl incrementation 

        //we postpone the atomicity of transaction -> compare_exchange_strong
        //and the state propagation -> ver_ctrl
        
        //another pattern is called state propagation by using cmpexch, such is logically harder to program, we bring the state from a random correct state -> another random correct state 
        //imagine malloc + free for example, we dont know if the old -> new value is unique, a decade could happen when we changing the head of the linked list, the comparing result is no longer relevant 

        template <class Reflector>
        constexpr void dg_reflect(const Reflector& reflector) const noexcept{
            reflector(thru_counter, ver_ctrl);
        }

        template <class Reflector>
        constexpr void dg_reflect(const Reflector& reflector) noexcept{
            reflector(thru_counter, ver_ctrl);
        }
    };

    using atomic_block_pragma_0_t = std::array<char, sizeof(uint32_t) + sizeof(uint32_t)>;

    //clear
    class CacheUniqueWriteTrafficController: public virtual CacheUniqueWriteTrafficControllerInterface{

        private:

            stdx::inplace_hdi_container<std::atomic<atomic_block_pragma_0_t>> block_ctrl;
            stdx::hdi_container<size_t> thru_cap;

            static constexpr auto make_pragma_0_block(AtomicBlock arg) noexcept -> atomic_block_pragma_0_t{

                constexpr size_t TRIVIAL_SZ = dg::network_trivial_serializer::size(AtomicBlock{});
                static_assert(TRIVIAL_SZ <= atomic_block_pragma_0_t{}.size());

                auto rs = atomic_block_pragma_0_t{};
                dg::network_trivial_serializer::serialize_into(rs.data(), arg);

                return rs;
            }

            static constexpr auto read_pragma_0_block(atomic_block_pragma_0_t arg) noexcept -> AtomicBlock{

                auto rs = AtomicBlock{};
                dg::network_trivial_serializer::deserialize_into(rs, arg.data());

                return rs;
            }

        public:

            using self = CacheUniqueWriteTrafficController;

            CacheUniqueWriteTrafficController(size_t thru_cap) noexcept: block_ctrl(std::in_place_t{}, self::make_pragma_0_block(AtomicBlock{0u, 0u})),
                                                                         thru_cap(stdx::hdi_container<size_t>{thru_cap}){}

            auto thru(size_t incoming_sz) noexcept -> std::expected<bool, exception_t>{

                //we rather busy wait
                //we'll attempt to uniform distribute this further to get a representation of the traffic, instead of an exact correct answer
                //a fetch add seems like a correct answer yet a very dangerous undefined logic
                //we are very tempted to do fetch add, really, yet it is statistically accurate, not logically accurate

                std::expected<bool, exception_t> rs = {}; 

                auto busy_wait_task = [&, this]() noexcept{
                    atomic_block_pragma_0_t then_block  = this->block_ctrl.value.load(std::memory_order_relaxed);
                    AtomicBlock then_semantic_block     = self::read_pragma_0_block(then_block);

                    if (then_semantic_block.thru_counter + incoming_sz > this->thru_cap.value){
                        rs = false;
                        return true;
                    }

                    AtomicBlock now_semantic_block      = AtomicBlock{.thru_counter = then_semantic_block.thru_counter + incoming_sz,
                                                                      .ver_ctrl     = then_semantic_block.ver_ctrl + 1u};

                    atomic_block_pragma_0_t now_block   = self::make_pragma_0_block(now_semantic_block);

                    bool was_updated                    = this->block_ctrl.value.compare_exchange_strong(then_block, now_block, std::memory_order_relaxed);

                    if (was_updated){
                        rs = true;
                        return true;
                    }

                    return false;
                };

                stdx::busy_wait(busy_wait_task);
                return rs;
            }

            auto thru_size() const noexcept -> size_t{

                atomic_block_pragma_0_t blk = this->block_ctrl.value.load(std::memory_order_relaxed);
                AtomicBlock semantic_blk    = self::read_pragma_0_block(blk);

                return semantic_blk.thru_counter;
            }

            auto thru_capacity() const noexcept -> size_t{

                return this->thru_cap.value;
            }

            void reset() noexcept{

                auto busy_wait_task = [&, this]() noexcept{
                    atomic_block_pragma_0_t then_block  = this->block_ctrl.value.load(std::memory_order_relaxed);
                    AtomicBlock then_semantic_block     = self::read_pragma_0_block(then_block); 
                    AtomicBlock now_semantic_block      = AtomicBlock{.thru_counter = 0u,
                                                                      .ver_ctrl     = then_semantic_block.ver_ctrl + 1u};
                    atomic_block_pragma_0_t now_block   = self::make_pragma_0_block(now_semantic_block);

                    return this->block_ctrl.value.compare_exchange_strong(then_block, now_block, std::memory_order_relaxed);
                };

                stdx::busy_wait(busy_wait_task);
            }
    };

    //clear
    class DistributedCacheUniqueWriteTrafficController: public virtual DistributedCacheUniqueWriteTrafficControllerInterface{

        private:

            std::unique_ptr<std::unique_ptr<CacheUniqueWriteTrafficControllerInterface>[]> base_arr;
            size_t pow2_base_arr_sz;
            size_t max_thru_sz;

        public:

            DistributedCacheUniqueWriteTrafficController(std::unique_ptr<std::unique_ptr<CacheUniqueWriteTrafficControllerInterface>[]> base_arr,
                                                         size_t pow2_base_arr_sz,
                                                         size_t max_thru_sz) noexcept: base_arr(std::move(base_arr)),
                                                                                       pow2_base_arr_sz(pow2_base_arr_sz),
                                                                                       max_thru_sz(max_thru_sz){}

            auto thru(size_t incoming_sz) noexcept -> std::expected<bool, exception_t>{

                //why arent we using a statistical thru (1 thru out of 40), we are susceptible to leaks of incoming_sz

                if (incoming_sz > this->max_thru_size()){
                    return std::unexpected(dg::network_exception::INVALID_ARGUMENT);
                }

                size_t random_clue  = dg::network_randomizer::randomize_int<size_t>();
                size_t base_arr_idx = random_clue & (this->pow2_base_arr_sz - 1u);

                return this->base_arr[base_arr_idx]->thru(incoming_sz);
            }

            void reset() noexcept{

                for (size_t i = 0u; i < this->pow2_base_arr_sz; ++i){
                    this->base_arr[i]->reset();
                }
            }

            auto max_thru_size() const noexcept -> size_t{

                return this->max_thru_sz;
            }
    };

    //clear
    class SubscriptibleWrappedResetTrafficController: public virtual UpdatableInterface{

        private:

            stdx::hdi_container<std::chrono::nanoseconds> update_dur;
            stdx::inplace_hdi_container<std::atomic<std::chrono::time_point<std::chrono::steady_clock>>> last_updated;
            std::shared_ptr<CacheUniqueWriteTrafficControllerInterface> updating_component;

        public:

            SubscriptibleWrappedResetTrafficController(std::chrono::nanoseconds update_dur,
                                                       std::chrono::time_point<std::chrono::steady_clock> last_updated,
                                                       std::shared_ptr<CacheUniqueWriteTrafficControllerInterface> updating_component) noexcept: update_dur(stdx::hdi_container<std::chrono::nanoseconds>{update_dur}),
                                                                                                                                                 last_updated(std::in_place_t{}, last_updated),
                                                                                                                                                 updating_component(std::move(updating_component)){}

            void update() noexcept{

                //attempt to do atomic_cmpexch to take unique update responsibility, clock always goes forward in time

                std::chrono::time_point<std::chrono::steady_clock> last_updated_value   = this->last_updated.value.load(std::memory_order_relaxed);
                std::chrono::time_point<std::chrono::steady_clock> now                  = std::chrono::steady_clock::now();
                std::chrono::nanoseconds diff                                           = std::chrono::duration_cast<std::chrono::nanoseconds>(now - last_updated_value);

                if (diff < this->update_dur.value){
                    return;
                }

                bool has_mtx_ticket = this->last_updated.value.compare_exchange_strong(last_updated_value, now, std::memory_order_relaxed);

                if (!has_mtx_ticket){
                    return;
                }

                this->updating_component->reset(); //thru
            }

    };

    //clear
    class UpdateWorker: public virtual dg::network_concurrency::WorkerInterface{

        private:

            std::shared_ptr<UpdatableInterface> updatable;
            std::chrono::nanoseconds heartbeat_dur;

        public:

            UpdateWorker(std::shared_ptr<UpdatableInterface> updatable,
                         std::chrono::nanoseconds heartbeat_dur) noexcept: updatable(std::move(updatable)),
                                                                           heartbeat_dur(heartbeat_dur){}

            auto run_one_epoch() noexcept -> bool{

                this->updatable->update();
                std::this_thread::sleep_for(this->heartbeat_dur);

                return true;
            }
    };

    //clear
    class RequestResolverWorker: public virtual dg::network_concurrency::WorkerInterface{

        private:

            dg::unordered_unstable_map<dg::string, std::unique_ptr<RequestHandlerInterface>> request_handler_map;
            std::shared_ptr<InfiniteCacheControllerInterface> request_cache_controller;
            std::shared_ptr<InfiniteCacheUniqueWriteControllerInterface> cachewrite_uex_controller;
            std::shared_ptr<DistributedCacheUniqueWriteTrafficControllerInterface> cachewrite_traffic_controller; 
            uint32_t resolve_channel;
            size_t resolve_consume_sz;
            uint32_t response_channel;
            size_t mailbox_feed_cap;
            size_t mailbox_prep_feed_cap;
            size_t request_handler_feed_cap;
            size_t cache_controller_feed_cap;
            size_t server_fetch_feed_cap;
            size_t cache_server_fetch_feed_cap;
            size_t cache_fetch_feed_cap;
            size_t busy_consume_sz;

        public:

            RequestResolverWorker(dg::unordered_unstable_map<dg::string, std::unique_ptr<RequestHandlerInterface>> request_handler_map,
                                  std::shared_ptr<InfiniteCacheControllerInterface> request_cache_controller,
                                  std::shared_ptr<InfiniteCacheUniqueWriteControllerInterface> cachewrite_uex_controller,
                                  std::shared_ptr<DistributedCacheUniqueWriteTrafficControllerInterface> cachewrite_traffic_controller,
                                  uint32_t resolve_channel,
                                  size_t resolve_consume_sz,
                                  uint32_t response_channel,
                                  size_t mailbox_feed_cap,
                                  size_t mailbox_prep_feed_cap,
                                  size_t request_handler_feed_cap,
                                  size_t cache_controller_feed_cap,
                                  size_t server_fetch_feed_cap,
                                  size_t cache_server_fetch_feed_cap,
                                  size_t cache_fetch_feed_cap,
                                  size_t busy_consume_sz) noexcept: request_handler_map(std::move(request_handler_map)),
                                                                    request_cache_controller(std::move(request_cache_controller)),
                                                                    cachewrite_uex_controller(std::move(cachewrite_uex_controller)),
                                                                    cachewrite_traffic_controller(std::move(cachewrite_traffic_controller)),
                                                                    resolve_channel(resolve_channel),
                                                                    resolve_consume_sz(resolve_consume_sz),
                                                                    response_channel(response_channel),
                                                                    mailbox_feed_cap(mailbox_feed_cap),
                                                                    mailbox_prep_feed_cap(mailbox_prep_feed_cap),
                                                                    request_handler_feed_cap(request_handler_feed_cap),
                                                                    cache_controller_feed_cap(cache_controller_feed_cap),
                                                                    server_fetch_feed_cap(server_fetch_feed_cap),
                                                                    cache_server_fetch_feed_cap(cache_server_fetch_feed_cap),
                                                                    cache_fetch_feed_cap(cache_fetch_feed_cap),
                                                                    busy_consume_sz(busy_consume_sz){}

            bool run_one_epoch() noexcept{

                size_t recv_buf_cap = this->resolve_consume_sz;
                size_t recv_buf_sz  = {};
                dg::network_stack_allocation::NoExceptAllocation<dg::string[]> recv_buf_arr(recv_buf_cap);
                dg::network_kernel_mailbox::recv(this->resolve_channel, recv_buf_arr.get(), recv_buf_sz, recv_buf_cap);

                auto mailbox_feed_resolutor                                 = InternalResponseFeedResolutor{}; 
                mailbox_feed_resolutor.mailbox_channel                      = this->response_channel;

                size_t trimmed_mailbox_feed_cap                             = std::min(std::min(this->mailbox_feed_cap, dg::network_kernel_mailbox::max_consume_size()), recv_buf_sz);
                size_t mailbox_feeder_allocation_cost                       = dg::network_producer_consumer::delvrsrv_allocation_cost(&mailbox_feed_resolutor, trimmed_mailbox_feed_cap);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> mailbox_feeder_mem(mailbox_feeder_allocation_cost);
                auto mailbox_feeder                                         = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_open_preallocated_raiihandle(&mailbox_feed_resolutor, trimmed_mailbox_feed_cap, mailbox_feeder_mem.get())); 

                //---

                auto mailbox_prep_feed_resolutor                            = InternalMailBoxPrepFeedResolutor{};
                mailbox_prep_feed_resolutor.mailbox_feeder                  = mailbox_feeder.get();

                size_t trimmed_mailbox_prep_feed_cap                        = std::min(this->mailbox_prep_feed_cap, recv_buf_sz);
                size_t mailbox_prep_feeder_allocation_cost                  = dg::network_producer_consumer::delvrsrv_allocation_cost(&mailbox_prep_feed_resolutor, trimmed_mailbox_prep_feed_cap);
                dg::network_stack_allocation::NoExceptAllocation<char[]> mailbox_prep_feeder_mem(mailbox_prep_feeder_allocation_cost);
                auto mailbox_prep_feeder                                    = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_open_preallocated_raiihandle(&mailbox_prep_feed_resolutor, trimmed_mailbox_prep_feed_cap, mailbox_prep_feeder_mem.get())); 

                //---

                auto cache_map_insert_feed_resolutor                        = InternalCacheMapFeedResolutor{}; 
                cache_map_insert_feed_resolutor.cache_controller            = this->request_cache_controller.get();

                size_t trimmed_cache_map_insert_feed_cap                    = std::min(std::min(this->cache_controller_feed_cap, this->request_cache_controller->max_consume_size()), recv_buf_sz); 
                size_t cache_map_insert_feeder_allocation_cost              = dg::network_producer_consumer::delvrsrv_allocation_cost(&cache_map_insert_feed_resolutor, trimmed_cache_map_insert_feed_cap);
                dg::network_stack_allocation::NoExceptAllocation<char[]> cache_map_insert_feeder_mem(cache_map_insert_feeder_allocation_cost);
                auto cache_map_insert_feeder                                = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_open_preallocated_raiihandle(&cache_map_insert_feed_resolutor, trimmed_cache_map_insert_feed_cap, cache_map_insert_feeder_mem.get())); 

                //---

                auto server_fetch_feed_resolutor                            = InternalServerFeedResolutor{};
                server_fetch_feed_resolutor.request_handler_map             = &this->request_handler_map;
                server_fetch_feed_resolutor.mailbox_prep_feeder             = mailbox_prep_feeder.get();
                server_fetch_feed_resolutor.cache_map_feeder                = cache_map_insert_feeder.get();

                size_t trimmed_server_fetch_feed_cap                        = std::min(this->server_fetch_feed_cap, recv_buf_sz);
                size_t server_feeder_allocation_cost                        = dg::network_producer_consumer::delvrsrv_kv_allocation_cost(&server_fetch_feed_resolutor, trimmed_server_fetch_feed_cap);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> server_feeder_mem(server_feeder_allocation_cost);
                auto server_feeder                                          = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_kv_open_preallocated_raiihandle(&server_fetch_feed_resolutor, trimmed_server_fetch_feed_cap, server_feeder_mem.get()));

                //---

                auto cache_server_fetch_feed_resolutor                      = InternalCacheServerFeedResolutor{}; 
                cache_server_fetch_feed_resolutor.cachewrite_uex_controller = this->cachewrite_uex_controller.get();
                cache_server_fetch_feed_resolutor.cachewrite_tfx_controller = this->cachewrite_traffic_controller.get();
                cache_server_fetch_feed_resolutor.server_feeder             = server_feeder.get();
                cache_server_fetch_feed_resolutor.mailbox_prep_feeder       = mailbox_prep_feeder.get();

                size_t trimmed_cache_server_fetch_feed_cap                  = std::min(std::min(std::min(this->cache_server_fetch_feed_cap, this->cachewrite_uex_controller->max_consume_size()), this->cachewrite_traffic_controller->max_thru_size()), recv_buf_sz);
                size_t cache_server_feeder_allocation_cost                  = dg::network_producer_consumer::delvrsrv_allocation_cost(&cache_server_fetch_feed_resolutor, trimmed_cache_server_fetch_feed_cap);
                dg::network_stack_allocation::NoExceptAllocation<char[]> cache_server_feeder_mem(cache_server_feeder_allocation_cost);
                auto cache_server_feeder                                    = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_open_preallocated_raiihandle(&cache_server_fetch_feed_resolutor, trimmed_cache_server_fetch_feed_cap, cache_server_feeder_mem.get())); 

                //---

                auto cache_fetch_feed_resolutor                             = InternalCacheFeedResolutor{};
                cache_fetch_feed_resolutor.cache_server_feeder              = cache_server_feeder.get();
                cache_fetch_feed_resolutor.mailbox_prep_feeder              = mailbox_prep_feeder.get();
                cache_fetch_feed_resolutor.cache_controller                 = this->request_cache_controller.get();

                size_t trimmed_cache_fetch_feed_cap                         = std::min(this->cache_fetch_feed_cap, recv_buf_sz);
                size_t cache_fetch_feeder_allocation_cost                   = dg::network_producer_consumer::delvrsrv_allocation_cost(&cache_fetch_feed_resolutor, trimmed_cache_fetch_feed_cap);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> cache_fetch_feeder_mem(cache_fetch_feeder_allocation_cost);
                auto cache_fetch_feeder                                     = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_open_preallocated_raiihandle(&cache_fetch_feed_resolutor, trimmed_cache_fetch_feed_cap, cache_fetch_feeder_mem.get()));

                for (size_t i = 0u; i < recv_buf_sz; ++i){
                    std::expected<model::InternalRequest, exception_t> request = dg::network_exception::to_cstyle_function(dg::network_compact_serializer::integrity_deserialize<model::InternalRequest, dg::string>)(recv_buf_arr[i], model::INTERNAL_REQUEST_SERIALIZATION_SECRET); 

                    if (!request.has_value()){
                        dg::network_log_stackdump::error_fast(dg::network_exception::verbose(request.error()));
                        continue;
                    }

                    std::expected<dg::network_kernel_mailbox::Address, exception_t> requestor_addr = dg::network_rest_frame::utility::deserialize_address(request->request.requestor);

                    if (!requestor_addr.has_value()){
                        dg::network_log_stackdump::error_fast(dg::network_exception::verbose(requestor_addr.error()));
                        continue;
                    }

                    auto now = std::chrono::utc_clock::now();

                    //alright, I admit this is hard to write, the feed's gonna drive some foos crazy

                    if (request->server_abs_timeout.has_value() && request->server_abs_timeout.value() <= now){
                        auto response   = model::InternalResponse{.response     = std::unexpected(dg::network_exception::REST_SERVERSIDE_ABSTIMEOUT_TIMEOUT), 
                                                                  .ticket_id    = request->ticket_id};

                        auto prep_arg   = InternalMailBoxPrepFeedArgument{.to               = requestor_addr.value(),
                                                                          .response         = std::move(response)}; 

                        dg::network_producer_consumer::delvrsrv_deliver(mailbox_prep_feeder.get(), std::move(prep_arg));
                        continue;
                    }

                    std::expected<ResourceAddress, exception_t> resource_path = dg::network_rest_frame::utility::deserialize_resource_address(request->request.requestee_uri);

                    if (!resource_path.has_value()){
                        auto response   = model::InternalResponse{.response     = std::unexpected(resource_path.error()), 
                                                                  .ticket_id    = request->ticket_id};

                        auto prep_arg   = InternalMailBoxPrepFeedArgument{.to               = requestor_addr.value(),
                                                                          .response         = std::move(response)};

                        dg::network_producer_consumer::delvrsrv_deliver(mailbox_prep_feeder.get(), std::move(prep_arg));
                        continue;
                    }

                    auto map_ptr = this->request_handler_map.find(resource_path.value().resource_addr);

                    if (map_ptr == this->request_handler_map.end()){
                        auto response   = model::InternalResponse{.response   = std::unexpected(dg::network_exception::REST_INVALID_URI), 
                                                                  .ticket_id  = request->ticket_id};

                        auto prep_arg   = InternalMailBoxPrepFeedArgument{.to               = requestor_addr.value(),
                                                                          .response         = std::move(response)};

                        dg::network_producer_consumer::delvrsrv_deliver(mailbox_prep_feeder.get(), std::move(prep_arg));
                        continue;
                    }

                    if (request->has_unique_response){
                        if (!request->client_request_cache_id.has_value()){
                            auto response   = model::InternalResponse{.response     = std::unexpected(dg::network_exception::REST_INVALID_ARGUMENT),
                                                                      .ticket_id    = request->ticket_id};

                            auto prep_arg   = InternalMailBoxPrepFeedArgument{.to               = requestor_addr.value(),
                                                                              .response         = std::move(response)};

                            dg::network_producer_consumer::delvrsrv_deliver(mailbox_prep_feeder.get(), std::move(prep_arg)); 
                            continue;
                        }

                        auto arg   = InternalCacheFeedArgument{.to              = requestor_addr.value(),
                                                               .local_uri_path  = map_ptr->first,
                                                               .cache_id        = request->client_request_cache_id.value(),
                                                               .ticket_id       = request->ticket_id,
                                                               .request         = std::move(request->request)};

                        dg::network_producer_consumer::delvrsrv_deliver(cache_fetch_feeder.get(), std::move(arg));                        
                        continue;
                    }

                    auto key_arg        = dg::string(map_ptr->first);
                    auto value_arg      = InternalServerFeedResolutorArgument{.to               = requestor_addr.value(),
                                                                              .cache_write_id   = std::nullopt,
                                                                              .ticket_id        = request->ticket_id,
                                                                              .request          = std::move(request->request)};

                    dg::network_producer_consumer::delvrsrv_kv_deliver(server_feeder.get(), key_arg, std::move(value_arg));
                }

                return recv_buf_sz >= this->busy_consume_sz;
            }
        
        private:

            struct InternalResponseFeedArgument{
                MailBoxArgument mailbox_arg;
                exception_t * exception_ptr;
            };

            struct InternalResponseFeedResolutor: dg::network_producer_consumer::ConsumerInterface<InternalResponseFeedArgument>{

                uint32_t mailbox_channel;

                void push(std::move_iterator<InternalResponseFeedArgument *> data_arr, size_t sz) noexcept{

                    InternalResponseFeedArgument * base_data_arr = data_arr.base();

                    dg::network_stack_allocation::NoExceptAllocation<exception_t[]> exception_arr(sz);
                    dg::network_stack_allocation::NoExceptAllocation<MailBoxArgument[]> mailbox_arr(sz);

                    for (size_t i = 0u; i < sz; ++i){
                        mailbox_arr[i] = std::move(base_data_arr[i].mailbox_arg);
                    }

                    dg::network_kernel_mailbox::send(this->mailbox_channel,
                                                     std::make_move_iterator(mailbox_arr.get()), sz,
                                                     exception_arr.get());

                    for (size_t i = 0u; i < sz; ++i){
                        *base_data_arr[i].exception_ptr = exception_arr[i];
                    }
                }
            };

            struct InternalMailBoxPrepFeedArgument{
                dg::network_kernel_mailbox::Address to;
                InternalResponse response;
            };

            struct InternalMailBoxPrepFeedResolutor: dg::network_producer_consumer::ConsumerInterface<InternalMailBoxPrepFeedArgument>{

                dg::network_producer_consumer::DeliveryHandle<InternalResponseFeedArgument> * mailbox_feeder;

                void push(std::move_iterator<InternalMailBoxPrepFeedArgument *> data_arr, size_t sz) noexcept{

                    InternalMailBoxPrepFeedArgument * base_data_arr = data_arr.base();

                    dg::network_stack_allocation::NoExceptAllocation<exception_t[]> exception_arr(sz);
                    dg::network_stack_allocation::NoExceptAllocation<exception_t[]> reexception_arr(sz);

                    for (size_t i = 0u; i < sz; ++i){
                        std::expected<dg::string, exception_t> serialized_response = dg::network_exception::to_cstyle_function(dg::network_compact_serializer::dgstd_serialize<dg::string, InternalResponse>)(base_data_arr[i].response, model::INTERNAL_RESPONSE_SERIALIZATION_SECRET);

                        if (!serialized_response.has_value()){
                            exception_arr[i] = serialized_response.error(); 
                            continue;
                        }

                        auto mailbox_arg        = dg::network_kernel_mailbox::MailBoxArgument{.to       = base_data_arr[i].to,
                                                                                              .content  = std::move(serialized_response.value())};

                        auto response_feed_arg  = InternalResponseFeedArgument{.mailbox_arg     = std::move(mailbox_arg),
                                                                               .exception_ptr   = std::next(exception_arr.get(), i)};

                        dg::network_producer_consumer::delvrsrv_deliver(this->mailbox_feeder, std::move(response_feed_arg));
                    }

                    dg::network_producer_consumer::delvrsrv_clear(this->mailbox_feeder);

                    for (size_t i = 0u; i < sz; ++i){
                        if (dg::network_exception::is_failed(exception_arr[i])){
                            auto response = InternalResponse{.response  = std::unexpected(exception_arr[i]),
                                                             .ticket_id = base_data_arr[i].response.ticket_id};

                            std::expected<dg::string, exception_t> serialized_response = dg::network_exception::to_cstyle_function(dg::network_compact_serializer::dgstd_serialize<dg::string, InternalResponse>)(response, model::INTERNAL_RESPONSE_SERIALIZATION_SECRET);

                            if (!serialized_response.has_value()){
                                reexception_arr[i] = serialized_response.error();
                                continue;
                            }

                            auto mailbox_arg        = dg::network_kernel_mailbox::MailBoxArgument{.to       = base_data_arr[i].to,
                                                                                                  .content  = std::move(serialized_response.value())};

                            auto response_feed_arg  = InternalResponseFeedArgument{.mailbox_arg     = std::move(mailbox_arg),
                                                                                   .exception_ptr   = std::next(reexception_arr.get(), i)};

                            dg::network_producer_consumer::delvrsrv_deliver(this->mailbox_feeder, std::move(response_feed_arg));
                        } else{
                            reexception_arr[i] = dg::network_exception::SUCCESS; 
                        }
                    }

                    dg::network_producer_consumer::delvrsrv_clear(this->mailbox_feeder);

                    //we need to log the reexceptions...

                    for (size_t i = 0u; i < sz; ++i){
                        if (dg::network_exception::is_failed(reexception_arr[i])){
                            dg::network_log_stackdump::error_fast_optional(dg::network_exception::verbose(reexception_arr[i]));
                        }
                    }
                }
            };

            struct InternalCacheMapFeedArgument{
                cache_id_t cache_id;
                Response response;
            };

            struct InternalCacheMapFeedResolutor: dg::network_producer_consumer::ConsumerInterface<InternalCacheMapFeedArgument>{

                InfiniteCacheControllerInterface * cache_controller;

                void push(std::move_iterator<InternalCacheMapFeedArgument *> data_arr, size_t sz) noexcept{

                    auto base_data_arr = data_arr.base();

                    dg::network_stack_allocation::NoExceptAllocation<cache_id_t[]> cache_id_arr(sz);
                    dg::network_stack_allocation::NoExceptAllocation<Response[]> response_arr(sz);
                    dg::network_stack_allocation::NoExceptAllocation<std::expected<bool, exception_t>[]> rs_arr(sz);

                    for (size_t i = 0u; i < sz; ++i){
                        cache_id_arr[i] = base_data_arr[i].cache_id;
                        response_arr[i] = std::move(base_data_arr[i].response);
                    }

                    this->cache_controller->insert_cache(cache_id_arr.get(), std::make_move_iterator(response_arr.get()), sz, rs_arr.get());

                    for (size_t i = 0u; i < sz; ++i){
                        if (!rs_arr[i].has_value()){
                            dg::network_log_stackdump::error_fast_optional(dg::network_exception::verbose(rs_arr[i].error()));
                            continue;
                        }

                        if (!rs_arr[i].value()){
                            dg::network_log_stackdump::error_fast_optional("REST_CACHEMAP BAD INSERT");
                            continue;
                        }
                    }
                }
            };

            struct InternalServerFeedResolutorArgument{
                dg::network_kernel_mailbox::Address to;
                std::optional<cache_id_t> cache_write_id;
                ticket_id_t ticket_id;
                Request request;
            };

            struct InternalServerFeedResolutor: dg::network_producer_consumer::KVConsumerInterface<dg::string, InternalServerFeedResolutorArgument>{

                dg::unordered_unstable_map<dg::string, std::unique_ptr<RequestHandlerInterface>> * request_handler_map;
                dg::network_producer_consumer::DeliveryHandle<InternalMailBoxPrepFeedArgument> * mailbox_prep_feeder;
                dg::network_producer_consumer::DeliveryHandle<InternalCacheMapFeedArgument> * cache_map_feeder;

                void push(const dg::string& local_uri_path, std::move_iterator<InternalServerFeedResolutorArgument *> data_arr, size_t sz) noexcept{

                    auto base_data_arr = data_arr.base();
                    dg::network_stack_allocation::NoExceptAllocation<Request[]> request_arr(sz);
                    dg::network_stack_allocation::NoExceptAllocation<Response[]> response_arr(sz);

                    for (size_t i = 0u; i < sz; ++i){
                        request_arr[i] = std::move(base_data_arr[i].request);
                    }

                    auto map_ptr = this->request_handler_map->find(local_uri_path);

                    if constexpr(DEBUG_MODE_FLAG){
                        if (map_ptr == this->request_handler_map->end()){
                            dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                            std::abort();
                        }
                    }

                    //get the responses for the requests, this cannot fail, so there is no std::expected<Response, exception_t>
                    //a fail of this request could denote a unique_write good resource leak, not bad leak
                    //the contract of worst case == that of one normal request signal remains 

                    map_ptr->second->handle(std::make_move_iterator(request_arr.get()), sz, response_arr.get());

                    for (size_t i = 0u; i < sz; ++i){

                        //attempt to write the cache if there is a designated cache_id, if cache_write is failed, it is a silent fail
                        //we are still honoring the contract of all fails == one normal request signal fail 

                        if (base_data_arr[i].cache_write_id.has_value()){
                            std::expected<Response, exception_t> cpy_response_arr = dg::network_exception::cstyle_initialize<Response>(response_arr[i]);

                            if (!cpy_response_arr.has_value()){
                                dg::network_log_stackdump::error_fast_optional(dg::network_exception::verbose(cpy_response_arr.error()));
                            } else{
                                auto cache_mapfeed_arg = InternalCacheMapFeedArgument{.cache_id = base_data_arr[i].cache_write_id.value(),
                                                                                      .response = std::move(cpy_response_arr.value())}; 

                                dg::network_producer_consumer::delvrsrv_deliver(this->cache_map_feeder, std::move(cache_mapfeed_arg));
                            }
                        }

                        //returns the result to the user

                        auto response   = model::InternalResponse{.response   = std::move(response_arr[i]),
                                                                  .ticket_id  = base_data_arr[i].ticket_id}; 

                        auto prep_arg   = InternalMailBoxPrepFeedArgument{.to               = base_data_arr[i].to,
                                                                          .response         = std::move(response)};

                        dg::network_producer_consumer::delvrsrv_deliver(this->mailbox_prep_feeder, std::move(prep_arg));
                    }
                } 
            };

            struct InternalCacheServerFeedArgument{
                dg::network_kernel_mailbox::Address to;
                dg::string local_uri_path;
                cache_id_t cache_id;
                ticket_id_t ticket_id;
                Request request;
            };

            struct InternalCacheServerFeedResolutor: dg::network_producer_consumer::ConsumerInterface<InternalCacheServerFeedArgument>{

                InfiniteCacheUniqueWriteControllerInterface * cachewrite_uex_controller;
                CacheUniqueWriteTrafficControllerInterface * cachewrite_tfx_controller;
                dg::network_producer_consumer::KVDeliveryHandle<dg::string, InternalServerFeedResolutorArgument> * server_feeder;
                dg::network_producer_consumer::DeliveryHandle<InternalMailBoxPrepFeedArgument> * mailbox_prep_feeder;

                void push(std::move_iterator<InternalCacheServerFeedArgument *> data_arr, size_t sz) noexcept{

                    auto base_data_arr = data_arr.base();

                    dg::network_stack_allocation::NoExceptAllocation<cache_id_t[]> cache_id_arr(sz);
                    dg::network_stack_allocation::NoExceptAllocation<std::expected<bool, exception_t>[]> cache_write_response_arr(sz);

                    for (size_t i = 0u; i < sz; ++i){
                        cache_id_arr[i] = base_data_arr[i].cache_id;
                    }

                    std::expected<bool, exception_t> thru_naive_status = this->cachewrite_tfx_controller->thru(sz); //if it is already uex_controller->thru, we got a leak, its interval leak, the crack between the read_cache and the thru, if it is not thru, then we are logically correct 
                    exception_t thru_status = {}; 

                    if (!thru_naive_status.has_value()){
                        thru_status = thru_naive_status.error();
                    } else{
                        if (!thru_naive_status.value()){
                            thru_status = dg::network_exception::REST_CACHE_POPULATION_LIMIT_REACHED;
                        }
                    }

                    //not thru, returns bad signal

                    if (dg::network_exception::is_failed(thru_status)){
                        for (size_t i = 0u; i < sz; ++i){
                            auto response = model::InternalResponse{.response   = std::unexpected(thru_status),
                                                                    .ticket_id  = base_data_arr[i].ticket_id};

                            auto prep_arg   = InternalMailBoxPrepFeedArgument{.to               = base_data_arr[i].to,
                                                                              .response         = std::move(response)};

                            dg::network_producer_consumer::delvrsrv_deliver(this->mailbox_prep_feeder, std::move(prep_arg));
                        }

                        return;
                    } 

                    //thru, attempts to get the unique write 

                    this->cachewrite_uex_controller->thru(cache_id_arr.get(), sz, cache_write_response_arr.get());

                    for (size_t i = 0u; i < sz; ++i){
                        //internal server error, can't read the thruness
                        //we need to somewhat generalize our external error interface ... this is too confusing even for me

                        if (!cache_write_response_arr[i].has_value()){
                            auto response = model::InternalResponse{.response   = std::unexpected(dg::network_exception::REST_INTERNAL_SERVER_ERROR),
                                                                    .ticket_id  = base_data_arr[i].ticket_id};

                            auto prep_arg   = InternalMailBoxPrepFeedArgument{.to               = base_data_arr[i].to,
                                                                              .response         = std::move(response)};

                            dg::network_producer_consumer::delvrsrv_deliver(this->mailbox_prep_feeder, std::move(prep_arg));
                            continue;
                        }

                        //somebody else gets the cache_write, we aren't unique...

                        if (!cache_write_response_arr[i].value()){
                            auto response   = model::InternalResponse{.response   = std::unexpected(dg::network_exception::REST_BAD_CACHE_UNIQUE_WRITE),
                                                                      .ticket_id  = base_data_arr[i].ticket_id};

                            auto prep_arg   = InternalMailBoxPrepFeedArgument{.to               = base_data_arr[i].to,
                                                                              .response         = std::move(response)};

                            dg::network_producer_consumer::delvrsrv_deliver(this->mailbox_prep_feeder, std::move(prep_arg));
                            continue;
                        }

                        //we have the cache_write right, we'd make sure to dispatch this to the server feed resolutor to get a response and cache the response

                        auto arg    = InternalServerFeedResolutorArgument{.to               = base_data_arr[i].to,
                                                                          .cache_write_id   = base_data_arr[i].cache_id,
                                                                          .ticket_id        = base_data_arr[i].ticket_id,
                                                                          .request          = std::move(base_data_arr[i].request)};

                        dg::network_producer_consumer::delvrsrv_kv_deliver(this->server_feeder, base_data_arr[i].local_uri_path, std::move(arg));
                    }
                }
            };

            struct InternalCacheFeedArgument{
                dg::network_kernel_mailbox::Address to;
                dg::string local_uri_path;
                cache_id_t cache_id;
                ticket_id_t ticket_id;
                Request request;
            };

            struct InternalCacheFeedResolutor: dg::network_producer_consumer::ConsumerInterface<InternalCacheFeedArgument>{

                dg::network_producer_consumer::DeliveryHandle<InternalCacheServerFeedArgument> * cache_server_feeder;
                dg::network_producer_consumer::DeliveryHandle<InternalMailBoxPrepFeedArgument> * mailbox_prep_feeder; 
                InfiniteCacheControllerInterface * cache_controller;

                void push(std::move_iterator<InternalCacheFeedArgument *> data_arr, size_t sz) noexcept{

                    auto base_data_arr = data_arr.base();

                    dg::network_stack_allocation::NoExceptAllocation<cache_id_t[]> cache_id_arr(sz);
                    dg::network_stack_allocation::NoExceptAllocation<std::expected<std::optional<Response>, exception_t>[]> response_arr(sz);

                    for (size_t i = 0u; i < sz; ++i){
                        cache_id_arr[i] = base_data_arr[i].cache_id;                        
                    }

                    this->cache_controller->get_cache(cache_id_arr.get(), sz, response_arr.get());

                    for (size_t i = 0u; i < sz; ++i){
                        if (!response_arr[i].has_value()){
                            auto response   = model::InternalResponse{.response     = std::unexpected(dg::network_exception::REST_INTERNAL_SERVER_ERROR),
                                                                      .ticket_id    = base_data_arr[i].ticket_id};

                            auto prep_arg   = InternalMailBoxPrepFeedArgument{.to               = base_data_arr[i].to,
                                                                              .response         = std::move(response)};

                            dg::network_producer_consumer::delvrsrv_deliver(this->mailbox_prep_feeder, std::move(prep_arg));
                            continue;
                        }

                        if (response_arr[i].value().has_value()){
                            auto response = model::InternalResponse{.response   = std::move(response_arr[i].value().value()),
                                                                    .ticket_id  = base_data_arr[i].ticket_id};

                            auto prep_arg   = InternalMailBoxPrepFeedArgument{.to               = base_data_arr[i].to,
                                                                              .response         = std::move(response)};

                            dg::network_producer_consumer::delvrsrv_deliver(this->mailbox_prep_feeder, std::move(prep_arg));
                            continue;
                        }

                        auto feed_arg    = InternalCacheServerFeedArgument{.to               = base_data_arr[i].to,
                                                                           .local_uri_path   = base_data_arr[i].local_uri_path,
                                                                           .cache_id         = base_data_arr[i].cache_id,
                                                                           .ticket_id        = base_data_arr[i].ticket_id,
                                                                           .request          = std::move(base_data_arr[i].request)};

                        dg::network_producer_consumer::delvrsrv_deliver(this->cache_server_feeder, std::move(feed_arg));
                    }
                }
            };
    };
}

namespace dg::network_rest_frame::client_impl1{

    using namespace dg::network_rest_frame::client; 

    static inline auto request_id_to_cache_id(const RequestID& request_id) noexcept -> CacheID{

        return CacheID{.ip = request_id.ip,
                       .native_cache_id = request_id.native_request_id};
    }

    template <class T, std::enable_if_t<std::is_unsigned_v<T>, bool> = true>
    static inline auto to_id_storage(T value) noexcept -> native_id_storage_t{

        constexpr size_t VALUE_TRIVIAL_SIZE = dg::network_trivial_serializer::size(T{}); 
        static_assert(VALUE_TRIVIAL_SIZE <= native_id_storage_t{}.size());

        native_id_storage_t result{}; 
        dg::network_trivial_serializer::serialize_into(result.data(), value);

        return result;
    } 

    //clear
    class SubscriberContainer{

        private:

            using observer_0_t = std::unique_ptr<ResponseOnReadyObserverInterface>;
            using observer_1_t = std::shared_ptr<ResponseOnReadyObserverInterface>; 

            std::variant<std::monostate, observer_0_t, observer_1_t> observer;

        public:

            SubscriberContainer(): observer(std::monostate{}){}

            auto subscribe(std::unique_ptr<ResponseOnReadyObserverInterface> observer) noexcept -> exception_t{

                if (observer == nullptr){
                    return dg::network_exception::INVALID_ARGUMENT;
                }

                this->observer = std::move(observer);
                return dg::network_exception::SUCCESS;
            }

            auto subscribe_sp(std::shared_ptr<ResponseOnReadyObserverInterface> observer) noexcept -> exception_t{

                if (observer == nullptr){
                    return dg::network_exception::INVALID_ARGUMENT;
                }

                this->observer = std::move(observer);
                return dg::network_exception::SUCCESS;
            }

            void notify() noexcept{

                if (std::holds_alternative<std::monostate>(this->observer)){
                    return;
                } else if (std::holds_alternative<std::unique_ptr<ResponseOnReadyObserverInterface>>(this->observer)){
                    std::get<std::unique_ptr<ResponseOnReadyObserverInterface>>(this->observer)->notify();
                } else if (std::holds_alternative<std::shared_ptr<ResponseOnReadyObserverInterface>>(this->observer)){
                    std::get<std::shared_ptr<ResponseOnReadyObserverInterface>>(this->observer)->notify();
                } else{
                    if constexpr(DEBUG_MODE_FLAG){
                        dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    } else{
                        std::unreachable();
                    }
                }
            }
    };

    //clear
    class UniqueConcurrentSubscriberContainer{

        private:

            SubscriberContainer base;

            std::atomic_flag was_set;
            std::atomic_flag was_done_set;
            std::atomic_flag was_notified; 

        public:

            //we'll leverage the special type of unique ticket to avoid circular dependency, such could be the callback function calling the subscribe function + etc.
            //essentially there is only one order thru for subscribe, one updated flag which guarantees that the unique order is thru so notify does not have race cond while accessing the underlying datatype
            //notify is also one time thru, so notify() does not have race cond (we dont really care about this feature, yet it is there to express the uniqueness)

            UniqueConcurrentSubscriberContainer(): base(),
                                                   was_set(false),
                                                   was_done_set(false),
                                                   was_notified(false){}

            auto subscribe(std::unique_ptr<ResponseOnReadyObserverInterface> observer) noexcept -> exception_t{

                if (observer == nullptr){
                    return dg::network_exception::INVALID_ARGUMENT;
                }

                bool has_set_right = this->was_set.test_and_set(std::memory_order_relaxed) == false;

                if (!has_set_right){
                    return dg::network_exception::REST_OBSERVER_SECOND_SET;
                }

                {
                    stdx::memtransaction_guard tx_grd;
                    dg::network_exception_handler::nothrow_log(this->base.subscribe(std::move(observer)));
                }

                this->was_done_set.test_and_set(std::memory_order_relaxed);

                return dg::network_exception::SUCCESS;
            }

            auto subscribe_sp(std::shared_ptr<ResponseOnReadyObserverInterface> observer) noexcept -> exception_t{

                if (observer == nullptr){
                    return dg::network_exception::INVALID_ARGUMENT;
                }

                bool has_set_right = this->was_set.test_and_set(std::memory_order_relaxed) == false;

                if (!has_set_right){
                    return dg::network_exception::REST_OBSERVER_SECOND_SET;
                }

                {
                    stdx::memtransaction_guard tx_grd;
                    dg::network_exception_handler::nothrow_log(this->base.subscribe_sp(std::move(observer)));
                }

                this->was_done_set.test_and_set(std::memory_order_relaxed);

                return dg::network_exception::SUCCESS;
            }

            auto notify() noexcept -> exception_t{

                bool precond_1  = this->was_done_set.test(std::memory_order_relaxed) == true; 

                if (!precond_1){
                    return dg::network_exception::REST_OBSERVER_BAD_NOTIFY;
                }

                bool precond_2  = this->was_notified.test_and_set(std::memory_order_relaxed) == false;

                if (!precond_2){
                    return dg::network_exception::REST_OBSERVER_SECOND_NOTIFY;
                }

                {
                    stdx::memtransaction_guard tx_grd;
                    this->base.notify();
                }

                return dg::network_exception::SUCCESS;
            }
    };

    //clear
    class BatchRequestResponseBase{

        private:

            stdx::inplace_hdi_container<std::atomic<intmax_t>> atomic_smp;
            dg::vector<std::expected<Response, exception_t>> resp_vec; //alright, there are hardware destructive interference issues, we dont want to talk about that yet
            stdx::inplace_hdi_container<std::atomic_flag> is_response_invoked;
            UniqueConcurrentSubscriberContainer subscriber_container;
            stdx::inplace_hdi_container<std::atomic_flag> is_done_updated;
            stdx::inplace_hdi_container<std::atomic_flag> is_done_updated_cleanup;

            static void assert_all_expected_initialized(dg::vector<std::expected<Response, exception_t>>& arg) noexcept{

                (void) arg;

                if constexpr(DEBUG_MODE_FLAG){
                    for (size_t i = 0u; i < arg.size(); ++i){
                        if (!arg[i].has_value() && arg[i].error() == dg::network_exception::EXPECTED_NOT_INITIALIZED){
                            dg::network_log_stackdump::critical(dg::network_exception::verbose(arg[i].error()));
                            std::abort();
                        }
                    }
                }
            }

            using self = BatchRequestResponseBase;

        public:

            BatchRequestResponseBase(size_t resp_sz): atomic_smp(std::in_place_t{}, -static_cast<intmax_t>(stdx::zero_throw(resp_sz)) + 1),
                                                      resp_vec(stdx::zero_throw(resp_sz), std::unexpected(dg::network_exception::EXPECTED_NOT_INITIALIZED)),
                                                      is_response_invoked(std::in_place_t{}, false),
                                                      subscriber_container(),
                                                      is_done_updated(std::in_place_t{}, false),
                                                      is_done_updated_cleanup(std::in_place_t{}, false){}

            //we'll improve the test_or_subscribe to guarantee no notify() on true later

            auto test_or_subscribe(std::unique_ptr<ResponseOnReadyObserverInterface> observer) noexcept -> std::expected<bool, exception_t>{

                if (this->atomic_smp.value.load(std::memory_order_relaxed) == 1){
                    return true;
                }

                exception_t err = this->subscriber_container.subscribe(std::move(observer)); 

                if (dg::network_exception::is_failed(err)){
                    return std::unexpected(err);
                }

                std::atomic_signal_fence(std::memory_order_seq_cst);

                if (this->atomic_smp.value.load(std::memory_order_relaxed) == 1){
                    return true;
                }

                return false;
            }

            auto test_or_subscribe_sp(std::shared_ptr<ResponseOnReadyObserverInterface> observer) noexcept -> std::expected<bool, exception_t>{

                if (this->atomic_smp.value.load(std::memory_order_relaxed) == 1){
                    return true;
                }

                exception_t err = this->subscriber_container.subscribe_sp(std::move(observer));

                if (dg::network_exception::is_failed(err)){
                    return std::unexpected(err);
                }

                std::atomic_signal_fence(std::memory_order_seq_cst);

                if (this->atomic_smp.value.load(std::memory_order_relaxed) == 1){
                    return true;
                }

                return false;
            }

            void update(size_t idx, std::expected<Response, exception_t> response) noexcept{

                this->internal_update(this, idx, std::move(response));
            }

            void deferred_memory_ordering_fetch(size_t idx, std::expected<Response, exception_t> response) noexcept{

                if constexpr(DEBUG_MODE_FLAG){
                    if (idx >= this->resp_vec.size()){
                        dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }

                    if (!response.has_value() && response.error() == dg::network_exception::EXPECTED_NOT_INITIALIZED){
                        dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                this->resp_vec[idx] = std::move(response);
            }

            void deferred_memory_ordering_fetch_close(size_t idx) noexcept{

                if constexpr(DEBUG_MODE_FLAG){
                    if (idx >= this->resp_vec.size()){
                        dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                this->internal_deferred_memory_ordering_fetch_close(idx, static_cast<void *>(&this->resp_vec[idx]));
            }

            auto response() noexcept -> std::expected<dg::vector<std::expected<Response, exception_t>>, exception_t>{

                bool was_invoked = this->is_response_invoked.value.test_and_set(std::memory_order_relaxed);

                if (was_invoked){
                    return std::unexpected(dg::network_exception::REST_RESPONSE_DOUBLE_INVOKE);
                }

                this->atomic_smp.value.wait(0, std::memory_order_acquire);
                self::assert_all_expected_initialized(this->resp_vec);

                return dg::vector<std::expected<Response, exception_t>>(std::move(this->resp_vec));
            }

            void close() noexcept{

                this->is_done_updated.value.wait(false, std::memory_order_relaxed);

                auto task = [&]() noexcept{
                    return this->is_done_updated_cleanup.value.test(std::memory_order_relaxed) == true;
                };

                stdx::busy_wait(task);
            }

        private:

            __attribute__((noipa)) static void internal_update(self * self_obj, size_t idx, std::expected<Response, exception_t> response) noexcept{

                if constexpr(DEBUG_MODE_FLAG){
                    if (idx >= self_obj->resp_vec.size()){
                        dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }

                    if (!response.has_value() && response.error() == dg::network_exception::EXPECTED_NOT_INITIALIZED){
                        dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                self_obj->resp_vec[idx] = std::move(response);
                intmax_t old            = self_obj->atomic_smp.value.fetch_add(1, std::memory_order_release);

                if constexpr(DEBUG_MODE_FLAG){
                    if (old > 0){
                        dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                if (old == 0){
                    self_obj->atomic_smp.value.notify_one();
                    std::atomic_signal_fence(std::memory_order_seq_cst);
                    self_obj->subscriber_container.notify();
                    std::atomic_signal_fence(std::memory_order_seq_cst);
                    self_obj->is_done_updated.value.test_and_set(std::memory_order_relaxed);
                    self_obj->is_done_updated.value.notify_one();
                    std::atomic_signal_fence(std::memory_order_seq_cst);
                    self_obj->is_done_updated_cleanup.value.test_and_set(std::memory_order_relaxed);
                }
            }

            void internal_deferred_memory_ordering_fetch_close(size_t idx, void * dirty_memory) noexcept{

                auto task = [](BatchRequestResponseBase * self_obj, size_t idx_arg, void * dirty_memory_arg) noexcept{
                    (void) idx_arg;
                    (void) dirty_memory_arg;

                    intmax_t old = self_obj->atomic_smp.value.fetch_add(1, std::memory_order_relaxed);

                    if constexpr(DEBUG_MODE_FLAG){
                        if (old > 0){
                            dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                            std::abort();
                        }
                    }

                    if (old == 0){
                        self_obj->atomic_smp.value.notify_one();
                        std::atomic_signal_fence(std::memory_order_seq_cst);
                        self_obj->subscriber_container.notify();
                        std::atomic_signal_fence(std::memory_order_seq_cst);
                        self_obj->is_done_updated.value.test_and_set(std::memory_order_relaxed);
                        self_obj->is_done_updated.value.notify_one();
                        std::atomic_signal_fence(std::memory_order_seq_cst);
                        self_obj->is_done_updated_cleanup.value.test_and_set(std::memory_order_relaxed);
                    }
                };

                stdx::noipa_do_task(task, this, idx, dirty_memory);
            }
    };

    //clear
    class BatchRequestResponse: public virtual BatchResponseInterface{

        private:

            class BatchRequestResponseBaseDesignatedObserver: public virtual ResponseObserverInterface{

                private:

                    BatchRequestResponseBase * base;
                    size_t idx;

                public:

                    BatchRequestResponseBaseDesignatedObserver() = default;

                    BatchRequestResponseBaseDesignatedObserver(BatchRequestResponseBase * base, 
                                                               size_t idx) noexcept: base(base),
                                                                                     idx(idx){}

                    void update(std::expected<Response, exception_t> response) noexcept{

                        this->base->update(this->idx, std::move(response));
                    }

                    void deferred_memory_ordering_fetch(std::expected<Response, exception_t> response) noexcept{

                        this->base->deferred_memory_ordering_fetch(this->idx, std::move(response));
                    }

                    void deferred_memory_ordering_fetch_close() noexcept{

                        this->base->deferred_memory_ordering_fetch_close(this->idx);
                    }
            };

            dg::vector<BatchRequestResponseBaseDesignatedObserver> observer_arr; 
            BatchRequestResponseBase base;
            stdx::inplace_hdi_container<std::atomic<bool>> response_wait_responsibility_flag;

            BatchRequestResponse(size_t resp_sz): observer_arr(resp_sz),
                                                  base(resp_sz),
                                                  response_wait_responsibility_flag(std::in_place_t{}, true){

                for (size_t i = 0u; i < resp_sz; ++i){
                    this->observer_arr[i] = BatchRequestResponseBaseDesignatedObserver(&this->base, i);
                }
            }

            friend auto make_batch_request_response(size_t resp_sz) noexcept -> std::expected<std::unique_ptr<BatchRequestResponse>, exception_t>; 

        public:

            BatchRequestResponse(const BatchRequestResponse&) = delete;
            BatchRequestResponse(BatchRequestResponse&&) = delete;
            BatchRequestResponse& operator =(const BatchRequestResponse&) = delete;
            BatchRequestResponse& operator =(BatchRequestResponse&&) = delete;

            ~BatchRequestResponse() noexcept{

                this->wait_response();
            }

            auto test_or_subscribe(std::unique_ptr<ResponseOnReadyObserverInterface> observer) noexcept -> std::expected<bool, exception_t>{

                return this->base.test_or_subscribe(std::move(observer));
            }

            auto test_or_subscribe_sp(std::shared_ptr<ResponseOnReadyObserverInterface> observer) noexcept -> std::expected<bool, exception_t>{

                return this->base.test_or_subscribe_sp(std::move(observer));
            }

            auto response() noexcept -> std::expected<dg::vector<std::expected<Response, exception_t>>, exception_t>{

                bool wait_responsibility = this->response_wait_responsibility_flag.value.exchange(false, std::memory_order_relaxed);

                if (wait_responsibility){
                    auto rs = dg::network_exception_handler::nothrow_log(this->base.response());
                    this->base.close();

                    return rs;
                }

                return std::unexpected(dg::network_exception::REST_RESPONSE_DOUBLE_INVOKE);
            }

            auto response_size() const noexcept -> size_t{

                return this->observer_arr.size();
            }

            auto get_observer(size_t idx) noexcept -> std::expected<ResponseObserverInterface *, exception_t>{ //response observer is a unique_resource, such is an acquisition of this pointer must involve accurate acquire + release mechanisms like every other dg::string or dg::vector, etc.

                if (idx >= this->observer_arr.size()){
                    return std::unexpected(dg::network_exception::INDEX_OUT_OF_RANGE);
                }

                return static_cast<ResponseObserverInterface *>(std::addressof(this->observer_arr[idx]));
            }

            void release_response_wait_responsibility() noexcept{

                this->response_wait_responsibility_flag.value.exchange(false, std::memory_order_relaxed);
            }

            void wait_response() noexcept{

                stdx::empty_noipa(this->response(), this->observer_arr, this->base);
            }
    };

    //
    auto make_batch_request_response(size_t resp_sz) noexcept -> std::expected<std::unique_ptr<BatchRequestResponse>, exception_t>{

        return {};
        // return dg::network_allocation::cstyle_make_unique<BatchRequestResponse>(resp_sz);
    }

    //clear
    class RequestContainer: public virtual RequestContainerInterface{

        private:

            dg::pow2_cyclic_queue<dg::vector<model::InternalRequest>> producer_queue;
            dg::pow2_cyclic_queue<std::pair<std::binary_semaphore *, std::optional<dg::vector<model::InternalRequest>> *>> waiting_queue;
            std::unique_ptr<std::mutex> mtx;

        public:

            RequestContainer(dg::pow2_cyclic_queue<dg::vector<model::InternalRequest>> producer_queue,
                             dg::pow2_cyclic_queue<std::pair<std::binary_semaphore *, std::optional<dg::vector<model::InternalRequest>> *>> waiting_queue,
                             std::unique_ptr<std::mutex> mtx) noexcept: producer_queue(std::move(producer_queue)),
                                                                        waiting_queue(std::move(waiting_queue)),
                                                                        mtx(std::move(mtx)){}

            auto push(dg::vector<model::InternalRequest>&& request) noexcept -> exception_t{

                std::binary_semaphore * releasing_smp = nullptr;

                exception_t err = [&]() noexcept{
                    stdx::xlock_guard<std::mutex> lck_grd(*this->mtx);

                    if (!this->waiting_queue.empty()){
                        auto [pending_smp, fetching_addr] = std::move(this->waiting_queue.front());
                        this->waiting_queue.pop_front();
                        *fetching_addr  = std::move(request);
                        std::atomic_signal_fence(std::memory_order_seq_cst);
                        releasing_smp   = pending_smp;

                        return dg::network_exception::SUCCESS;
                    }

                    if (this->producer_queue.size() == this->producer_queue.capacity()){
                        return dg::network_exception::RESOURCE_EXHAUSTION;
                    }

                    dg::network_exception_handler::nothrow_log(this->producer_queue.push_back(std::move(request)));
                    return dg::network_exception::SUCCESS;
                }();

                if (releasing_smp != nullptr){
                    releasing_smp->release();
                }

                return err;
            }

            auto pop() noexcept -> dg::vector<model::InternalRequest>{

                //alright I dont want to talk bad about compiler
                //but stack allocation is another beast, compiler can assume things they should not assume for unknown reasons 
                //we must instruct a launder operation, this is a valid operation (gcc volatile, not clang volatile) to hinder compiler optimizations
                //until we have found a patch, may we meet again

                auto pending_smp        = std::binary_semaphore(0);
                auto internal_request   = std::optional<dg::vector<model::InternalRequest>>{};

                while (true){
                    stdx::xlock_guard<std::mutex> lck_grd(*this->mtx);

                    if (!this->producer_queue.empty()){
                        auto rs = std::move(this->producer_queue.front());
                        this->producer_queue.pop_front();
                        return rs;
                    }

                    if (this->waiting_queue.size() == this->waiting_queue.capacity()){
                        continue;
                    }

                    this->waiting_queue.push_back(std::make_pair(&pending_smp, &internal_request));
                    break;
                }

                stdx::volatile_access(&pending_smp)->acquire(); //taints the value of pending_smp (good for stack deallocation), returns a binary semaphore out in the wild

                //taints the value of internal_request(good for stack deallocation) and consumes the pending_smp value
                //which renders this operation after the previous volatile access, returns a std::optional<> out in the wild
                //the volatile access is NOT encouraged to be used if not knowing exactly what this does

                //this goes for the std::pow2_cyclic_queue<>
                //think about the pow2_cyclic_queue for a second

                //every access to the queue is guaranteed to be defined, we aren't talking about the restriction of access of the containee's class members or the containee reference by itself
                //the pow2_cyclic_queue's only defined usage is push() and pop() in and out of the production queue
                //because the pop() transfer the restriction of access -> the callee, and forever detached from the returning result, puff... out

                //if we are to look at the lifetime of the containee
                //containee from the user -> the production queue, transfer the restriction of access -> the production queue upon SUCCESS
                //so there is no corrupted memory operation performed by compiler (the production queue holds unique reference to the containee, assume all logics were accurate up to the transfer point)
                //containee from production queue -> user, transfer the restriction of access from the production queue -> callee upon SUCCESS
                //there is no corrupted memory operation performed by compiler

                //poof by contradiction: assume there are memory corruptions, compiler must be wrongly assuming the values of the containees at the consumption point, such logic is inferred from another "internal pointer" of value, which is fenced out by std::launder() or violates the restrictness stated above
                //                       assume there will be memory corruptions not originated from the consuming variable, (x = container->pop()), this violates the precond restriction of access stated above 

                //launder is only good for restriction transferring, this is the only defined usage of launder without being afraid of invoking undefined behaviors, period.
                //the restriction is not only for the containee, but also every logically touchable memory_regions inferred from the containee
                //alright, this now sounds very confusing, we wont go there yet
                //our compiler is not perfect, neither are the implementors
                //we have our own ways of semanticalizing everything, we are literally on our own if we are to use C++ compilers

                return dg::vector<model::InternalRequest>(std::move(stdx::volatile_access(&internal_request, pending_smp)->value()));
            }
    };

    struct NormalTicketHasher{

        constexpr auto operator()(const ticket_id_t& ticket_id) const noexcept -> size_t{

            return dg::network_hash::hash_reflectible(ticket_id);
        }
    };

    struct IncrementingTicketHasher{

        constexpr auto operator()(const ticket_id_t& ticket_id) const noexcept -> size_t{

            static_assert(std::is_unsigned_v<ticket_id_t>);
            return ticket_id & std::numeric_limits<size_t>::max();
        }
    };

    //clear
    class ResponseObserverRelSafeWrapper{

        private:

            std::add_pointer_t<ResponseObserverInterface> response_observer;

        public:

            ResponseObserverRelSafeWrapper() = default;
            ResponseObserverRelSafeWrapper(std::add_pointer_t<ResponseObserverInterface> response_observer): response_observer(response_observer){}
            ResponseObserverRelSafeWrapper(const ResponseObserverRelSafeWrapper&) = delete;
            ResponseObserverRelSafeWrapper(ResponseObserverRelSafeWrapper&& other) noexcept: response_observer(std::exchange(other.response_observer, nullptr)){}

            ~ResponseObserverRelSafeWrapper() noexcept{

                if (this->response_observer != nullptr){
                    this->response_observer->update(std::unexpected(dg::network_exception::REST_OTHER_ERROR));
                }
            }

            ResponseObserverRelSafeWrapper& operator =(const ResponseObserverRelSafeWrapper&) = delete;

            ResponseObserverRelSafeWrapper& operator =(ResponseObserverRelSafeWrapper&& other) noexcept{

                if (std::addressof(other) == this){
                    return *this;
                }

                if (this->response_observer != nullptr){
                    this->response_observer->update(std::unexpected(dg::network_exception::REST_OTHER_ERROR));
                }

                this->response_observer = std::exchange(other.response_observer, nullptr);
                return *this;
            }

            auto get() noexcept -> std::add_pointer_t<ResponseObserverInterface>{

                return this->response_observer;
            }

            void release() noexcept{

                this->response_observer = nullptr;
            }     
    };

    //clear
    class TicketController: public virtual TicketControllerInterface{

        private:

            dg::unordered_unstable_map<model::ticket_id_t, std::optional<ResponseObserverRelSafeWrapper>> ticket_resource_map;
            size_t ticket_resource_map_cap;
            model::ticket_id_t ticket_id_counter;
            std::unique_ptr<std::mutex> mtx;
            stdx::hdi_container<size_t> max_consume_per_load;

        public:

            TicketController(dg::unordered_unstable_map<model::ticket_id_t, std::optional<ResponseObserverRelSafeWrapper>> ticket_resource_map,
                             size_t ticket_resource_map_cap,
                             model::ticket_id_t ticket_id_counter,
                             std::unique_ptr<std::mutex> mtx,
                             stdx::hdi_container<size_t> max_consume_per_load): ticket_resource_map(std::move(ticket_resource_map)),
                                                                                ticket_resource_map_cap(ticket_resource_map_cap),
                                                                                ticket_id_counter(ticket_id_counter),
                                                                                mtx(std::move(mtx)),
                                                                                max_consume_per_load(std::move(max_consume_per_load)){}

            auto open_ticket(size_t sz, model::ticket_id_t * out_ticket_arr) noexcept -> exception_t{

                if constexpr(DEBUG_MODE_FLAG){
                    if (sz > this->max_consume_size()){
                        dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                stdx::xlock_guard<std::mutex> lck_grd(*this->mtx);

                size_t new_sz = this->ticket_resource_map.size() + sz;

                if (new_sz > this->ticket_resource_map_cap){
                    return dg::network_exception::RESOURCE_EXHAUSTION;
                }

                for (size_t i = 0u; i < sz; ++i){
                    static_assert(std::is_unsigned_v<ticket_id_t>); //

                    model::ticket_id_t new_ticket_id    = this->ticket_id_counter++;
                    auto [map_ptr, status]              = this->ticket_resource_map.insert(std::make_pair(new_ticket_id, std::optional<ResponseObserverRelSafeWrapper>(std::nullopt)));
                    dg::network_exception_handler::dg_assert(status);
                    out_ticket_arr[i]                   = new_ticket_id;
                }

                return dg::network_exception::SUCCESS;
            }

            void assign_observer(model::ticket_id_t * ticket_id_arr, size_t sz, std::add_pointer_t<ResponseObserverInterface> * corresponding_observer_arr, std::expected<bool, exception_t> * exception_arr) noexcept{

                stdx::xlock_guard<std::mutex> lck_grd(*this->mtx);

                for (size_t i = 0u; i < sz; ++i){
                    model::ticket_id_t current_ticket_id = ticket_id_arr[i];
                    auto map_ptr = this->ticket_resource_map.find(current_ticket_id);

                    if (map_ptr == this->ticket_resource_map.end()){
                        exception_arr[i] = std::unexpected(dg::network_exception::REST_TICKET_NOT_FOUND);
                        continue;
                    }

                    if (map_ptr->second.has_value()){
                        exception_arr[i] = false;
                        continue;
                    }

                    //this is now confusing, because nullptr should suffice ...
                    if (corresponding_observer_arr[i] == nullptr){
                        exception_arr[i] = std::unexpected(dg::network_exception::INVALID_ARGUMENT);
                        continue;
                    }

                    map_ptr->second     = ResponseObserverRelSafeWrapper(corresponding_observer_arr[i]);
                    exception_arr[i]    = true;
                }
            }

            void steal_observer(model::ticket_id_t * ticket_id_arr, size_t sz, std::expected<std::add_pointer_t<ResponseObserverInterface>, exception_t> * response_arr) noexcept{

                stdx::xlock_guard<std::mutex> lck_grd(*this->mtx);

                for (size_t i = 0u; i < sz; ++i){
                    model::ticket_id_t current_ticket_id = ticket_id_arr[i];
                    auto map_ptr = this->ticket_resource_map.find(current_ticket_id);

                    if (map_ptr == this->ticket_resource_map.end()){
                        response_arr[i] = std::unexpected(dg::network_exception::REST_TICKET_NOT_FOUND);
                        continue;
                    }

                    if (!map_ptr->second.has_value()){
                        response_arr[i] = std::unexpected(dg::network_exception::REST_TICKET_OBSERVER_NOT_FOUND);
                        continue;
                    }

                    response_arr[i] = map_ptr->second.value().get();
                    map_ptr->second.value().release();
                    map_ptr->second = std::nullopt;
                }
            }

            void close_ticket(model::ticket_id_t * ticket_id_arr, size_t sz) noexcept{

                stdx::xlock_guard<std::mutex> lck_grd(*this->mtx);

                for (size_t i = 0u; i < sz; ++i){
                    //we are unforgiving
                    size_t removed_sz = this->ticket_resource_map.erase(ticket_id_arr[i]);

                    if constexpr(DEBUG_MODE_FLAG){
                        if (removed_sz == 0u){
                            dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                            std::abort();
                        }
                    }
                }
            }

            auto max_consume_size() noexcept -> size_t{

                return this->max_consume_per_load.value;
            }
    };

    //clear
    class DistributedTicketController: public virtual TicketControllerInterface{

        private:

            std::unique_ptr<std::unique_ptr<TicketControllerInterface>[]> base_arr;
            size_t pow2_base_arr_sz;
            size_t probe_arr_sz;
            size_t keyvalue_feed_cap;
            size_t minimum_discretization_sz;
            size_t maximum_discretization_sz;
            size_t max_consume_per_load; 

        public:

            DistributedTicketController(std::unique_ptr<std::unique_ptr<TicketControllerInterface>[]> base_arr,
                                        size_t pow2_base_arr_sz,
                                        size_t probe_arr_sz,
                                        size_t keyvalue_feed_cap,
                                        size_t minimum_discretization_sz,
                                        size_t maximum_discretization_sz,
                                        size_t max_consume_per_load) noexcept: base_arr(std::move(base_arr)),
                                                                               pow2_base_arr_sz(pow2_base_arr_sz),
                                                                               probe_arr_sz(probe_arr_sz),
                                                                               keyvalue_feed_cap(keyvalue_feed_cap),
                                                                               minimum_discretization_sz(minimum_discretization_sz),
                                                                               maximum_discretization_sz(maximum_discretization_sz),
                                                                               max_consume_per_load(max_consume_per_load){}

            auto open_ticket(size_t sz, model::ticket_id_t * rs) noexcept -> exception_t{

                if constexpr(DEBUG_MODE_FLAG){
                    if (sz > this->max_consume_size()){
                        dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                size_t tentative_discretization_sz  = sz / this->probe_arr_sz;
                size_t discretization_sz            = std::min(std::max(tentative_discretization_sz, this->minimum_discretization_sz), this->maximum_discretization_sz);
                size_t peeking_base_arr_sz          = sz / discretization_sz + static_cast<size_t>(sz % discretization_sz != 0u); 
                size_t success_sz                   = 0u;

                for (size_t i = 0u; i < peeking_base_arr_sz; ++i){
                    size_t first        = i * discretization_sz;
                    size_t last         = std::max(static_cast<size_t>((i + 1) * discretization_sz), sz);
                    size_t sub_sz       = last - first; 
                    size_t random_clue  = dg::network_randomizer::randomize_int<size_t>(); 
                    size_t base_arr_idx = random_clue & (this->pow2_base_arr_sz - 1u);
                    exception_t err     = this->base_arr[base_arr_idx]->open_ticket(sub_sz, std::next(rs, first));

                    if (dg::network_exception::is_failed(err)){
                        this->close_ticket(rs, success_sz);
                        return err;
                    }

                    for (size_t i = 0u; i < sub_sz; ++i){
                        rs[first + i] = this->internal_encode_ticket_id(rs[first + i], base_arr_idx);                        
                    }

                    success_sz += sub_sz;
                }

                return dg::network_exception::SUCCESS;
            }

            void assign_observer(model::ticket_id_t * ticket_id_arr, size_t sz, std::add_pointer_t<ResponseObserverInterface> * assigning_observer_arr, std::expected<bool, exception_t> * exception_arr) noexcept{

                auto feed_resolutor                 = InternalAssignObserverFeedResolutor{};
                feed_resolutor.controller_arr       = this->base_arr.get();

                size_t trimmed_keyvalue_feed_cap    = std::min(this->keyvalue_feed_cap, sz);
                size_t feeder_allocation_cost       = dg::network_producer_consumer::delvrsrv_kv_allocation_cost(&feed_resolutor, trimmed_keyvalue_feed_cap);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> feeder_mem(feeder_allocation_cost);
                auto feeder                         = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_kv_open_preallocated_raiihandle(&feed_resolutor, trimmed_keyvalue_feed_cap, feeder_mem.get()));

                for (size_t i = 0u; i < sz; ++i){
                    ticket_id_t base_ticket_id                  = {};
                    size_t partitioned_idx                      = {};
                    std::tie(base_ticket_id, partitioned_idx)   = this->internal_decode_ticket_id(ticket_id_arr[i]);

                    if (partitioned_idx >= this->pow2_base_arr_sz){
                        exception_arr[i] = std::unexpected(dg::network_exception::REST_TICKET_NOT_FOUND);
                        continue;
                    }

                    auto feed_arg           = InternalAssignObserverFeedArgument{};
                    feed_arg.base_ticket_id = base_ticket_id;
                    feed_arg.observer       = assigning_observer_arr[i];
                    feed_arg.exception_ptr  = std::next(exception_arr, i);

                    dg::network_producer_consumer::delvrsrv_kv_deliver(feeder.get(), partitioned_idx, feed_arg);
                }
            }

            void steal_observer(model::ticket_id_t * ticket_id_arr, size_t sz, std::expected<std::add_pointer_t<ResponseObserverInterface>, exception_t> * out_observer_arr) noexcept{

                auto feed_resolutor                 = InternalStealObserverFeedResolutor{};
                feed_resolutor.controller_arr       = this->base_arr.get();

                size_t trimmed_keyvalue_feed_cap    = std::min(this->keyvalue_feed_cap, sz);
                size_t feeder_allocation_cost       = dg::network_producer_consumer::delvrsrv_kv_allocation_cost(&feed_resolutor, trimmed_keyvalue_feed_cap);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> feeder_mem(feeder_allocation_cost);
                auto feeder                         = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_kv_open_preallocated_raiihandle(&feed_resolutor, trimmed_keyvalue_feed_cap, feeder_mem.get()));

                for (size_t i = 0u; i < sz; ++i){
                    ticket_id_t base_ticket_id                  = {};
                    size_t partitioned_idx                      = {};
                    std::tie(base_ticket_id, partitioned_idx)   = this->internal_decode_ticket_id(ticket_id_arr[i]);

                    if (partitioned_idx >= this->pow2_base_arr_sz){
                        out_observer_arr[i] = std::unexpected(dg::network_exception::REST_TICKET_NOT_FOUND);
                        continue;
                    }

                    auto feed_arg               = InternalStealObserverFeedArgument{};
                    feed_arg.base_ticket_id     = base_ticket_id;
                    feed_arg.out_observer_ptr   = std::next(out_observer_arr, i);

                    dg::network_producer_consumer::delvrsrv_kv_deliver(feeder.get(), partitioned_idx, feed_arg);
                }
            }

            void close_ticket(model::ticket_id_t * ticket_id_arr, size_t sz) noexcept{

                auto feed_resolutor                 = InternalCloseTicketFeedResolutor{};
                feed_resolutor.controller_arr       = this->base_arr.get();

                size_t trimmed_keyvalue_feed_cap    = std::min(this->keyvalue_feed_cap, sz);
                size_t feeder_allocation_cost       = dg::network_producer_consumer::delvrsrv_kv_allocation_cost(&feed_resolutor, trimmed_keyvalue_feed_cap);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> feeder_mem(feeder_allocation_cost);
                auto feeder                         = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_kv_open_preallocated_raiihandle(&feed_resolutor, trimmed_keyvalue_feed_cap, feeder_mem.get()));

                for (size_t i = 0u; i < sz; ++i){
                    ticket_id_t base_ticket_id                  = {};
                    size_t partitioned_idx                      = {};
                    std::tie(base_ticket_id, partitioned_idx)   = this->internal_decode_ticket_id(ticket_id_arr[i]);

                    if constexpr(DEBUG_MODE_FLAG){
                        if (partitioned_idx >= this->pow2_base_arr_sz){
                            dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION)); //we are very unforgiving about the inverse operation, because it hints a serious corruption has occurred
                            std::abort();
                        }
                    }

                    // auto feed_arg           = InternalCloseTicketFeedArgument{};
                    // feed_arg.base_ticket_id = base_ticket_id;

                    dg::network_producer_consumer::delvrsrv_kv_deliver(feeder.get(), partitioned_idx, base_ticket_id);
                }
            }

            auto max_consume_size() noexcept -> size_t{

                return this->max_consume_per_load;
            }
        
        private:

            inline auto internal_encode_ticket_id(ticket_id_t base_ticket_id, size_t base_arr_idx) noexcept -> ticket_id_t{

                static_assert(std::is_unsigned_v<ticket_id_t>);

                size_t popcount = std::countr_zero(this->pow2_base_arr_sz);
                return stdx::safe_unsigned_lshift(base_ticket_id, popcount) | base_arr_idx;
            }

            inline auto internal_decode_ticket_id(ticket_id_t current_ticket_id) noexcept -> std::pair<ticket_id_t, size_t>{

                static_assert(std::is_unsigned_v<ticket_id_t>);

                size_t popcount             = std::countr_zero(this->pow2_base_arr_sz);
                size_t bitmask              = stdx::lowones_bitgen<size_t>(popcount);
                size_t base_arr_idx         = current_ticket_id & bitmask;
                ticket_id_t base_ticket_id  = current_ticket_id >> popcount;

                return std::make_pair(base_ticket_id, base_arr_idx);
            }

            struct InternalAssignObserverFeedArgument{
                ticket_id_t base_ticket_id;
                std::add_pointer_t<ResponseObserverInterface> observer;
                std::expected<bool, exception_t> * exception_ptr;
            };

            struct InternalAssignObserverFeedResolutor: dg::network_producer_consumer::KVConsumerInterface<size_t, InternalAssignObserverFeedArgument>{

                std::unique_ptr<TicketControllerInterface> * controller_arr;

                void push(const size_t& partitioned_idx, std::move_iterator<InternalAssignObserverFeedArgument *> data_arr, size_t sz) noexcept{
                    
                    InternalAssignObserverFeedArgument * base_data_arr = data_arr.base();

                    dg::network_stack_allocation::NoExceptAllocation<ticket_id_t[]> ticket_id_arr(sz);
                    dg::network_stack_allocation::NoExceptAllocation<std::add_pointer_t<ResponseObserverInterface>[]> assigning_observer_arr(sz);
                    dg::network_stack_allocation::NoExceptAllocation<std::expected<bool, exception_t>[]> exception_arr(sz);

                    for (size_t i = 0u; i < sz; ++i){
                        ticket_id_arr[i]            = base_data_arr[i].base_ticket_id;
                        assigning_observer_arr[i]   = base_data_arr[i].observer;
                    }

                    this->controller_arr[partitioned_idx]->assign_observer(ticket_id_arr.get(), sz, assigning_observer_arr.get(), exception_arr.get());

                    for (size_t i = 0u; i < sz; ++i){
                        *base_data_arr[i].exception_ptr = exception_arr[i];
                    }
                }
            };

            struct InternalStealObserverFeedArgument{
                ticket_id_t base_ticket_id;
                std::expected<std::add_pointer_t<ResponseObserverInterface>, exception_t> * out_observer_ptr;
            };

            struct InternalStealObserverFeedResolutor: dg::network_producer_consumer::KVConsumerInterface<size_t, InternalStealObserverFeedArgument>{

                std::unique_ptr<TicketControllerInterface> * controller_arr;

                void push(const size_t& partitioned_idx, std::move_iterator<InternalStealObserverFeedArgument *> data_arr, size_t sz) noexcept{

                    InternalStealObserverFeedArgument * base_data_arr = data_arr.base();

                    dg::network_stack_allocation::NoExceptAllocation<ticket_id_t[]> ticket_id_arr(sz);
                    dg::network_stack_allocation::NoExceptAllocation<std::expected<std::add_pointer_t<ResponseObserverInterface>, exception_t>[]> stealing_observer_arr(sz);

                    for (size_t i = 0u; i < sz; ++i){
                        ticket_id_arr[i] = base_data_arr[i].base_ticket_id;
                    }

                    this->controller_arr[partitioned_idx]->steal_observer(ticket_id_arr.get(), sz, stealing_observer_arr.get());

                    for (size_t i = 0u; i < sz; ++i){
                        *base_data_arr[i].out_observer_ptr = stealing_observer_arr[i];
                    }
                }
            };

            // struct InternalCloseTicketFeedArgument{
            //     ticket_id_t base_ticket_id;
            // };

            struct InternalCloseTicketFeedResolutor: dg::network_producer_consumer::KVConsumerInterface<size_t, ticket_id_t>{

                std::unique_ptr<TicketControllerInterface> * controller_arr;

                void push(const size_t& partitioned_idx, std::move_iterator<ticket_id_t *> data_arr, size_t sz) noexcept{

                    // InternalCloseTicketFeedArgument * base_data_arr = data_arr.base();

                    // dg::network_stack_allocation::NoExceptAllocation<ticket_id_t[]> ticket_id_arr(sz);

                    // for (size_t i = 0u; i < sz; ++i){
                        // ticket_id_arr[i] = base_data_arr[i].base_ticket_id;
                    // }

                    this->controller_arr[partitioned_idx]->close_ticket(data_arr.base(), sz);
                }
            };
    };

    //clear
    class TicketTimeoutManager: public virtual TicketTimeoutManagerInterface{

        public:

            struct ExpiryBucket{
                model::ticket_id_t ticket_id;
                std::chrono::time_point<std::chrono::steady_clock> abs_timeout;
            };

        private:

            dg::vector<ExpiryBucket> expiry_bucket_queue; //this is harder than expected, we are afraid of the priority queue, yet we would want to discretize this to avoid priority queues
            size_t expiry_bucket_queue_cap;
            std::unique_ptr<std::mutex> mtx;
            stdx::hdi_container<std::chrono::nanoseconds> max_dur;
            stdx::hdi_container<size_t> max_consume_per_load;

        public:

            TicketTimeoutManager(dg::vector<ExpiryBucket> expiry_bucket_queue,
                                 size_t expiry_bucket_queue_cap,
                                 std::unique_ptr<std::mutex> mtx,
                                 stdx::hdi_container<std::chrono::nanoseconds> max_dur,
                                 stdx::hdi_container<size_t> max_consume_per_load) noexcept: expiry_bucket_queue(std::move(expiry_bucket_queue)),
                                                                                             expiry_bucket_queue_cap(expiry_bucket_queue_cap),
                                                                                             mtx(std::move(mtx)),
                                                                                             max_dur(std::move(max_dur)),
                                                                                             max_consume_per_load(std::move(max_consume_per_load)){}

            void clock_in(ClockInArgument * registering_arr, size_t sz, exception_t * exception_arr) noexcept{

                if constexpr(DEBUG_MODE_FLAG){
                    if (sz > this->max_consume_size()){
                        dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }
                
                stdx::xlock_guard<std::mutex> lck_grd(*this->mtx);

                auto now        = std::chrono::steady_clock::now(); 
                auto greater    = [](const ExpiryBucket& lhs, const ExpiryBucket& rhs) noexcept {return lhs.abs_timeout > rhs.abs_timeout;};

                for (size_t i = 0u; i < sz; ++i){
                    auto [ticket_id, current_dur] = std::make_pair(registering_arr[i].clocked_in_ticket, registering_arr[i].expiry_dur);

                    if (current_dur > this->max_clockin_dur()){
                        exception_arr[i] = dg::network_exception::REST_INVALID_TIMEOUT;
                        continue;
                    }

                    if (this->expiry_bucket_queue.size() == this->expiry_bucket_queue_cap){
                        exception_arr[i] = dg::network_exception::QUEUE_FULL;
                        continue;
                    }

                    this->expiry_bucket_queue.push_back(ExpiryBucket{ticket_id, now + current_dur});
                    std::push_heap(this->expiry_bucket_queue.begin(), this->expiry_bucket_queue.end(), greater);
                    exception_arr[i] = dg::network_exception::SUCCESS;
                }
            }

            void get_expired_ticket(model::ticket_id_t * ticket_arr, size_t& ticket_arr_sz, size_t ticket_arr_cap) noexcept{

                stdx::xlock_guard<std::mutex> lck_grd(*this->mtx);

                ticket_arr_sz   = 0u;
                auto now        = std::chrono::steady_clock::now();
                auto greater    = [](const ExpiryBucket& lhs, const ExpiryBucket& rhs) noexcept {return lhs.abs_timeout > rhs.abs_timeout;};

                while (true){
                    if (ticket_arr_sz == ticket_arr_cap){
                        return;
                    }

                    if (this->expiry_bucket_queue.empty()){
                        return;
                    }

                    if (this->expiry_bucket_queue.front().abs_timeout > now){
                        return;
                    }

                    ticket_arr[ticket_arr_sz++] = this->expiry_bucket_queue.front().ticket_id;
                    std::pop_heap(this->expiry_bucket_queue.begin(), this->expiry_bucket_queue.end(), greater);
                    this->expiry_bucket_queue.pop_back();
                }
            }

            auto max_clockin_dur() const noexcept -> std::chrono::nanoseconds{

                return this->max_dur.value;
            }

            void clear() noexcept{

                stdx::xlock_guard<std::mutex> lck_grd(*this->mtx);

                this->expiry_bucket_queue.clear();
            }

            auto max_consume_size() noexcept -> size_t{

                return this->max_consume_per_load.value;
            }
    };

    //clear
    class DistributedTicketTimeoutManager: public virtual TicketTimeoutManagerInterface{

        private:

            std::unique_ptr<std::unique_ptr<TicketTimeoutManagerInterface>[]> base_arr;
            size_t pow2_base_arr_sz;
            size_t keyvalue_feed_cap;
            size_t zero_bounce_sz;
            std::unique_ptr<DrainerPredicateInterface> drainer_predicate;
            std::chrono::nanoseconds max_dur;
            size_t drain_peek_cap_per_container;
            size_t max_consume_per_load;

        public:

            DistributedTicketTimeoutManager(std::unique_ptr<std::unique_ptr<TicketTimeoutManagerInterface>[]> base_arr,
                                            size_t pow2_base_arr_sz,
                                            size_t keyvalue_feed_cap,
                                            size_t zero_bounce_sz,
                                            std::unique_ptr<DrainerPredicateInterface> drainer_predicate,
                                            std::chrono::nanoseconds max_dur,
                                            size_t drain_peek_cap_per_container,
                                            size_t max_consume_per_load) noexcept: base_arr(std::move(base_arr)),
                                                                                   pow2_base_arr_sz(pow2_base_arr_sz),
                                                                                   keyvalue_feed_cap(keyvalue_feed_cap),
                                                                                   zero_bounce_sz(zero_bounce_sz),
                                                                                   drainer_predicate(std::move(drainer_predicate)),
                                                                                   max_dur(max_dur),
                                                                                   drain_peek_cap_per_container(drain_peek_cap_per_container),
                                                                                   max_consume_per_load(max_consume_per_load){}

            void clock_in(ClockInArgument * registering_arr, size_t sz, exception_t * exception_arr) noexcept{

                auto feed_resolutor                 = InternalClockInFeedResolutor{};
                feed_resolutor.manager_arr          = this->base_arr.get(); 

                size_t trimmed_keyvalue_feed_cap    = std::min(this->keyvalue_feed_cap, sz);
                size_t feeder_allocation_cost       = dg::network_producer_consumer::delvrsrv_kv_allocation_cost(&feed_resolutor, trimmed_keyvalue_feed_cap);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> feeder_mem(feeder_allocation_cost);
                auto feeder                         = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_kv_open_preallocated_raiihandle(&feed_resolutor, trimmed_keyvalue_feed_cap, feeder_mem.get()));

                std::fill(exception_arr, std::next(exception_arr, sz), dg::network_exception::SUCCESS);

                for (size_t i = 0u; i < sz; ++i){
                    if (registering_arr[i].expiry_dur > this->max_clockin_dur()){
                        exception_arr[i] = dg::network_exception::REST_INVALID_TIMEOUT;
                        continue;
                    }

                    size_t hashed_value     = dg::network_hash::hash_reflectible(registering_arr[i].clocked_in_ticket);
                    size_t partitioned_idx  = hashed_value & (this->pow2_base_arr_sz - 1u);

                    auto feed_arg           = InternalClockInFeedArgument{};
                    feed_arg.ticket_id      = registering_arr[i].clocked_in_ticket;
                    feed_arg.dur            = registering_arr[i].expiry_dur;
                    feed_arg.exception_ptr  = std::next(exception_arr, i); 

                    dg::network_producer_consumer::delvrsrv_kv_deliver(feeder.get(), partitioned_idx, feed_arg);
                }
            }

            auto max_clockin_dur() const noexcept -> std::chrono::nanoseconds{

                return this->max_dur;
            }

            void get_expired_ticket(model::ticket_id_t * ticket_arr, size_t& ticket_arr_sz, size_t ticket_arr_cap) noexcept{

                if (this->drainer_predicate->is_should_drain()){
                    this->internal_drain(ticket_arr, ticket_arr_sz, ticket_arr_cap);
                    this->drainer_predicate->reset();
                } else{
                    this->internal_curious_pop(ticket_arr, ticket_arr_sz, ticket_arr_cap);
                }
            }

            void clear() noexcept{

                for (size_t i = 0u; i < this->pow2_base_arr_sz; ++i){
                    this->base_arr[i]->clear();
                }
            }

            auto max_consume_size() noexcept -> size_t{

                return this->max_consume_per_load;
            }
        
        private:

            struct InternalClockInFeedArgument{
                ticket_id_t ticket_id;
                std::chrono::nanoseconds dur;
                exception_t * exception_ptr;
            };

            struct InternalClockInFeedResolutor: dg::network_producer_consumer::KVConsumerInterface<size_t, InternalClockInFeedArgument>{

                std::unique_ptr<TicketTimeoutManagerInterface> * manager_arr;

                void push(const size_t& partitioned_idx, std::move_iterator<InternalClockInFeedArgument *> data_arr, size_t sz) noexcept{
                    
                    InternalClockInFeedArgument * base_data_arr = data_arr.base();

                    dg::network_stack_allocation::NoExceptAllocation<ClockInArgument[]> registering_arr(sz);
                    dg::network_stack_allocation::NoExceptAllocation<exception_t[]> exception_arr(sz);

                    for (size_t i = 0u; i < sz; ++i){
                        registering_arr[i] = ClockInArgument{.clocked_in_ticket = base_data_arr[i].ticket_id, 
                                                             .expiry_dur        = base_data_arr[i].dur};
                    }

                    this->manager_arr[partitioned_idx]->clock_in(registering_arr.get(), sz, exception_arr.get());

                    for (size_t i = 0u; i < sz; ++i){
                        if (dg::network_exception::is_failed(exception_arr[i])){
                            *base_data_arr[i].exception_ptr = exception_arr[i];
                        }
                    }
                }
            };

            void internal_drain(model::ticket_id_t * ticket_arr, size_t& ticket_arr_sz, size_t ticket_arr_cap) noexcept{

                size_t random_clue                      = dg::network_randomizer::randomize_int<size_t>();
                ticket_arr_sz                           = 0u;
                model::ticket_id_t * current_ticket_arr = ticket_arr;
                size_t current_arr_cap                  = ticket_arr_cap;

                for (size_t i = 0u; i < this->pow2_base_arr_sz; ++i){
                    if (current_arr_cap == 0u){
                        break;
                    }

                    size_t random_idx   = (random_clue + i) & (this->pow2_base_arr_sz - 1u);
                    size_t current_sz   = {};
                    size_t peeking_cap  = std::min(current_arr_cap, this->drain_peek_cap_per_container); 
                    this->base_arr[random_idx]->get_expired_ticket(current_ticket_arr, current_sz, peeking_cap);
                    std::advance(current_ticket_arr, current_sz);
                    current_arr_cap     -= current_sz;
                    ticket_arr_sz       += current_sz;
                }
            }

            void internal_curious_pop(model::ticket_id_t * ticket_arr, size_t& ticket_arr_sz, size_t ticket_arr_cap) noexcept{

                ticket_arr_sz = 0u;

                for (size_t i = 0u; i < this->zero_bounce_sz; ++i){
                    size_t idx = dg::network_randomizer::randomize_int<size_t>() & (this->pow2_base_arr_sz - 1u);
                    this->base_arr[idx]->get_expired_ticket(ticket_arr, ticket_arr_sz, ticket_arr_cap);

                    if (ticket_arr_sz != 0u){
                        return;
                    }
                }
            }
    };

    //clear
    class ExhaustionControlledTicketTimeoutManager: public virtual TicketTimeoutManagerInterface{

        private:

            std::unique_ptr<TicketTimeoutManagerInterface> base;
            std::shared_ptr<dg::network_concurrency_infretry_x::ExecutorInterface> infretry_device; //its insanely hard to solve this by using notify + atomic_wait, we'll come up with a patch 

        public:

            ExhaustionControlledTicketTimeoutManager(std::unique_ptr<TicketTimeoutManagerInterface> base,
                                                     std::shared_ptr<dg::network_concurrency_infretry_x::ExecutorInterface> infretry_device) noexcept: base(std::move(base)),
                                                                                                                                                       infretry_device(std::move(infretry_device)){}

            void clock_in(ClockInArgument * registering_arr, size_t sz, exception_t * exception_arr) noexcept{

                auto first_registering_ptr          = registering_arr;
                auto last_registering_ptr           = std::next(first_registering_ptr, sz);
                exception_t * first_exception_ptr   = exception_arr;
                exception_t * last_exception_ptr    = std::next(first_exception_ptr, sz);
                size_t sliding_window_sz            = sz;

                auto task = [&, this]() noexcept{
                    this->base->clock_in(first_registering_ptr, sliding_window_sz, first_exception_ptr);

                    exception_t * first_retriable_exception_ptr = std::find(first_exception_ptr, last_exception_ptr, dg::network_exception::QUEUE_FULL);
                    exception_t * last_retriable_exception_ptr  = std::find_if(first_retriable_exception_ptr, last_exception_ptr, [](exception_t err){return err != dg::network_exception::QUEUE_FULL;});

                    size_t relative_offset                      = std::distance(first_exception_ptr, first_retriable_exception_ptr);
                    sliding_window_sz                           = std::distance(first_retriable_exception_ptr, last_retriable_exception_ptr);

                    std::advance(first_registering_ptr, relative_offset);
                    std::advance(first_exception_ptr, relative_offset);

                    return first_exception_ptr == last_exception_ptr;
                };

                dg::network_concurrency_infretry_x::ExecutableWrapper virtual_task(task);
                this->infretry_device->exec(virtual_task);
            }

            void get_expired_ticket(model::ticket_id_t * output_arr, size_t& sz, size_t cap) noexcept{

                this->base->get_expired_ticket(output_arr, sz, cap);
            }

            auto max_clockin_dur() const noexcept -> std::chrono::nanoseconds{

                return this->base->max_clockin_dur();
            }

            void clear() noexcept{

                this->base->clear();
            }

            auto max_consume_size() noexcept -> size_t{

                return this->base->max_consume_size();
            }
    };

    //clear
    class IncrementingRequestIDGenerator: public virtual RequestIDGeneratorInterface{

        private:

            std::array<char, 8u> ip;
            uint8_t ip_factory_id;
            stdx::inplace_hdi_container<std::atomic<uint64_t>> id_counter;

        public:

            IncrementingRequestIDGenerator(std::array<char, 8u> ip,
                                           uint8_t ip_factory_id,
                                           uint64_t id_counter) noexcept: ip(ip),
                                                                          ip_factory_id(ip_factory_id),
                                                                          id_counter(std::in_place_t{}, id_counter){}

            auto get(size_t ticket_sz, RequestID * output_request_id_arr) noexcept -> exception_t{

                uint64_t start_id   = this->id_counter.value.fetch_add(ticket_sz, std::memory_order_relaxed);
                uint64_t current_id = start_id;

                for (size_t i = 0u; i < ticket_sz; ++i){
                    output_request_id_arr[i] = RequestID{.ip                = this->ip,
                                                         .native_request_id = client_impl1::to_id_storage(current_id++)};
                }

                return dg::network_exception::SUCCESS;
            }
    };

    //clear
    class InBoundWorker: public virtual dg::network_concurrency::WorkerInterface{

        private:

            std::shared_ptr<TicketControllerInterface> ticket_controller;
            uint32_t channel;
            size_t ticket_controller_feed_cap;
            size_t recv_consume_sz;
            size_t busy_consume_sz;
        
        public:

            InBoundWorker(std::shared_ptr<TicketControllerInterface> ticket_controller,
                          uint32_t channel,
                          size_t ticket_controller_feed_cap,
                          size_t recv_consume_sz,
                          size_t busy_consume_sz) noexcept: ticket_controller(std::move(ticket_controller)),
                                                            channel(channel),
                                                            ticket_controller_feed_cap(ticket_controller_feed_cap),
                                                            recv_consume_sz(recv_consume_sz),
                                                            busy_consume_sz(busy_consume_sz){}

            bool run_one_epoch() noexcept{

                size_t buf_arr_cap  = this->recv_consume_sz;
                size_t buf_arr_sz   = {};
                dg::network_stack_allocation::NoExceptAllocation<dg::string[]> buf_arr(buf_arr_cap); 
                dg::network_kernel_mailbox::recv(this->channel, buf_arr.get(), buf_arr_sz, buf_arr_cap);

                auto feed_resolutor                         = InternalFeedResolutor{};
                feed_resolutor.ticket_controller            = this->ticket_controller.get();

                size_t trimmed_ticket_controller_feed_cap   = std::min(this->ticket_controller_feed_cap, buf_arr_sz);
                size_t feeder_allocation_cost               = dg::network_producer_consumer::delvrsrv_allocation_cost(&feed_resolutor, trimmed_ticket_controller_feed_cap);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> feeder_mem(feeder_allocation_cost);
                auto feeder                                 = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_open_preallocated_raiihandle(&feed_resolutor, trimmed_ticket_controller_feed_cap, feeder_mem.get())); 

                for (size_t i = 0u; i < buf_arr_sz; ++i){
                    std::expected<model::InternalResponse, exception_t> response = dg::network_exception::to_cstyle_function(dg::network_compact_serializer::dgstd_deserialize<model::InternalResponse, dg::string>)(buf_arr[i], model::INTERNAL_RESPONSE_SERIALIZATION_SECRET);

                    if (!response.has_value()){
                        dg::network_log_stackdump::error_fast_optional(dg::network_exception::verbose(response.error()));
                        continue;
                    }

                    dg::network_producer_consumer::delvrsrv_deliver(feeder.get(), std::move(response.value()));                    
                }

                return buf_arr_sz >= this->busy_consume_sz;
            }

        private:

            struct InternalFeedResolutor: dg::network_producer_consumer::ConsumerInterface<model::InternalResponse>{

                TicketControllerInterface * ticket_controller;

                void push(std::move_iterator<model::InternalResponse *> response_arr, size_t sz) noexcept{

                    model::InternalResponse * base_response_arr = response_arr.base();

                    dg::network_stack_allocation::NoExceptAllocation<model::ticket_id_t[]> ticket_id_arr(sz);
                    dg::network_stack_allocation::NoExceptAllocation<std::expected<std::add_pointer_t<ResponseObserverInterface>, exception_t>[]> observer_arr(sz);

                    for (size_t i = 0u; i < sz; ++i){
                        ticket_id_arr[i] = base_response_arr[i].ticket_id;
                    }

                    this->ticket_controller->steal_observer(ticket_id_arr.get(), sz, observer_arr.get());

                    for (size_t i = 0u; i < sz; ++i){
                        if (!observer_arr[i].has_value()){
                            dg::network_log_stackdump::error_fast_optional(dg::network_exception::verbose(dg::network_exception::REST_BAD_RESPONSE));
                            continue;
                        }

                        stdx::safe_ptr_access(observer_arr[i].value())->deferred_memory_ordering_fetch(std::move(base_response_arr[i].response)); //declare expectations
                    }

                    std::atomic_thread_fence(std::memory_order_release);
                    //we'd like to have a signal fence std::memory_order_seq_cst just for our sake of safety
                    std::atomic_signal_fence(std::memory_order_seq_cst);

                    for (size_t i = 0u; i < sz; ++i){
                        if (!observer_arr[i].has_value()){
                            continue;
                        }

                        stdx::safe_ptr_access(observer_arr[i].value())->deferred_memory_ordering_fetch_close();
                    }
                }
            };
    };

    //clear
    class OutBoundWorker: public virtual dg::network_concurrency::WorkerInterface{

        private:

            std::shared_ptr<RequestContainerInterface> request_container;
            uint32_t channel;
            size_t mailbox_feed_cap;

        public:

            OutBoundWorker(std::shared_ptr<RequestContainerInterface> request_container,
                           uint32_t channel,
                           size_t mailbox_feed_cap) noexcept: request_container(std::move(request_container)),
                                                              channel(channel),
                                                              mailbox_feed_cap(mailbox_feed_cap){}

            bool run_one_epoch() noexcept{

                dg::vector<model::InternalRequest> request_vec = this->request_container->pop();

                auto feed_resolutor             = InternalFeedResolutor{};
                feed_resolutor.channel          = this->channel;

                size_t trimmed_mailbox_feed_cap = std::min(std::min(this->mailbox_feed_cap, dg::network_kernel_mailbox::max_consume_size()), static_cast<size_t>(request_vec.size()));
                size_t feeder_allocation_cost   = dg::network_producer_consumer::delvrsrv_allocation_cost(&feed_resolutor, trimmed_mailbox_feed_cap);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> feeder_mem(feeder_allocation_cost);
                auto feeder                     = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_open_preallocated_raiihandle(&feed_resolutor, trimmed_mailbox_feed_cap, feeder_mem.get()));

                for (model::InternalRequest& request: request_vec){
                    std::expected<ResourceAddress, exception_t> addr = dg::network_rest_frame::utility::deserialize_resource_address(request.request.requestee_uri);

                    if (!addr.has_value()){
                        dg::network_log_stackdump::error_fast_optional(dg::network_exception::verbose(addr.error()));
                        continue;
                    }

                    std::expected<dg::string, exception_t> bstream = dg::network_exception::to_cstyle_function(dg::network_compact_serializer::dgstd_serialize<dg::string, model::InternalRequest>)(request, model::INTERNAL_REQUEST_SERIALIZATION_SECRET);

                    if (!bstream.has_value()){
                        dg::network_log_stackdump::error_fast_optional(dg::network_exception::verbose(bstream.error()));
                        continue;
                    }

                    auto feed_arg       = dg::network_kernel_mailbox::MailBoxArgument{.to       = addr.value().remote_addr,
                                                                                      .content  = std::move(bstream.value())};

                    dg::network_producer_consumer::delvrsrv_deliver(feeder.get(), std::move(feed_arg));
                }

                return true;
            }

        private:

            struct InternalFeedResolutor: dg::network_producer_consumer::ConsumerInterface<dg::network_kernel_mailbox::MailBoxArgument>{

                uint32_t channel;

                void push(std::move_iterator<dg::network_kernel_mailbox::MailBoxArgument *> mailbox_arg, size_t sz) noexcept{

                    dg::network_stack_allocation::NoExceptAllocation<exception_t[]> exception_arr(sz);
                    dg::network_kernel_mailbox::send(mailbox_arg, sz, exception_arr.get());

                    for (size_t i = 0u; i < sz; ++i){
                        if (dg::network_exception::is_failed(exception_arr[i])){
                            dg::network_log_stackdump::error_fast_optional(dg::network_exception::verbose(exception_arr[i]));
                        }
                    }
                }
            };
    };

    //clear
    class ExpiryWorker: public virtual dg::network_concurrency::WorkerInterface{

        private:

            std::shared_ptr<TicketControllerInterface> ticket_controller;
            std::shared_ptr<TicketTimeoutManagerInterface> ticket_timeout_manager;
            size_t timeout_consume_sz;
            size_t ticketcontroller_observer_steal_cap;
            size_t busy_timeout_consume_sz;  

        public:

            ExpiryWorker(std::shared_ptr<TicketControllerInterface> ticket_controller,
                         std::shared_ptr<TicketTimeoutManagerInterface> ticket_timeout_manager,
                         size_t timeout_consume_sz,
                         size_t ticketcontroller_observer_steal_cap,
                         size_t busy_timeout_consume_sz) noexcept: ticket_controller(std::move(ticket_controller)),
                                                                   ticket_timeout_manager(std::move(ticket_timeout_manager)),
                                                                   timeout_consume_sz(timeout_consume_sz),
                                                                   ticketcontroller_observer_steal_cap(ticketcontroller_observer_steal_cap),
                                                                   busy_timeout_consume_sz(busy_timeout_consume_sz){}

            bool run_one_epoch() noexcept{

                size_t expired_ticket_arr_cap       = this->timeout_consume_sz;
                size_t expired_ticket_arr_sz        = {};
                dg::network_stack_allocation::NoExceptAllocation<model::ticket_id_t[]> expired_ticket_arr(expired_ticket_arr_cap);
                this->ticket_timeout_manager->get_expired_ticket(expired_ticket_arr.get(), expired_ticket_arr_sz, expired_ticket_arr_cap);

                auto feed_resolutor                 = InternalFeedResolutor{};
                feed_resolutor.ticket_controller    = this->ticket_controller.get();

                size_t trimmed_observer_steal_cap   = std::min(this->ticketcontroller_observer_steal_cap, expired_ticket_arr_sz);
                size_t feeder_allocation_cost       = dg::network_producer_consumer::delvrsrv_allocation_cost(&feed_resolutor, trimmed_observer_steal_cap);
                dg::network_stack_allocation::NoExceptRawAllocation<char[]> feeder_mem(feeder_allocation_cost);
                auto feeder                         = dg::network_exception_handler::nothrow_log(dg::network_producer_consumer::delvrsrv_open_preallocated_raiihandle(&feed_resolutor, trimmed_observer_steal_cap, feeder_mem.get()));

                for (size_t i = 0u; i < expired_ticket_arr_sz; ++i){
                    dg::network_producer_consumer::delvrsrv_deliver(feeder.get(), expired_ticket_arr[i]);
                }

                return expired_ticket_arr_sz >= this->busy_timeout_consume_sz;
            }

        private:

            struct InternalFeedResolutor: dg::network_producer_consumer::ConsumerInterface<model::ticket_id_t>{

                TicketControllerInterface * ticket_controller;

                void push(std::move_iterator<model::ticket_id_t *> ticket_id_arr, size_t ticket_id_arr_sz) noexcept{

                    model::ticket_id_t * base_ticket_id_arr = ticket_id_arr.base();
                    dg::network_stack_allocation::NoExceptAllocation<std::expected<std::add_pointer_t<ResponseObserverInterface>, exception_t>[]> stolen_response_observer_arr(ticket_id_arr_sz);

                    this->ticket_controller->steal_observer(base_ticket_id_arr, ticket_id_arr_sz, stolen_response_observer_arr.get());

                    for (size_t i = 0u; i < ticket_id_arr_sz; ++i){
                        if (!stolen_response_observer_arr[i].has_value()){
                            continue;
                        }

                        stdx::safe_ptr_access(stolen_response_observer_arr[i].value())->deferred_memory_ordering_fetch(std::unexpected(dg::network_exception::REST_CLIENTSIDE_TIMEOUT));
                    }

                    std::atomic_thread_fence(std::memory_order_release);
                    //we'd like to have a signal fence std::memory_order_seq_cst just for our sake of safety
                    std::atomic_signal_fence(std::memory_order_seq_cst);

                    for (size_t i = 0u; i < ticket_id_arr_sz; ++i){
                        if (!stolen_response_observer_arr[i].has_value()){
                            continue;
                        }

                        stdx::safe_ptr_access(stolen_response_observer_arr[i].value())->deferred_memory_ordering_fetch_close();
                    }
                }
            };
    };

    //clear
    class RestController: public virtual RestControllerInterface{

        private:

            std::vector<dg::network_concurrency::daemon_raii_handle_t> daemon_vec;
            std::shared_ptr<RequestContainerInterface> request_container;
            std::shared_ptr<TicketControllerInterface> ticket_controller;
            std::shared_ptr<TicketTimeoutManagerInterface> ticket_timeout_manager;
            std::unique_ptr<RequestIDGeneratorInterface> request_id_generator;
            stdx::hdi_container<size_t> max_consume_per_load;

        public:

            using self = RestController;

            RestController(std::vector<dg::network_concurrency::daemon_raii_handle_t> daemon_vec,
                           std::shared_ptr<RequestContainerInterface> request_container,
                           std::shared_ptr<TicketControllerInterface> ticket_controller,
                           std::shared_ptr<TicketTimeoutManagerInterface> ticket_timeout_manager,
                           std::unique_ptr<RequestIDGeneratorInterface> request_id_generator,
                           stdx::hdi_container<size_t> max_consume_per_load) noexcept: daemon_vec(std::move(daemon_vec)),
                                                                                       request_container(std::move(request_container)),
                                                                                       ticket_controller(std::move(ticket_controller)),
                                                                                       ticket_timeout_manager(std::move(ticket_timeout_manager)),
                                                                                       request_id_generator(std::move(request_id_generator)),
                                                                                       max_consume_per_load(std::move(max_consume_per_load)){}

            auto request(model::ClientRequest&& client_request) noexcept -> std::expected<std::unique_ptr<ResponseInterface>, exception_t>{

                std::expected<std::unique_ptr<BatchResponseInterface>, exception_t> resp = this->batch_request(std::make_move_iterator(std::addressof(client_request)), 1u); //alright, this might not be defined

                if (!resp.has_value()){
                    return std::unexpected(resp.error());
                }

                return self::internal_make_single_response(static_cast<std::unique_ptr<BatchResponseInterface>&&>(resp.value()));
            }

            //alright, we'll do debugging
            //we'll analyze the success path
            //then we'll analyze the failed paths, their exit points and the effects on the component internal states

            auto batch_request(std::move_iterator<model::ClientRequest *> client_request_arr, size_t sz) noexcept -> std::expected<std::unique_ptr<BatchResponseInterface>, exception_t>{

                //the code is not hard, yet it is extremely easy to leak resources
                //let's not abort for external interfaces

                if (sz > this->max_consume_size()){
                    return std::unexpected(dg::network_exception::MAX_CONSUME_SIZE_EXCEEDED);
                }

                if (sz == 0u){
                    return std::unexpected(dg::network_exception::INVALID_ARGUMENT); //invalid argument, internal states are not affected, no leaks
                }

                model::ClientRequest * base_client_request_arr = client_request_arr.base();
                std::chrono::nanoseconds max_timeout_dur = this->ticket_timeout_manager->max_clockin_dur(); 

                for (size_t i = 0u; i < sz; ++i){
                    if (base_client_request_arr[i].client_timeout_dur > max_timeout_dur){
                        return std::unexpected(dg::network_exception::REST_INVALID_TIMEOUT); //failed to meet timeout preconds, close tickets by response + release_wait_responsbiility of response + deallocate response resources
                    }
                }

                dg::network_stack_allocation::NoExceptAllocation<model::ticket_id_t[]> ticket_id_arr(sz);
                exception_t err = this->ticket_controller->open_ticket(sz, ticket_id_arr.get()); //open the ticket

                if (dg::network_exception::is_failed(err)){
                    return std::unexpected(err); //failed to open tickets, no actions
                }

                auto ticket_resource_grd = stdx::resource_guard([&]() noexcept{
                    this->ticket_controller->close_ticket(ticket_id_arr.get(), sz);
                });

                std::expected<std::unique_ptr<InternalBatchResponse>, exception_t> response = self::internal_make_batch_request_response(sz, ticket_id_arr.get(), this->ticket_controller); //open batch_response associated with the tickets, take ticket responsibility

                if (!response.has_value()){
                    return std::unexpected(response.error()); //failed to open response, close the tickets 
                }

                ticket_resource_grd.release(); //ticket responsibility tranferred -> internal_batch_response

                auto response_resource_grd = stdx::resource_guard([&]() noexcept{
                    response.value()->release_response_wait_responsibility();
                });

                dg::network_stack_allocation::NoExceptAllocation<std::add_pointer_t<ResponseObserverInterface>[]> response_observer_arr(sz);
                dg::network_stack_allocation::NoExceptAllocation<std::expected<bool, exception_t>[]> response_observer_exception_arr(sz);

                for (size_t i = 0u; i < sz; ++i){
                    response_observer_arr[i] = dg::network_exception_handler::nothrow_log(response.value()->get_observer(i));
                }

                this->ticket_controller->assign_observer(ticket_id_arr.get(), sz, response_observer_arr.get(), response_observer_exception_arr.get()); //bind observers -> ticket_controller to listen for responses

                for (size_t i = 0u; i < sz; ++i){
                    if (!response_observer_exception_arr[i].has_value()){
                        return std::unexpected(response_observer_exception_arr[i].error()); //failed to bind observers, close tickets by response + release_wait_responsbiility of response + deallocate response resources
                    }

                    dg::network_exception_handler::dg_assert(response_observer_exception_arr[i].value());
                }

                std::expected<dg::vector<model::InternalRequest>, exception_t> pushing_container = this->internal_make_internal_request(std::make_move_iterator(base_client_request_arr), ticket_id_arr.get(), sz);

                if (!pushing_container.has_value()){
                    return std::unexpected(pushing_container.error()); //failed to create a pushed container, client_request_arr remains intact, close tickets by response + release_wait_responsibility of response + deallocate response resources
                }

                exception_t push_err = this->request_container->push(static_cast<dg::vector<model::InternalRequest>&&>(pushing_container.value())); //push the outbound request

                if (dg::network_exception::is_failed(push_err)){
                    this->internal_rollback_client_request(base_client_request_arr, std::move(pushing_container.value()));
                    return std::unexpected(push_err); //failed to push thru the container, ticket_id is not referenced by other components, base_client_request_arr is not intact, reverse the operation, close tickets + release response wait responsibility
                }

                //thru

                dg::network_stack_allocation::NoExceptAllocation<ClockInArgument[]> clockin_arr(sz);
                dg::network_stack_allocation::NoExceptAllocation<exception_t[]> clockin_exception_arr(sz);

                for (size_t i = 0u; i < sz; ++i){
                    clockin_arr[i] = ClockInArgument{.clocked_in_ticket = ticket_id_arr[i], 
                                                     .expiry_dur        = base_client_request_arr[i].client_timeout_dur};
                }

                this->ticket_timeout_manager->clock_in(clockin_arr.get(), sz, clockin_exception_arr.get()); //clock in the tickets to rescue

                for (size_t i = 0u; i < sz; ++i){
                    if (dg::network_exception::is_failed(clockin_exception_arr[i])){
                        dg::network_log_stackdump::critical(dg::network_exception::verbose(clockin_exception_arr[i])); //unable to fail, resource leaks + deadlock otherwise, very dangerous, rather terminate
                        std::abort();
                    }
                }

                //

                response_resource_grd.release();

                return std::unique_ptr<BatchResponseInterface>(std::move(response.value()));
            }

            auto get_designated_request_id(size_t request_id_sz, RequestID * out_request_id_arr) noexcept -> exception_t{

                if (request_id_sz > this->max_consume_size()){
                    return dg::network_exception::MAX_CONSUME_SIZE_EXCEEDED;
                }

                return this->request_id_generator->get(request_id_sz, out_request_id_arr);
            } 

            auto max_consume_size() noexcept -> size_t{

                return this->max_consume_per_load.value;
            }

        private:

            class InternalBatchResponse: public virtual BatchResponseInterface{

                private:

                    std::unique_ptr<BatchRequestResponse> base;
                    std::unique_ptr<model::ticket_id_t[]> ticket_id_arr;
                    size_t ticket_id_arr_sz;
                    std::shared_ptr<TicketControllerInterface> ticket_controller;
                    bool ticket_release_responsibility;

                public:

                    //defer construction -> factory, all deferred constructions are marked as noexcept to avoid leaks, such is malloc() -> inplace -> return, fails can only happen at malloc 

                    InternalBatchResponse(std::unique_ptr<BatchRequestResponse> base,
                                          std::unique_ptr<model::ticket_id_t[]> ticket_id_arr,
                                          size_t ticket_id_arr_sz,
                                          std::shared_ptr<TicketControllerInterface> ticket_controller,
                                          bool ticket_release_responsibility) noexcept: base(std::move(base)),
                                                                                        ticket_id_arr(std::move(ticket_id_arr)),
                                                                                        ticket_id_arr_sz(ticket_id_arr_sz),
                                                                                        ticket_controller(std::move(ticket_controller)),
                                                                                        ticket_release_responsibility(ticket_release_responsibility){}

                    ~InternalBatchResponse() noexcept{

                        this->close();
                    }

                    auto test_or_subscribe(std::unique_ptr<ResponseOnReadyObserverInterface> observer) noexcept -> std::expected<bool, exception_t>{

                        return this->base->test_or_subscribe(std::move(observer));
                    }

                    auto test_or_subscribe_sp(std::shared_ptr<ResponseOnReadyObserverInterface> observer) noexcept -> std::expected<bool, exception_t>{

                        return this->base->test_or_subscribe_sp(std::move(observer));
                    }

                    auto response() noexcept -> std::expected<dg::vector<std::expected<Response, exception_t>>, exception_t>{

                        auto rs = this->base->response();
                        std::atomic_signal_fence(std::memory_order_seq_cst);
                        this->release_ticket();
                        return rs;
                    }

                    auto response_size() const noexcept -> size_t{

                        return this->base->response_size();
                    }

                    auto get_observer(size_t idx) noexcept -> std::expected<std::add_pointer_t<ResponseObserverInterface>, exception_t>{

                        return this->base->get_observer(idx);
                    }

                    void release_response_wait_responsibility() noexcept{

                        this->base->release_response_wait_responsibility();
                    }

                    void release_ticket_release_responsibility() noexcept{

                        this->ticket_release_responsibility = false;
                    }

                    void close() noexcept{

                        this->base->wait_response();
                        std::atomic_signal_fence(std::memory_order_seq_cst); //not really necessary
                        this->release_ticket();
                    }

                private:

                    void release_ticket() noexcept{

                        auto task = [](InternalBatchResponse * self_obj) noexcept{
                            if (!self_obj->ticket_release_responsibility){
                                return;
                            }

                            self_obj->ticket_controller->close_ticket(self_obj->ticket_id_arr.get(), self_obj->ticket_id_arr_sz);                        
                            self_obj->ticket_release_responsibility = false;
                        };

                        stdx::noipa_do_task(task, this); //this is incredibly hard to get right
                    }
            };

            class InternalSingleResponse: public virtual ResponseInterface{

                private:

                    std::unique_ptr<BatchResponseInterface> base;
                
                public:

                    InternalSingleResponse(std::unique_ptr<BatchResponseInterface> base) noexcept: base(std::move(base)){}

                    auto test_or_subscribe(std::unique_ptr<ResponseOnReadyObserverInterface> observer) noexcept -> std::expected<bool, exception_t>{

                        return this->base->test_or_subscribe(std::move(observer));
                    }

                    auto test_or_subscribe_sp(std::shared_ptr<ResponseOnReadyObserverInterface> observer) noexcept -> std::expected<bool, exception_t>{

                        return this->base->test_or_subscribe_sp(std::move(observer));
                    }

                    auto response() noexcept -> std::expected<Response, exception_t>{

                        auto rs = this->base->response();

                        if (!rs.has_value()){
                            return std::unexpected(rs.error());
                        }

                        if constexpr(DEBUG_MODE_FLAG){
                            if (rs->size() != 1u){
                                dg::network_log_stackdump::critical(dg::network_exception::verbose(dg::network_exception::INTERNAL_CORRUPTION));
                                std::abort();
                            }
                        }

                        static_assert(std::is_nothrow_move_constructible_v<Response>);
                        return std::expected<Response, exception_t>(std::move(rs->front()));
                    }
            };

            static auto internal_make_batch_request_response(size_t request_sz, ticket_id_t * ticket_id_arr,
                                                             std::shared_ptr<TicketControllerInterface> ticket_controller) noexcept -> std::expected<std::unique_ptr<InternalBatchResponse>, exception_t>{

                if (request_sz == 0u){
                    return std::unexpected(dg::network_exception::INVALID_ARGUMENT);
                }

                if (ticket_controller == nullptr){
                    return std::unexpected(dg::network_exception::INVALID_ARGUMENT);
                }

                std::expected<std::unique_ptr<BatchRequestResponse>, exception_t> base = dg::network_rest_frame::client_impl1::make_batch_request_response(request_sz);

                if (!base.has_value()){
                    return std::unexpected(base.error());
                }

                auto resource_grd = stdx::resource_guard([rptr = base.value().get()]() noexcept{
                    rptr->release_response_wait_responsibility();
                });

                std::expected<std::unique_ptr<model::ticket_id_t[]>, exception_t> cpy_ticket_id_arr = dg::network_allocation::cstyle_make_unique<ticket_id_t[]>(request_sz);

                if (!cpy_ticket_id_arr.has_value()){
                    return std::unexpected(cpy_ticket_id_arr.error());
                }

                std::copy(stdx::safe_ptr_access(ticket_id_arr), std::next(stdx::safe_ptr_access(ticket_id_arr), request_sz), cpy_ticket_id_arr.value().get());
                std::expected<std::unique_ptr<InternalBatchResponse>, exception_t> rs = dg::network_allocation::cstyle_make_unique<InternalBatchResponse>(std::move(base.value()), 
                                                                                                                                                          std::move(cpy_ticket_id_arr.value()), 
                                                                                                                                                          request_sz, 
                                                                                                                                                          std::move(ticket_controller), 
                                                                                                                                                          true);

                if (!rs.has_value()){
                    return std::unexpected(rs.error());
                }

                resource_grd.release();

                return rs;
            }

            static auto internal_make_single_response(std::unique_ptr<BatchResponseInterface>&& base) noexcept -> std::expected<std::unique_ptr<ResponseInterface>, exception_t>{

                return dg::network_allocation::cstyle_make_unique<InternalSingleResponse>(static_cast<std::unique_ptr<BatchResponseInterface>&&>(base));
            }

            static auto internal_make_internal_request(std::move_iterator<model::ClientRequest *> request_arr, ticket_id_t * ticket_id_arr, size_t request_arr_sz) noexcept -> std::expected<dg::vector<model::InternalRequest>, exception_t>{

                model::ClientRequest * base_request_arr                             = request_arr.base();
                std::expected<dg::vector<model::InternalRequest>, exception_t> rs   = dg::network_exception::cstyle_initialize<dg::vector<model::InternalRequest>>(request_arr_sz);

                if (!rs.has_value()){
                    return std::unexpected(rs.error());
                }

                for (size_t i = 0u; i < request_arr_sz; ++i){
                    static_assert(std::is_nothrow_move_assignable_v<model::InternalRequest>);
                    static_assert(std::is_nothrow_move_constructible_v<dg::string>);

                    rs.value()[i] = InternalRequest{.request    = Request{.requestee_uri                = std::move(base_request_arr[i].requestee_uri),
                                                                          .requestor                    = std::move(base_request_arr[i].requestor),
                                                                          .payload                      = std::move(base_request_arr[i].payload),
                                                                          .payload_serialization_format = std::move(base_request_arr[i].payload_serialization_format)},

                                                    .ticket_id                  = ticket_id_arr[i],
                                                    .has_unique_response        = base_request_arr[i].designated_request_id.has_value(),
                                                    .client_request_cache_id    = base_request_arr[i].designated_request_id.has_value() ? std::optional<cache_id_t>(request_id_to_cache_id(base_request_arr[i].designated_request_id.value()))
                                                                                                                                        : std::optional<cache_id_t>(std::nullopt),
                                                    .server_abs_timeout         = base_request_arr[i].server_abs_timeout};
                }

                return rs;
            }

            static void internal_rollback_client_request(model::ClientRequest * client_request_arr, dg::vector<model::InternalRequest>&& internal_request_arr) noexcept{

                for (size_t i = 0u; i < internal_request_arr.size(); ++i){
                    client_request_arr[i].requestee_uri                 = std::move(internal_request_arr[i].request.requestee_uri);
                    client_request_arr[i].requestor                     = std::move(internal_request_arr[i].request.requestor);
                    client_request_arr[i].payload                       = std::move(internal_request_arr[i].request.payload);
                    client_request_arr[i].payload_serialization_format  = std::move(internal_request_arr[i].request.payload_serialization_format);
                }
            }
    };

    //clear
    class DistributedRestController: public virtual RestControllerInterface{

        private:

            std::unique_ptr<std::unique_ptr<RestControllerInterface>[]> rest_controller_arr;
            size_t pow2_rest_controller_arr_sz;
            stdx::hdi_container<size_t> max_consume_per_load;

        public:

            DistributedRestController(std::unique_ptr<std::unique_ptr<RestControllerInterface>[]> rest_controller_arr,
                                      size_t pow2_rest_controller_arr_sz,
                                      stdx::hdi_container<size_t> max_consume_per_load) noexcept: rest_controller_arr(std::move(rest_controller_arr)),
                                                                                                  pow2_rest_controller_arr_sz(pow2_rest_controller_arr_sz),
                                                                                                  max_consume_per_load(std::move(max_consume_per_load)){} 

            auto request(model::ClientRequest&& request) noexcept -> std::expected<std::unique_ptr<ResponseInterface>, exception_t>{

                size_t random_clue  = dg::network_randomizer::randomize_int<size_t>();
                size_t idx          = random_clue & (this->pow2_rest_controller_arr_sz - 1u);

                return this->rest_controller_arr[idx]->request(static_cast<model::ClientRequest&&>(request));
            }

            auto batch_request(std::move_iterator<model::ClientRequest *> request_arr, size_t request_arr_sz) noexcept -> std::expected<std::unique_ptr<BatchResponseInterface>, exception_t>{

                if (request_arr_sz > this->max_consume_size()){
                    return std::unexpected(dg::network_exception::MAX_CONSUME_SIZE_EXCEEDED);
                }

                size_t random_clue  = dg::network_randomizer::randomize_int<size_t>();
                size_t idx          = random_clue & (this->pow2_rest_controller_arr_sz - 1u);

                return this->rest_controller_arr[idx]->batch_request(request_arr, request_arr_sz);
            }

            //let's not overcomplicate, such is request_id is just a unique identifier for a Request, and does not hold another special bookkept semantic meaning or special dispatch 
            //we can't really implement a feature that is not going to be used, and actually slow down the code, it's often bad design, this is bad design the fact that we are asking the question

            auto get_designated_request_id(size_t request_id_sz, RequestID * out_request_id_arr) noexcept -> exception_t{

                if (request_id_sz > this->max_consume_size()){
                    return dg::network_exception::MAX_CONSUME_SIZE_EXCEEDED;
                }

                size_t random_clue  = dg::network_randomizer::randomize_int<size_t>();
                size_t idx          = random_clue & (this->pow2_rest_controller_arr_sz - 1u);

                return this->rest_controller_arr[idx]->get_designated_request_id(request_id_sz, out_request_id_arr);
            }

            auto max_consume_size() noexcept -> size_t{

                return this->max_consume_per_load.value;
            }
    };
}

#endif