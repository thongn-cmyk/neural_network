#ifndef __DG_NETWORK_REST_FRAME_H__
#define __DG_NETWORK_REST_FRAME_H__ 

//define HEADER_CONTROL 12

#include <coroutine_subsystem/coroutine_x.h>
// #include <cron_subsystem/cron_subsystem.h>

#include <stdint.h>
#include <stdlib.h>
#include "network_allocation.h"
#include "network_std_container.h"
#include <chrono>
#include "network_exception.h"
#include "stdx.h"
#include "network_log.h"
#include "network_compact_serializer.h"
#include <variant>
#include "network_kernel_mailbox.h"
#include "network_producer_consumer.h"
#include "network_stack_allocation.h"
#include "network_ticket_timeout_manager.h"
#include "network_cron.h"

namespace dg_sock::network_rest_frame::model
{
    //

    using ticket_id_t   = uint64_t; //I've thought long and hard, it's better to do bitshift, because the otherwise would be breaking single responsibilities, breach of extensions
    using clock_id_t    = uint64_t; 

    static inline constexpr uint32_t INTERNAL_REQUEST_SERIALIZATION_SECRET  = 3312354321ULL;
    static inline constexpr uint32_t INTERNAL_RESPONSE_SERIALIZATION_SECRET = 3554488158ULL;
    static inline constexpr std::string_view REST_FRAME_VERSION_SUFFX       = std::string_view("REST_FRAME_V1"); //this is to actually solve the serialization problem, we can weed out the version problems, for the bad packets, we are guaranteed to filter that using the hashed value 

    using ipv6_storage_t        = std::array<char, 8u>;
    using ipv4_storage_t        = std::array<char, 4u>;
    using ip_storage_t          = std::variant<ipv4_storage_t, ipv6_storage_t>; 
    using native_id_storage_t   = std::array<char, 8u>; 
    using Address               = dg_sock::network_kernel_mailbox::Address;

    struct CacheID
    {
        ip_storage_t ip;
        native_id_storage_t native_cache_id; 

        template <class Reflector>
        constexpr void dg_reflect(const Reflector& reflector) const noexcept
        {
            reflector(ip, native_cache_id);
        }

        template <class Reflector>
        constexpr void dg_reflect(const Reflector& reflector) noexcept
        {
            reflector(ip, native_cache_id);
        }
    };

    using cache_id_t    = CacheID; 

    //
    struct RequestID
    {
        ip_storage_t ip;
        native_id_storage_t native_request_id;

        template <class Reflector>
        constexpr void dg_reflect(const Reflector& reflector) const noexcept
        {
            reflector(ip, native_request_id);
        }

        template <class Reflector>
        constexpr void dg_reflect(const Reflector& reflector) noexcept
        {
            reflector(ip, native_request_id);
        }
    };

    using request_id_t = RequestID;

    struct ResourceAddress
    {
        Address remote_addr;
        dg_sock::string resource_addr;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(remote_addr, dg_sock::network_compact_serializer::wrap_container<uint16_t>(resource_addr));
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(remote_addr, dg_sock::network_compact_serializer::wrap_container<uint16_t>(resource_addr));
        }
    };

    using Url = ResourceAddress;

    struct ClientRequest
    {
        ResourceAddress requestee_url;
        Address requestor;

        dg_sock::string payload;
        dg_sock::string payload_serialization_format;

        std::chrono::nanoseconds client_timeout_dur;
        std::optional<std::chrono::time_point<std::chrono::utc_clock>> server_abs_timeout; //this is hard to solve, we can be stucked in a pipe and actually stay there forever, abs_timeout only works for post the transaction, which is already too late, I dont know of the way to do this correctly
        std::optional<request_id_t> designated_request_id; 

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(requestee_url, requestor,
                      payload, payload_serialization_format,
                      client_timeout_dur,
                      server_abs_timeout,
                      designated_request_id);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(requestee_url, requestor,
                      payload, payload_serialization_format,
                      client_timeout_dur,
                      server_abs_timeout,
                      designated_request_id);
        }
    };

    struct Request
    {
        ResourceAddress requestee_url;
        Address requestor;

        dg_sock::string payload;
        dg_sock::string payload_serialization_format;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(requestee_url, requestor,
                      payload, payload_serialization_format);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(requestor, requestor,
                      payload, payload_serialization_format);
        }
    };

    struct Response
    {
        dg_sock::string response;
        dg_sock::string response_serialization_format;

        exception_t err_code;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(response, response_serialization_format,
                      err_code);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(response, response_serialization_format,
                      err_code);
        }
    };

    using ClientResponse = Response;

    struct InternalRequest
    {
        Request request;
        ticket_id_t ticket_id;

        bool has_unique_response;
        std::optional<cache_id_t> client_request_cache_id;
        std::optional<std::chrono::time_point<std::chrono::utc_clock>> server_abs_timeout;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(request, ticket_id, has_unique_response,
                      client_request_cache_id, server_abs_timeout);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(request, ticket_id, has_unique_response,
                      client_request_cache_id, server_abs_timeout);
        }
    };

    struct InternalResponse
    {
        std::expected<Response, exception_t> response;
        ticket_id_t ticket_id;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(response, ticket_id);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(response, ticket_id);
        }
    };

    using ClockInArgument = dg_sock::ticket_system::ClockInArgument<ticket_id_t>;
}

namespace dg_sock::network_rest_frame::server
{
    using namespace dg_sock::network_rest_frame::model;

    struct CacheControllerInterface
    {
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

    struct InfiniteCacheControllerInterface
    {
        virtual ~InfiniteCacheControllerInterface() noexcept = default;

        virtual void get_cache(cache_id_t * cache_id_arr, size_t sz, std::expected<std::optional<Response>, exception_t> * response_arr) noexcept = 0;
        virtual void insert_cache(cache_id_t * cache_id_arr, std::move_iterator<Response *> response_arr, size_t sz, std::expected<bool, exception_t> * rs_arr) noexcept = 0; //this is probably the most debatable in the C++ world, yet it has unique applications in this particular component, that's why we want to reimplement our containers literally every time
        virtual auto max_response_size() const noexcept -> size_t = 0;
        virtual auto max_consume_size() noexcept -> size_t = 0;
    };

    struct CacheUniqueWriteControllerInterface
    {
        virtual ~CacheUniqueWriteControllerInterface() noexcept = default;

        virtual void thru(cache_id_t * cache_id_arr, size_t sz, std::expected<bool, exception_t> * rs_arr) noexcept = 0;
        virtual void contains(cache_id_t * cache_id_arr, size_t sz, bool * rs_arr) noexcept = 0;
        virtual void clear() noexcept = 0;
        virtual auto size() const noexcept -> size_t = 0;
        virtual auto capacity() const noexcept -> size_t = 0;
        virtual auto max_consume_size() noexcept -> size_t = 0;
    };

    struct InfiniteCacheUniqueWriteControllerInterface
    {
        virtual ~InfiniteCacheUniqueWriteControllerInterface() noexcept = default;

        virtual void thru(cache_id_t * cache_id_arr, size_t sz, std::expected<bool, exception_t> * rs_arr) noexcept = 0;
        virtual auto max_consume_size() noexcept -> size_t = 0;
    };

    struct CacheUniqueWriteTrafficControllerInterface
    {
        virtual ~CacheUniqueWriteTrafficControllerInterface() noexcept = default;

        virtual auto thru(size_t sz) noexcept -> std::expected<bool, exception_t> = 0;
        virtual auto thru_capacity() const noexcept -> size_t = 0;
        virtual void reset() noexcept = 0;
    };

    struct UpdatableInterface
    {
        virtual ~UpdatableInterface() noexcept = default;

        virtual void update() noexcept = 0;
    };

    struct RequestHandlerInterface
    {
        using Request   = model::Request;
        using Response  = model::Response;

        virtual ~RequestHandlerInterface() noexcept = default;

        virtual void handle(std::move_iterator<Request *> request_arr, size_t request_arr_sz, Response * response_arr) noexcept = 0;
        virtual auto max_consume_size() noexcept -> size_t = 0;
    };

    struct OneRequestHandlerInterface
    {
        using Request   = model::Request;
        using Response  = model::Response;

        virtual ~OneRequestHandlerInterface() noexcept = default;

        virtual auto handle(const Request& request) -> Response = 0;
    };

    struct RequestHandlerDictionaryInterface
    {
        virtual ~RequestHandlerDictionaryInterface() noexcept = default;

        virtual auto add_resolver(std::string_view resource_addr, std::shared_ptr<RequestHandlerInterface> request_handler) noexcept -> exception_t = 0;
        virtual void remove_resolver(std::string_view resource_addr) noexcept = 0;
        virtual auto get_resolver(std::string_view resource_addr) noexcept -> std::shared_ptr<RequestHandlerInterface> = 0;
    };

    struct RequestHandlerRetrieverInterface
    {
        virtual ~RequestHandlerRetrieverInterface() noexcept = default;

        virtual auto get_resolver(std::string_view resource_addr) noexcept -> std::add_pointer_t<RequestHandlerInterface> = 0;
    };

    struct RequestFiltererInterface
    {
        virtual ~RequestFiltererInterface() noexcept = default;
        virtual auto thru(const Request& request) -> exception_t = 0;
    };
    
    struct RequestFiltererDictionaryInterface
    {
        virtual ~RequestFiltererDictionaryInterface() noexcept = default;

        virtual auto add_filterer(std::string_view resource_addr, std::shared_ptr<RequestFiltererInterface> request_filterer) noexcept -> exception_t = 0;
        virtual void remove_filterer(std::string_view resource_addr) noexcept = 0;
        virtual auto get_filterer(std::string_view resource_addr) noexcept -> std::shared_ptr<RequestFiltererInterface> = 0;
    };

    struct RequestFiltererRetrieverInterface
    {
        virtual ~RequestFiltererRetrieverInterface() noexcept = default;

        virtual auto get_filterer(std::string_view resource_addr) noexcept -> std::add_pointer_t<RequestFiltererInterface> = 0;
    };
}

namespace dg_sock::network_rest_frame::client
{
    using namespace dg_sock::network_rest_frame::model;

    //we'll solve this later

    struct UpdatableInterface
    {
        virtual ~UpdatableInterface() noexcept = default;

        virtual void update() noexcept = 0;
    };

    struct RequestIDGeneratorInterface
    {
        virtual ~RequestIDGeneratorInterface() noexcept = default;

        virtual auto get(size_t request_id_sz, RequestID * request_id_arr) noexcept -> exception_t = 0;
    };

    struct ResponseObserverInterface
    {
        virtual ~ResponseObserverInterface() noexcept = default;

        virtual void update(std::expected<Response, exception_t> response) noexcept = 0;
    };

    struct BatchResponseInterface
    {
        virtual ~BatchResponseInterface() noexcept = default;

        virtual auto is_completed() noexcept -> bool = 0;
        virtual auto response() noexcept -> std::expected<dg_sock::vector<std::expected<Response, exception_t>>, exception_t> = 0;
        virtual auto response_size() const noexcept -> size_t = 0;
    };

    struct ResponseInterface
    {
        virtual ~ResponseInterface() noexcept = default;

        virtual auto is_completed() noexcept -> bool = 0;
        virtual auto response() noexcept -> std::expected<Response, exception_t> = 0; 
    };

    struct RequestContainerInterface
    {
        virtual ~RequestContainerInterface() noexcept = default;

        virtual auto push(dg_sock::vector<model::InternalRequest>&& request_vec) noexcept -> exception_t = 0;
        virtual auto pop() noexcept -> dg_sock::vector<model::InternalRequest> = 0;
    };

    struct TicketControllerInterface
    {
        virtual ~TicketControllerInterface() noexcept = default;

        virtual auto open_ticket(size_t sz, model::ticket_id_t * rs) noexcept -> exception_t = 0;
        virtual void assign_observer(model::ticket_id_t * ticket_id_arr, size_t sz,
                                     std::move_iterator<std::shared_ptr<ResponseObserverInterface> *> assigning_observer_arr,
                                     std::expected<bool, exception_t> * exception_arr) noexcept = 0;

        virtual void steal_observer(model::ticket_id_t * ticket_id_arr, size_t sz,
                                    std::expected<std::shared_ptr<ResponseObserverInterface>, exception_t> * out_observer_arr) noexcept = 0;

        virtual void close_ticket(model::ticket_id_t * ticket_id_arr, size_t sz) noexcept = 0;
        virtual auto max_consume_size() noexcept -> size_t = 0; 
    };

    using TicketTimeoutManagerInterface = dg_sock::ticket_system::TicketTimeoutManagerInterface<ticket_id_t>;

    struct RestControllerInterface
    {
        virtual ~RestControllerInterface() noexcept = default;

        virtual auto request(model::ClientRequest&&) noexcept -> std::expected<dg_sock::unique_ptr<ResponseInterface>, exception_t> = 0;
        virtual auto batch_request(std::move_iterator<model::ClientRequest *> client_request_arr, size_t client_request_arr_sz) noexcept -> std::expected<dg_sock::unique_ptr<BatchResponseInterface>, exception_t> = 0;
        virtual auto get_designated_request_id(size_t request_id_arr_sz, RequestID * request_id_arr) noexcept -> exception_t = 0;
        virtual auto max_consume_size() noexcept -> size_t = 0;
    };
}

namespace dg_sock::network_rest_frame::server_impl1
{
    using namespace dg_sock::network_rest_frame::server; 

    //clear
    class CacheController: public virtual CacheControllerInterface
    {
        private:

            dg_sock::unordered_unstable_map<cache_id_t, Response> cache_map;
            size_t cache_map_cap;
            size_t max_response_sz;
            size_t max_consume_per_load;

        public:

            CacheController(dg_sock::unordered_unstable_map<cache_id_t, Response> cache_map,
                            size_t cache_map_cap,
                            size_t max_response_sz,
                            size_t max_consume_per_load) noexcept: cache_map(std::move(cache_map)),
                                                                   cache_map_cap(cache_map_cap),
                                                                   max_response_sz(max_response_sz),
                                                                   max_consume_per_load(std::move(max_consume_per_load)){}

            void get_cache(cache_id_t * cache_id_arr, size_t sz, std::expected<std::optional<Response>, exception_t> * rs_arr) noexcept
            {
                for (size_t i = 0u; i < sz; ++i)
                {
                    auto map_ptr = std::as_const(this->cache_map).find(cache_id_arr[i]);

                    if (map_ptr == this->cache_map.end())
                    {
                        rs_arr[i] = std::optional<Response>(std::nullopt);
                    }
                    else
                    {
                        std::expected<Response, exception_t> cpy_response = dg_sock::network_exception::cstyle_initialize<Response>(map_ptr->second);

                        if (!cpy_response.has_value())
                        {
                            rs_arr[i] = std::unexpected(cpy_response.error());
                        }
                        else
                        {
                            static_assert(std::is_nothrow_move_constructible_v<Response> && std::is_nothrow_move_assignable_v<Response>);
                            rs_arr[i] = std::optional<Response>(std::move(cpy_response.value()));
                        }
                    }
                }
            }

            void insert_cache(cache_id_t * cache_id_arr,
                              std::move_iterator<Response *> response_arr, size_t sz,
                              std::expected<bool, exception_t> * rs_arr) noexcept
            {
                if constexpr(DEBUG_MODE_FLAG)
                {
                    if (sz > this->max_consume_size())
                    {
                        dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                Response * base_response_arr = response_arr.base(); 

                for (size_t i = 0u; i < sz; ++i)
                {
                    if (this->cache_map.size() == this->cache_map_cap)
                    {
                        rs_arr[i] = std::unexpected(dg_sock::network_exception::RESOURCE_EXHAUSTION);
                        continue;
                    }

                    if (base_response_arr[i].response.size() > this->max_response_sz)
                    {
                        rs_arr[i] = std::unexpected(dg_sock::network_exception::REST_CACHE_MAX_RESPONSE_SIZE_REACHED);
                        continue;
                    }

                    static_assert(std::is_nothrow_move_constructible_v<Response> && std::is_nothrow_move_assignable_v<Response>);

                    auto insert_token   = std::make_pair(cache_id_arr[i], std::move(base_response_arr[i]));
                    auto [_, status]    = this->cache_map.insert(std::move(insert_token));
                    rs_arr[i]           = status;
                }
            }

            void contains(cache_id_t * cache_id_arr, size_t sz, bool * rs_arr) noexcept
            {
                for (size_t i = 0u; i < sz; ++i)
                {
                    rs_arr[i] = this->cache_map.contains(cache_id_arr[i]);
                }
            }

            auto max_response_size() const noexcept -> size_t
            {
                return this->max_response_sz;
            }

            void clear() noexcept
            {
                this->cache_map.clear();
            }

            auto size() const noexcept -> size_t
            {
                return this->cache_map.size();
            }

            auto capacity() const noexcept -> size_t
            {
                return this->cache_map_cap;
            }

            auto max_consume_size() noexcept -> size_t
            {
                return this->max_consume_per_load;
            }
    };

    //clear
    class MutexControlledCacheController: public virtual CacheControllerInterface
    {
        private:

            std::unique_ptr<CacheControllerInterface> base;
            std::unique_ptr<stdxx::fair_atomic_flag> mtx;

        public:

            MutexControlledCacheController(std::unique_ptr<CacheControllerInterface> base,
                                           std::unique_ptr<stdxx::fair_atomic_flag> mtx) noexcept: base(std::move(base)),
                                                                                      mtx(std::move(mtx)){}

            void get_cache(cache_id_t * cache_id_arr, size_t sz, std::expected<std::optional<Response>, exception_t> * rs_arr) noexcept
            {
                stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(*this->mtx);
                this->base->get_cache(cache_id_arr, sz, rs_arr);
            }

            void insert_cache(cache_id_t * cache_id_arr, std::move_iterator<Response *> response_arr, size_t sz, std::expected<bool, exception_t> * rs_arr) noexcept
            {
                stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(*this->mtx);
                this->base->insert_cache(cache_id_arr, response_arr, sz, rs_arr);
            }

            void clear() noexcept
            {
                stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(*this->mtx);
                this->base->clear();
            }

            void contains(cache_id_t * cache_id_arr, size_t sz, bool * rs_arr) noexcept
            {
                stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(*this->mtx);
                this->base->contains(cache_id_arr, sz, rs_arr);
            }

            auto size() const noexcept -> size_t
            {
                stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(*this->mtx);
                return this->base->size();
            }

            auto capacity() const noexcept -> size_t
            {
                stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(*this->mtx);
                return this->base->capacity();
            }

            auto max_response_size() const noexcept -> size_t
            {
                stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(*this->mtx);
                return this->base->max_response_size(); 
            } 

            auto max_consume_size() noexcept -> size_t{

                stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(*this->mtx);
                return this->base->max_consume_size();
            }
    };

    //clear
    class AdvancedInfiniteCacheController: public virtual InfiniteCacheControllerInterface
    {
        private:

            dg_sock::cyclic_unordered_node_map<cache_id_t, Response> cache_map;
            size_t max_response_sz;
            size_t max_consume_per_load;

        public:

            AdvancedInfiniteCacheController(dg_sock::cyclic_unordered_node_map<cache_id_t, Response> cache_map,
                                            size_t max_response_sz,
                                            size_t max_consume_per_load) noexcept: cache_map(std::move(cache_map)),
                                                                                   max_response_sz(max_response_sz),
                                                                                   max_consume_per_load(max_consume_per_load){}

            void get_cache(cache_id_t * cache_id_arr, size_t sz, std::expected<std::optional<Response>, exception_t> * response_arr) noexcept
            {
                for (size_t i = 0u; i < sz; ++i)
                {
                    auto map_ptr = this->cache_map.find(cache_id_arr[i]);

                    if (map_ptr == this->cache_map.end())
                    {
                        response_arr[i] = std::optional<Response>(std::nullopt);
                        continue;
                    }

                    std::expected<Response, exception_t> response_cpy = dg_sock::network_exception::cstyle_initialize<Response>(map_ptr->second);

                    if (!response_cpy.has_value())
                    {
                        response_arr[i] = std::unexpected(response_cpy.error());
                        continue;
                    }

                    response_arr[i] = std::optional<Response>(std::move(response_cpy.value()));
                }
            }

            void insert_cache(cache_id_t * cache_id_arr,
                              std::move_iterator<Response *> response_arr, size_t sz,
                              std::expected<bool, exception_t> * rs_arr) noexcept
            {
                if constexpr(DEBUG_MODE_FLAG)
                {
                    if (sz > this->max_consume_size())
                    {
                        dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                Response * base_response_arr = response_arr.base();

                for (size_t i = 0u; i < sz; ++i)
                {
                    if (base_response_arr[i].response.size() > this->max_response_size())
                    {
                        rs_arr[i] = std::unexpected(dg_sock::network_exception::REST_CACHE_MAX_RESPONSE_SIZE_REACHED);
                        continue;
                    }

                    auto insert_token   = std::make_pair(cache_id_arr[i], std::move(base_response_arr[i]));
                    auto [_, status]    = this->cache_map.insert(std::move(insert_token));
                    rs_arr[i]           = status;
                }
            }

            auto max_response_size() const noexcept -> size_t
            {
                return this->max_response_sz;
            }

            auto max_consume_size() noexcept -> size_t
            {
                return this->max_consume_per_load;
            }
    };

    //clear
    class MutexControlledInfiniteCacheController: public virtual InfiniteCacheControllerInterface
    {
        private:

            std::unique_ptr<InfiniteCacheControllerInterface> base;
            std::unique_ptr<stdxx::fair_atomic_flag> mtx;

        public:

            MutexControlledInfiniteCacheController(std::unique_ptr<InfiniteCacheControllerInterface> base,
                                                   std::unique_ptr<stdxx::fair_atomic_flag> mtx) noexcept: base(std::move(base)),
                                                                                              mtx(std::move(mtx)){}

            void get_cache(cache_id_t * cache_id_arr, size_t sz, std::expected<std::optional<Response>, exception_t> * rs_arr) noexcept
            {
                stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(*this->mtx);
                this->base->get_cache(cache_id_arr, sz, rs_arr);
            }

            void insert_cache(cache_id_t * cache_id_arr,
                              std::move_iterator<Response *> response_arr, size_t sz,
                              std::expected<bool, exception_t> * rs_arr) noexcept
            {
                stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(*this->mtx);
                this->base->insert_cache(cache_id_arr, response_arr, sz, rs_arr);
            }

            auto max_response_size() const noexcept -> size_t
            {
                stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(*this->mtx);
                return this->base->max_response_size();
            }

            auto max_consume_size() noexcept -> size_t
            {
                stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(*this->mtx);
                return this->base->max_consume_size();
            }
    };

    //clear
    class DistributedCacheController: public virtual InfiniteCacheControllerInterface
    {
        private:

            std::unique_ptr<std::unique_ptr<InfiniteCacheControllerInterface>[]> cache_controller_arr;
            size_t pow2_cache_controller_arr_sz;
            size_t getcache_keyvalue_feed_cap;
            size_t insertcache_keyvalue_feed_cap;
            size_t max_consume_per_load;
            size_t max_response_sz;

        public:

            DistributedCacheController(std::unique_ptr<std::unique_ptr<InfiniteCacheControllerInterface>[]> cache_controller_arr,
                                       size_t pow2_cache_controller_arr_sz,
                                       size_t getcache_keyvalue_feed_cap,
                                       size_t insertcache_keyvalue_feed_cap,
                                       size_t max_consume_per_load,
                                       size_t max_response_sz) noexcept: cache_controller_arr(std::move(cache_controller_arr)),
                                                                         pow2_cache_controller_arr_sz(pow2_cache_controller_arr_sz),
                                                                         getcache_keyvalue_feed_cap(getcache_keyvalue_feed_cap),
                                                                         insertcache_keyvalue_feed_cap(insertcache_keyvalue_feed_cap),
                                                                         max_consume_per_load(std::move(max_consume_per_load)),
                                                                         max_response_sz(max_response_sz){}

            void get_cache(cache_id_t * cache_id_arr, size_t sz, std::expected<std::optional<Response>, exception_t> * rs_arr) noexcept
            {
                auto feed_resolutor                 = InternalGetCacheFeedResolutor{};
                feed_resolutor.cache_controller_arr = this->cache_controller_arr.get();

                size_t trimmed_keyvalue_feed_cap    = std::min(this->getcache_keyvalue_feed_cap, sz);
                size_t feeder_allocation_cost       = dg_sock::network_producer_consumer::delvrsrv_kv_allocation_cost(&feed_resolutor, trimmed_keyvalue_feed_cap);
                dg_sock::network_stack_allocation::NoExceptRawAllocation<char[]> feeder_mem(feeder_allocation_cost);
                auto feeder                         = dg_sock::network_exception_handler::nothrow_log(dg_sock::network_producer_consumer::delvrsrv_kv_open_preallocated_raiihandle(&feed_resolutor, trimmed_keyvalue_feed_cap, feeder_mem.get()));

                for (size_t i = 0u; i < sz; ++i)
                {
                    size_t hashed_cache_id_value    = dg_sock::network_hash::hash_reflectible(cache_id_arr[i]);
                    size_t partitioned_idx          = hashed_cache_id_value & (this->pow2_cache_controller_arr_sz - 1u);
                    auto feed_arg                   = InternalGetCacheFeedArgument
                    {
                        .cache_id   = cache_id_arr[i],
                        .rs_ptr     = std::next(rs_arr, i)
                    };

                    dg_sock::network_producer_consumer::delvrsrv_kv_deliver(feeder.get(), partitioned_idx, feed_arg);
                }
            }

            void insert_cache(cache_id_t * cache_id_arr,
                              std::move_iterator<Response *> response_arr, size_t sz,
                              std::expected<bool, exception_t> * rs_arr) noexcept
            {
                if constexpr(DEBUG_MODE_FLAG)
                {
                    if (sz > this->max_consume_size())
                    {
                        dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                Response * base_response_arr        = response_arr.base();

                auto feed_resolutor                 = InternalCacheInsertFeedResolutor{};
                feed_resolutor.cache_controller_arr = this->cache_controller_arr.get();

                size_t trimmed_keyvalue_feed_cap    = std::min(this->insertcache_keyvalue_feed_cap, sz);
                size_t feeder_allocation_cost       = dg_sock::network_producer_consumer::delvrsrv_kv_allocation_cost(&feed_resolutor, trimmed_keyvalue_feed_cap);
                dg_sock::network_stack_allocation::NoExceptRawAllocation<char[]> feeder_mem(feeder_allocation_cost);
                auto feeder                         = dg_sock::network_exception_handler::nothrow_log(dg_sock::network_producer_consumer::delvrsrv_kv_open_preallocated_raiihandle(&feed_resolutor, trimmed_keyvalue_feed_cap, feeder_mem.get()));

                for (size_t i = 0u; i < sz; ++i)
                {
                    size_t hashed_cache_id_value    = dg_sock::network_hash::hash_reflectible(cache_id_arr[i]);
                    size_t partitioned_idx          = hashed_cache_id_value & (this->pow2_cache_controller_arr_sz - 1u);
                    auto feed_arg                   = InternalCacheInsertFeedArgument
                    {
                        .cache_id     = cache_id_arr[i],
                        .response_ptr = std::make_move_iterator(std::next(base_response_arr, i)),
                        .rs           = std::next(rs_arr, i)
                    };

                    dg_sock::network_producer_consumer::delvrsrv_kv_deliver(feeder.get(), partitioned_idx, std::move(feed_arg));
                }
            }

            auto max_response_size() const noexcept -> size_t
            {
                return this->max_response_sz;
            }

            auto max_consume_size() noexcept -> size_t
            {
                return this->max_consume_per_load;
            }

        private:

            struct InternalGetCacheFeedArgument
            {
                cache_id_t cache_id;
                std::expected<std::optional<Response>, exception_t> * rs_ptr;
            };

            struct InternalGetCacheFeedResolutor: dg_sock::network_producer_consumer::KVConsumerInterface<size_t, InternalGetCacheFeedArgument>
            {
                std::unique_ptr<InfiniteCacheControllerInterface> * cache_controller_arr;

                void push(const size_t& partitioned_idx, std::move_iterator<InternalGetCacheFeedArgument *> data_arr, size_t sz) noexcept
                {
                    InternalGetCacheFeedArgument * base_data_arr = data_arr.base();

                    dg_sock::network_stack_allocation::NoExceptAllocation<cache_id_t[]> cache_id_arr(sz);
                    dg_sock::network_stack_allocation::NoExceptAllocation<std::expected<std::optional<Response>, exception_t>[]> rs_arr(sz);

                    for (size_t i = 0u; i < sz; ++i)
                    {
                        cache_id_arr[i] = base_data_arr[i].cache_id;
                    }

                    this->cache_controller_arr[partitioned_idx]->get_cache(cache_id_arr.get(), sz, rs_arr.get());

                    for (size_t i = 0u; i < sz; ++i)
                    {
                        //we should standardize the move_if_noexcept
                        *base_data_arr[i].rs_ptr = std::move(rs_arr[i]);
                    }
                }
            };

            struct InternalCacheInsertFeedArgument
            {
                cache_id_t cache_id;
                std::move_iterator<Response *> response_ptr;
                std::expected<bool, exception_t> * rs;
            };

            struct InternalCacheInsertFeedResolutor: dg_sock::network_producer_consumer::KVConsumerInterface<size_t, InternalCacheInsertFeedArgument>
            {
                std::unique_ptr<InfiniteCacheControllerInterface> * cache_controller_arr;

                void push(const size_t& partitioned_idx, std::move_iterator<InternalCacheInsertFeedArgument *> data_arr, size_t sz) noexcept
                {
                    InternalCacheInsertFeedArgument * base_data_arr = data_arr.base();

                    dg_sock::network_stack_allocation::NoExceptAllocation<cache_id_t[]> cache_id_arr(sz);
                    dg_sock::network_stack_allocation::NoExceptAllocation<Response[]> response_arr(sz);
                    dg_sock::network_stack_allocation::NoExceptAllocation<std::expected<bool, exception_t>[]> rs_arr(sz);

                    for (size_t i = 0u; i < sz; ++i)
                    {
                        cache_id_arr[i]                 = base_data_arr[i].cache_id;
                        Response * base_response_ptr    = base_data_arr[i].response_ptr.base();
                        response_arr[i]                 = std::move(*base_response_ptr);
                    }

                    this->cache_controller_arr[partitioned_idx]->insert_cache(cache_id_arr.get(),
                                                                              std::make_move_iterator(response_arr.get()), sz,
                                                                              rs_arr.get());

                    for (size_t i = 0u; i < sz; ++i)
                    {
                        *base_data_arr[i].rs = rs_arr[i];

                        if (!rs_arr[i].has_value())
                        {
                            Response * base_response_ptr    = base_data_arr[i].response_ptr.base();
                            *base_response_ptr              = std::move(response_arr[i]);
                        }
                    }
                }
            };
    };

    //we'll would like to use bloom filters yet we have found a reason to do so, we are micro optimizing
    //clear
    class CacheUniqueWriteController: public virtual CacheUniqueWriteControllerInterface
    {
        private:

            dg_sock::unordered_unstable_set<cache_id_t> cache_id_set;
            size_t cache_id_set_cap;
            size_t max_consume_per_load;

        public:

            CacheUniqueWriteController(dg_sock::unordered_unstable_set<cache_id_t> cache_id_set,
                                       size_t cache_id_set_cap,
                                       size_t max_consume_per_load) noexcept: cache_id_set(std::move(cache_id_set)),
                                                                              cache_id_set_cap(cache_id_set_cap),
                                                                              max_consume_per_load(max_consume_per_load){}

            void thru(cache_id_t * cache_id_arr, size_t sz, std::expected<bool, exception_t> * rs_arr) noexcept
            {
                if constexpr(DEBUG_MODE_FLAG)
                {
                    if (sz > this->max_consume_size())
                    {
                        dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                for (size_t i = 0u; i < sz; ++i)
                {
                    auto set_ptr = this->cache_id_set.find(cache_id_arr[i]);

                    if (set_ptr != this->cache_id_set.end())
                    {
                        rs_arr[i] = false; //false, found, already thru
                        continue;
                    }

                    //unique, try to insert

                    if (this->cache_id_set.size() == this->cache_id_set_cap)
                    {
                        //cap reached, return cap exception, no_actions 
                        rs_arr[i] = std::unexpected(dg_sock::network_exception::RESOURCE_EXHAUSTION);
                        continue;
                    }

                    auto [_, status]  = this->cache_id_set.insert(cache_id_arr[i]);
                    dg_sock::network_exception_handler::dg_assert(status);
                    rs_arr[i] = true; //thru, uniqueness acknowledged by cache_id_set
                }
            }

            void contains(cache_id_t * cache_id_arr, size_t sz, bool * rs_arr) noexcept
            {
                for (size_t i = 0u; i < sz; ++i)
                {
                    rs_arr[i] = this->cache_id_set.contains(cache_id_arr[i]);
                }
            }

            void clear() noexcept
            {
                this->cache_id_set.clear();
            }

            auto size() const noexcept -> size_t
            {
                return this->cache_id_set.size();
            }

            auto capacity() const noexcept -> size_t
            {
                return this->cache_id_set_cap;
            }

            auto max_consume_size() noexcept -> size_t
            {
                return this->max_consume_per_load;
            }
    };

    //clear
    class MutexControlledCacheWriteExclusionController: public virtual CacheUniqueWriteControllerInterface
    {
        private:

            std::unique_ptr<CacheUniqueWriteControllerInterface> base;
            std::unique_ptr<stdxx::fair_atomic_flag> mtx;

        public:

            MutexControlledCacheWriteExclusionController(std::unique_ptr<CacheUniqueWriteControllerInterface> base,
                                                         std::unique_ptr<stdxx::fair_atomic_flag> mtx) noexcept: base(std::move(base)),
                                                                                                    mtx(std::move(mtx)){}

            void thru(cache_id_t * cache_id_arr, size_t sz, std::expected<bool, exception_t> * rs_arr) noexcept
            {
                stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(*this->mtx);
                this->base->thru(cache_id_arr, sz, rs_arr);
            }

            void contains(cache_id_t * cache_id_arr, size_t sz, bool * rs_arr) noexcept
            {
                stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(*this->mtx);
                this->base->contains(cache_id_arr, sz, rs_arr);
            }

            void clear() noexcept
            {
                stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(*this->mtx);
                this->base->clear();
            }

            auto size() const noexcept -> size_t
            {
                stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(*this->mtx);
                return this->base->size();
            }

            auto capacity() const noexcept -> size_t
            {
                stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(*this->mtx);
                return this->base->capacity();
            }

            auto max_consume_size() noexcept -> size_t
            {
                stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(*this->mtx);
                return this->base->max_consume_size();
            }
    };

    //clear
    class AdvancedInfiniteCacheUniqueWriteController: public virtual InfiniteCacheUniqueWriteControllerInterface
    {
        private:

            dg_sock::cyclic_unordered_node_set<cache_id_t> cache_id_set;
            size_t max_consume_per_load;

        public:

            AdvancedInfiniteCacheUniqueWriteController(dg_sock::cyclic_unordered_node_set<cache_id_t> cache_id_set,
                                                       size_t max_consume_per_load) noexcept: cache_id_set(std::move(cache_id_set)),
                                                                                              max_consume_per_load(max_consume_per_load){}

            void thru(cache_id_t * cache_id_arr, size_t sz, std::expected<bool, exception_t> * rs_arr) noexcept
            {
                if constexpr(DEBUG_MODE_FLAG)
                {
                    if (sz > this->max_consume_size())
                    {
                        dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                for (size_t i = 0u; i < sz; ++i)
                {
                    auto set_ptr = this->cache_id_set.find(cache_id_arr[i]);

                    if (set_ptr != this->cache_id_set.end())
                    {
                        rs_arr[i] = false;
                        continue;
                    }

                    auto [_, status] = this->cache_id_set.insert(cache_id_arr[i]);
                    dg_sock::network_exception_handler::dg_assert(status);
                    rs_arr[i] = true;
                }
            }

            auto max_consume_size() noexcept -> size_t
            {
                return this->max_consume_per_load;
            }
    };

    //clear
    class MutexControlledInfiniteCacheWriteExclusionController: public virtual InfiniteCacheUniqueWriteControllerInterface
    {
        private:

            std::unique_ptr<InfiniteCacheUniqueWriteControllerInterface> base;
            std::unique_ptr<stdxx::fair_atomic_flag> mtx;

        public:

            MutexControlledInfiniteCacheWriteExclusionController(std::unique_ptr<InfiniteCacheUniqueWriteControllerInterface> base,
                                                                 std::unique_ptr<stdxx::fair_atomic_flag> mtx) noexcept: base(std::move(base)),
                                                                                                                        mtx(std::move(mtx)){}

            void thru(cache_id_t * cache_id_arr, size_t sz, std::expected<bool, exception_t> * rs_arr) noexcept
            {
                stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(*this->mtx);
                this->base->thru(cache_id_arr, sz, rs_arr);
            }

            auto max_consume_size() noexcept -> size_t
            {
                stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(*this->mtx);
                return this->base->max_consume_size();
            }
    };

    //clear
    class DistributedUniqueCacheWriteController: public virtual InfiniteCacheUniqueWriteControllerInterface
    {
        private:

            std::unique_ptr<std::unique_ptr<InfiniteCacheUniqueWriteControllerInterface>[]> base_arr;
            size_t pow2_base_arr_sz;
            size_t thru_keyvalue_feed_cap;
            size_t max_consume_per_load;

        public:

            DistributedUniqueCacheWriteController(std::unique_ptr<std::unique_ptr<InfiniteCacheUniqueWriteControllerInterface>[]> base_arr,
                                                  size_t pow2_base_arr_sz,
                                                  size_t thru_keyvalue_feed_cap,
                                                  size_t max_consume_per_load) noexcept: base_arr(std::move(base_arr)),
                                                                                         pow2_base_arr_sz(pow2_base_arr_sz),
                                                                                         thru_keyvalue_feed_cap(thru_keyvalue_feed_cap),
                                                                                         max_consume_per_load(max_consume_per_load){}

            void thru(cache_id_t * cache_id_arr, size_t sz, std::expected<bool, exception_t> * rs_arr) noexcept
            {
                auto feed_resolutor                     = InternalThruFeedResolutor{};
                feed_resolutor.controller_arr           = this->base_arr.get();

                size_t trimmed_thru_keyvalue_feed_cap   = std::min(this->thru_keyvalue_feed_cap, sz);
                size_t feeder_allocation_cost           = dg_sock::network_producer_consumer::delvrsrv_kv_allocation_cost(&feed_resolutor, trimmed_thru_keyvalue_feed_cap);
                dg_sock::network_stack_allocation::NoExceptRawAllocation<char[]> feeder_mem(feeder_allocation_cost);
                auto feeder                             = dg_sock::network_exception_handler::nothrow_log(dg_sock::network_producer_consumer::delvrsrv_kv_open_preallocated_raiihandle(&feed_resolutor, trimmed_thru_keyvalue_feed_cap, feeder_mem.get()));

                for (size_t i = 0u; i < sz; ++i)
                {
                    size_t hashed_cache_id_value    = dg_sock::network_hash::hash_reflectible(cache_id_arr[i]); 
                    size_t partitioned_idx          = hashed_cache_id_value & (this->pow2_base_arr_sz - 1u);
                    auto feed_arg                   = InternalThruFeedArgument
                    {
                        .cache_id    = cache_id_arr[i],
                        .rs_ptr      = std::next(rs_arr, i)
                    };

                    dg_sock::network_producer_consumer::delvrsrv_kv_deliver(feeder.get(), partitioned_idx, feed_arg);
                }
            }

            auto max_consume_size() noexcept -> size_t{

                return this->max_consume_per_load;
            }

        private:

            struct InternalThruFeedArgument
            {
                cache_id_t cache_id;
                std::expected<bool, exception_t> * rs_ptr;
            };

            struct InternalThruFeedResolutor: dg_sock::network_producer_consumer::KVConsumerInterface<size_t, InternalThruFeedArgument>
            {
                std::unique_ptr<InfiniteCacheUniqueWriteControllerInterface> * controller_arr;

                void push(const size_t& partitioned_idx, std::move_iterator<InternalThruFeedArgument *> data_arr, size_t sz) noexcept
                {
                    InternalThruFeedArgument * base_data_arr = data_arr.base();

                    dg_sock::network_stack_allocation::NoExceptAllocation<cache_id_t[]> cache_id_arr(sz);
                    dg_sock::network_stack_allocation::NoExceptAllocation<std::expected<bool, exception_t>[]> rs_arr(sz);

                    for (size_t i = 0u; i < sz; ++i)
                    {
                        cache_id_arr[i] = base_data_arr[i].cache_id;
                    }

                    this->controller_arr[partitioned_idx]->thru(cache_id_arr.get(), sz, rs_arr.get());

                    for (size_t i = 0u; i < sz; ++i)
                    {
                        *base_data_arr[i].rs_ptr = rs_arr[i];
                    }
                }
            };
    };

    struct AtomicBlock
    {
        uint32_t thru_counter;
        uint32_t ver_ctrl;

        template <class Reflector>
        constexpr void dg_reflect(const Reflector& reflector) const noexcept
        {
            reflector(thru_counter, ver_ctrl);
        }

        template <class Reflector>
        constexpr void dg_reflect(const Reflector& reflector) noexcept
        {
            reflector(thru_counter, ver_ctrl);
        }
    };

    using atomic_block_pragma_0_t = std::array<char, sizeof(uint32_t) + sizeof(uint32_t)>;

    //clear
    class CacheUniqueWriteTrafficController: public virtual CacheUniqueWriteTrafficControllerInterface
    {
        private:

            stdxx::inplace_hdi_container<std::atomic<atomic_block_pragma_0_t>> block_ctrl;
            stdxx::hdi_container<size_t> thru_cap;

            static constexpr auto make_pragma_0_block(AtomicBlock arg) noexcept -> atomic_block_pragma_0_t
            {
                constexpr size_t TRIVIAL_SZ = dg_sock::network_trivial_serializer::size(AtomicBlock{});
                static_assert(TRIVIAL_SZ <= atomic_block_pragma_0_t{}.size());

                auto rs = atomic_block_pragma_0_t{};
                dg_sock::network_trivial_serializer::serialize_into(rs.data(), arg);

                return rs;
            }

            static constexpr auto read_pragma_0_block(atomic_block_pragma_0_t arg) noexcept -> AtomicBlock
            {
                auto rs = AtomicBlock{};
                dg_sock::network_trivial_serializer::deserialize_into(rs, arg.data());

                return rs;
            }

        public:

            using self = CacheUniqueWriteTrafficController;

            CacheUniqueWriteTrafficController(size_t thru_cap) noexcept: block_ctrl(std::in_place_t{}, self::make_pragma_0_block(AtomicBlock{0u, 0u})),
                                                                         thru_cap(stdxx::hdi_container<size_t>{thru_cap}){}

            auto thru(size_t incoming_sz) noexcept -> std::expected<bool, exception_t>
            {
                std::expected<bool, exception_t> rs = {}; 

                auto busy_wait_task = [&, this]() noexcept
                {
                    atomic_block_pragma_0_t then_block  = this->block_ctrl.value.load(std::memory_order_relaxed);
                    AtomicBlock then_semantic_block     = self::read_pragma_0_block(then_block);

                    if (then_semantic_block.thru_counter + incoming_sz > this->thru_cap.value)
                    {
                        rs = false;
                        return true;
                    }

                    AtomicBlock now_semantic_block      = AtomicBlock{.thru_counter = then_semantic_block.thru_counter + incoming_sz,
                                                                      .ver_ctrl     = then_semantic_block.ver_ctrl + 1u};

                    atomic_block_pragma_0_t now_block   = self::make_pragma_0_block(now_semantic_block);

                    bool was_updated                    = this->block_ctrl.value.compare_exchange_strong(then_block, now_block, std::memory_order_relaxed);

                    if (was_updated)
                    {
                        rs = true;
                        return true;
                    }

                    return false;
                };

                stdxx::busy_wait(busy_wait_task);
                return rs;
            }

            auto thru_size() const noexcept -> size_t
            {
                atomic_block_pragma_0_t blk = this->block_ctrl.value.load(std::memory_order_relaxed);
                AtomicBlock semantic_blk    = self::read_pragma_0_block(blk);

                return semantic_blk.thru_counter;
            }

            auto thru_capacity() const noexcept -> size_t
            {
                return this->thru_cap.value;
            }

            void reset() noexcept
            {
                auto busy_wait_task = [&, this]() noexcept
                {
                    atomic_block_pragma_0_t then_block  = this->block_ctrl.value.load(std::memory_order_relaxed);
                    AtomicBlock then_semantic_block     = self::read_pragma_0_block(then_block); 
                    AtomicBlock now_semantic_block      = AtomicBlock{.thru_counter = 0u,
                                                                      .ver_ctrl     = then_semantic_block.ver_ctrl + 1u};
                    atomic_block_pragma_0_t now_block   = self::make_pragma_0_block(now_semantic_block);

                    return this->block_ctrl.value.compare_exchange_strong(then_block, now_block, std::memory_order_relaxed);
                };

                stdxx::busy_wait(busy_wait_task);
            }
    };

    //clear
    class DistributedCacheUniqueWriteTrafficController: public virtual CacheUniqueWriteTrafficControllerInterface
    {
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

            auto thru(size_t incoming_sz) noexcept -> std::expected<bool, exception_t>
            {
                //why arent we using a statistical thru (1 thru out of 40), we are susceptible to leaks of incoming_sz

                if (incoming_sz > this->thru_capacity())
                {
                    return std::unexpected(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                size_t random_clue  = dg_sock::network_randomizer::randomize_int<size_t>();
                size_t base_arr_idx = random_clue & (this->pow2_base_arr_sz - 1u);

                return this->base_arr[base_arr_idx]->thru(incoming_sz);
            }

            void reset() noexcept
            {
                for (size_t i = 0u; i < this->pow2_base_arr_sz; ++i)
                {
                    this->base_arr[i]->reset();
                }
            }

            auto thru_capacity() const noexcept -> size_t
            {
                return this->max_thru_sz;
            }
    };

    //clear
    class SelfUpdateCacheUniqueWriteTrafficController: public virtual CacheUniqueWriteTrafficControllerInterface
    {
        private:

            std::shared_ptr<CacheUniqueWriteTrafficControllerInterface> base;
            std::shared_ptr<void> daemon;

        public:

            SelfUpdateCacheUniqueWriteTrafficController(std::unique_ptr<CacheUniqueWriteTrafficControllerInterface> base,
                                                        std::chrono::nanoseconds update_dur)
            {
                if (base == nullptr)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                const std::chrono::nanoseconds MIN_UPDATE_DUR   = std::chrono::nanoseconds(0);
                const std::chrono::nanoseconds MAX_UPDATE_DUR   = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::hours(1));

                if (std::clamp(update_dur, MIN_UPDATE_DUR, MAX_UPDATE_DUR) != update_dur)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                this->base      = std::move(base);
                this->daemon    = dg_sock::network_cron::register_periodic_cronjob(dg_sock::make_shared<InternalUpdater>(this->base),
                                                                                   update_dur);
            }

            auto thru(size_t incoming_sz) noexcept -> std::expected<bool, exception_t>
            {
                return this->base->thru(incoming_sz);
            }

            void reset() noexcept
            {
                this->base->reset();
            }

            auto thru_capacity() const noexcept -> size_t
            {
                return this->base->thru_capacity();
            }

        private:

            class InternalUpdater: public virtual dg_sock::network_cron::UpdatableInterface
            {
                private:

                    std::shared_ptr<CacheUniqueWriteTrafficControllerInterface> base;

                public:

                    InternalUpdater(std::shared_ptr<CacheUniqueWriteTrafficControllerInterface> base): base(std::move(base)){}

                    void update()
                    {
                        this->base->reset();
                    }
            };
    };

    //clear
    class RequestHandlerDictionary: public virtual RequestHandlerDictionaryInterface
    {
        private:

            dg_sock::unordered_unstable_map<dg_sock::string, std::shared_ptr<RequestHandlerInterface>> request_map;
            std::unique_ptr<stdxx::fair_atomic_flag> mtx;

        public:

            RequestHandlerDictionary(): request_map(),
                                        mtx(stdxx::make_unique_fair_atomic_flag()){}

            auto add_resolver(std::string_view resource_addr, std::shared_ptr<RequestHandlerInterface> request_handler) noexcept -> exception_t
            {
                stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(*this->mtx);

                if (request_handler == nullptr)
                {
                    return dg_sock::network_exception::INVALID_ARGUMENT;
                }

                try
                {
                    this->request_map.insert_or_assign(resource_addr, request_handler);
                }
                catch (...)
                {
                    return dg_sock::network_exception::wrap_std_exception(std::current_exception());
                }

                return dg_sock::network_exception::SUCCESS;
            }

            void remove_resolver(std::string_view resource_addr) noexcept
            {
                stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(*this->mtx);

                this->request_map.erase(resource_addr);
            }

            auto get_resolver(std::string_view resource_addr) noexcept -> std::shared_ptr<RequestHandlerInterface>
            {
                stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(*this->mtx);

                auto map_ptr = this->request_map.find(resource_addr);

                if (map_ptr == this->request_map.end())
                {
                    return nullptr;
                }

                return map_ptr->second;
            }
    };

    //clear
    class OneFetchRequestHandlerRetriever: public virtual RequestHandlerRetrieverInterface
    {
        private:

            std::shared_ptr<RequestHandlerDictionaryInterface> global_pool;
            dg_sock::unordered_unstable_map<dg_sock::string, std::shared_ptr<RequestHandlerInterface>> request_map;

        public:

            OneFetchRequestHandlerRetriever(std::shared_ptr<RequestHandlerDictionaryInterface> global_pool)
            {
                if (global_pool == nullptr)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                this->global_pool   = std::move(global_pool);
                this->request_map   = {};
            }

            auto get_resolver(std::string_view resource_addr) noexcept -> RequestHandlerInterface *
            {
                auto map_ptr = this->request_map.find(resource_addr);

                if (map_ptr == this->request_map.end()) [[unlikely]]
                {
                    return this->slow_path(resource_addr);
                }
                else [[likely]]
                {
                    return map_ptr->second.get();
                }
            }            
        
        private:

            __attribute__((noinline)) auto slow_path(std::string_view resource_addr) noexcept -> RequestHandlerInterface *
            {
                auto map_ptr = this->request_map.find(resource_addr);

                if (map_ptr == this->request_map.end())
                {
                    std::shared_ptr<RequestHandlerInterface> tmp = this->global_pool->get_resolver(resource_addr);

                    if (tmp == nullptr)
                    {
                        return nullptr;
                    }

                    try
                    {
                        auto [new_map_ptr, status] = this->request_map.insert(std::make_pair(resource_addr, tmp));
                        dg_sock::network_exception_handler::dg_assert(status);
                        map_ptr = new_map_ptr;
                    }
                    catch (...)
                    {
                        dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }                    
                }

                return map_ptr->second.get();
            }
    };

    //clear
    class RequestFiltererDictionary: public virtual RequestFiltererDictionaryInterface
    {
        private:

            dg_sock::unordered_unstable_map<dg_sock::string, std::shared_ptr<RequestFiltererInterface>> filterer_map;
            std::unique_ptr<stdxx::fair_atomic_flag> mtx;

        public:

            RequestFiltererDictionary(): filterer_map(),
                                         mtx(stdxx::make_unique_fair_atomic_flag()){}

            auto add_filterer(std::string_view resource_addr, std::shared_ptr<RequestFiltererInterface> request_filterer) noexcept -> exception_t
            {
                stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(*this->mtx);

                if (request_filterer == nullptr)
                {
                    return dg_sock::network_exception::INVALID_ARGUMENT;
                }

                try
                {
                    this->filterer_map.insert_or_assign(resource_addr, request_filterer);
                }
                catch (...)
                {
                    return dg_sock::network_exception::wrap_std_exception(std::current_exception());
                }

                return dg_sock::network_exception::SUCCESS;
            }

            void remove_filterer(std::string_view resource_addr) noexcept
            {
                stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(*this->mtx);

                this->filterer_map.erase(resource_addr);
            }

            auto get_filterer(std::string_view resource_addr) noexcept -> std::shared_ptr<RequestFiltererInterface>
            {
                stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(*this->mtx);

                auto map_ptr = this->filterer_map.find(resource_addr);

                if (map_ptr == this->filterer_map.end())
                {
                    return nullptr;
                }

                return map_ptr->second;
            }
    };

    //clear
    class OneFetchRequestFiltererRetriever: public virtual RequestFiltererRetrieverInterface
    {
        private:

            std::shared_ptr<RequestFiltererDictionaryInterface> global_pool;
            dg_sock::unordered_unstable_map<dg_sock::string, std::shared_ptr<RequestFiltererInterface>> request_map;
        
        public:

            OneFetchRequestFiltererRetriever(std::shared_ptr<RequestFiltererDictionaryInterface> global_pool)
            {
                if (global_pool == nullptr)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                this->global_pool   = std::move(global_pool);
                this->request_map   = {};
            }

            auto get_filterer(std::string_view resource_addr) noexcept -> RequestFiltererInterface *
            {
                auto map_ptr = this->request_map.find(resource_addr);

                if (map_ptr == this->request_map.end()) [[unlikely]]
                {
                    return this->slow_path(resource_addr);
                }
                else [[likely]]
                {
                    return map_ptr->second.get();
                }
            }
        
        private:
            
            __attribute__((noinline)) auto slow_path(std::string_view resource_addr) noexcept -> RequestFiltererInterface *
            {
                auto map_ptr = this->request_map.find(resource_addr);

                if (map_ptr == this->request_map.end())
                {
                    std::shared_ptr<RequestFiltererInterface> tmp = this->global_pool->get_filterer(resource_addr);

                    if (tmp == nullptr)
                    {
                        return nullptr;
                    }

                    try
                    {
                        auto [new_map_ptr, status] = this->request_map.insert(std::make_pair(resource_addr, tmp));
                        dg_sock::network_exception_handler::dg_assert(status);
                        map_ptr = new_map_ptr;
                    }
                    catch (...)
                    {
                        dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                return map_ptr->second.get();
            }
    };

    //clear
    class OneRequestHandlerAdapter: public virtual RequestHandlerInterface
    {
        private:

            std::shared_ptr<OneRequestHandlerInterface> base;

        public:

            OneRequestHandlerAdapter(std::shared_ptr<OneRequestHandlerInterface> base) noexcept: base(std::move(base)){}

            void handle(std::move_iterator<Request *> request_arr, size_t request_arr_sz, Response * response_arr) noexcept
            {
                auto base_request_arr = request_arr.base();

                for (size_t i = 0u; i < request_arr_sz; ++i)
                {
                    try
                    {
                        response_arr[i] = this->base->handle(base_request_arr[i]);
                    }
                    catch (...)
                    {
                        response_arr[i] = Response
                        {
                            .response                       = {},
                            .response_serialization_format  = {},
                            .err_code                       = dg_sock::network_exception::wrap_std_exception(std::current_exception())
                        };
                    }
                }
            }

            auto max_consume_size() noexcept -> size_t
            {
                return std::numeric_limits<size_t>::max();
            }
    };

    //clear
    class RequestResolverWorker: public virtual dg_sock::network_concurrency::WorkerInterface
    {
        private:

            std::shared_ptr<RequestHandlerRetrieverInterface> request_handler_map;
            std::shared_ptr<RequestFiltererRetrieverInterface> request_filterer_map;
            std::shared_ptr<InfiniteCacheControllerInterface> request_cache_controller;
            std::shared_ptr<InfiniteCacheUniqueWriteControllerInterface> cachewrite_uex_controller;
            std::shared_ptr<CacheUniqueWriteTrafficControllerInterface> cachewrite_traffic_controller;
            uint32_t recv_channel;
            uint32_t send_channel;  
            size_t resolve_consume_sz;
            size_t mailbox_feed_cap;
            size_t mailbox_prep_feed_cap;
            size_t cache_controller_feed_cap;
            size_t server_fetch_feed_cap;
            size_t cache_server_fetch_feed_cap;
            size_t cache_fetch_feed_cap;
            size_t busy_consume_sz;

        public:

            RequestResolverWorker(std::shared_ptr<RequestHandlerRetrieverInterface> request_handler_map,
                                  std::shared_ptr<RequestFiltererRetrieverInterface> request_filterer_map,
                                  std::shared_ptr<InfiniteCacheControllerInterface> request_cache_controller,
                                  std::shared_ptr<InfiniteCacheUniqueWriteControllerInterface> cachewrite_uex_controller,
                                  std::shared_ptr<CacheUniqueWriteTrafficControllerInterface> cachewrite_traffic_controller,
                                  uint32_t recv_channel,
                                  uint32_t send_channel,
                                  size_t resolve_consume_sz,
                                  size_t mailbox_feed_cap,
                                  size_t mailbox_prep_feed_cap,
                                  size_t cache_controller_feed_cap,
                                  size_t server_fetch_feed_cap,
                                  size_t cache_server_fetch_feed_cap,
                                  size_t cache_fetch_feed_cap,
                                  size_t busy_consume_sz) noexcept: request_handler_map(std::move(request_handler_map)),
                                                                    request_filterer_map(std::move(request_filterer_map)),
                                                                    request_cache_controller(std::move(request_cache_controller)),
                                                                    cachewrite_uex_controller(std::move(cachewrite_uex_controller)),
                                                                    cachewrite_traffic_controller(std::move(cachewrite_traffic_controller)),
                                                                    recv_channel(recv_channel),
                                                                    send_channel(send_channel),
                                                                    resolve_consume_sz(resolve_consume_sz),
                                                                    mailbox_feed_cap(mailbox_feed_cap),
                                                                    mailbox_prep_feed_cap(mailbox_prep_feed_cap),
                                                                    cache_controller_feed_cap(cache_controller_feed_cap),
                                                                    server_fetch_feed_cap(server_fetch_feed_cap),
                                                                    cache_server_fetch_feed_cap(cache_server_fetch_feed_cap),
                                                                    cache_fetch_feed_cap(cache_fetch_feed_cap),
                                                                    busy_consume_sz(busy_consume_sz){}

            bool run_one_epoch() noexcept
            {
                size_t recv_buf_cap = this->resolve_consume_sz;
                size_t recv_buf_sz  = {};

                dg_sock::network_stack_allocation::NoExceptAllocation<dg_sock::string[]> recv_buf_arr(recv_buf_cap);
                dg_sock::network_kernel_mailbox::recv(this->recv_channel, recv_buf_arr.get(), recv_buf_sz, recv_buf_cap);

                auto mailbox_feed_resolutor                                 = InternalResponseFeedResolutor{}; 
                mailbox_feed_resolutor.send_channel                         = this->send_channel;

                size_t trimmed_mailbox_feed_cap                             = std::min(std::min(this->mailbox_feed_cap, dg_sock::network_kernel_mailbox::max_consume_size()), recv_buf_sz);
                size_t mailbox_feeder_allocation_cost                       = dg_sock::network_producer_consumer::delvrsrv_allocation_cost(&mailbox_feed_resolutor, trimmed_mailbox_feed_cap);
                dg_sock::network_stack_allocation::NoExceptRawAllocation<char[]> mailbox_feeder_mem(mailbox_feeder_allocation_cost);
                auto mailbox_feeder                                         = dg_sock::network_exception_handler::nothrow_log(dg_sock::network_producer_consumer::delvrsrv_open_preallocated_raiihandle(&mailbox_feed_resolutor, trimmed_mailbox_feed_cap, mailbox_feeder_mem.get())); 

                //---

                auto mailbox_prep_feed_resolutor                            = InternalMailBoxPrepFeedResolutor{};
                mailbox_prep_feed_resolutor.mailbox_feeder                  = mailbox_feeder.get();

                size_t trimmed_mailbox_prep_feed_cap                        = std::min(this->mailbox_prep_feed_cap, recv_buf_sz);
                size_t mailbox_prep_feeder_allocation_cost                  = dg_sock::network_producer_consumer::delvrsrv_allocation_cost(&mailbox_prep_feed_resolutor, trimmed_mailbox_prep_feed_cap);
                dg_sock::network_stack_allocation::NoExceptAllocation<char[]> mailbox_prep_feeder_mem(mailbox_prep_feeder_allocation_cost);
                auto mailbox_prep_feeder                                    = dg_sock::network_exception_handler::nothrow_log(dg_sock::network_producer_consumer::delvrsrv_open_preallocated_raiihandle(&mailbox_prep_feed_resolutor, trimmed_mailbox_prep_feed_cap, mailbox_prep_feeder_mem.get())); 

                //---

                auto cache_map_insert_feed_resolutor                        = InternalCacheMapFeedResolutor{}; 
                cache_map_insert_feed_resolutor.cache_controller            = this->request_cache_controller.get();

                size_t trimmed_cache_map_insert_feed_cap                    = std::min(std::min(this->cache_controller_feed_cap, this->request_cache_controller->max_consume_size()), recv_buf_sz); 
                size_t cache_map_insert_feeder_allocation_cost              = dg_sock::network_producer_consumer::delvrsrv_allocation_cost(&cache_map_insert_feed_resolutor, trimmed_cache_map_insert_feed_cap);
                dg_sock::network_stack_allocation::NoExceptAllocation<char[]> cache_map_insert_feeder_mem(cache_map_insert_feeder_allocation_cost);
                auto cache_map_insert_feeder                                = dg_sock::network_exception_handler::nothrow_log(dg_sock::network_producer_consumer::delvrsrv_open_preallocated_raiihandle(&cache_map_insert_feed_resolutor, trimmed_cache_map_insert_feed_cap, cache_map_insert_feeder_mem.get())); 

                //---

                auto server_fetch_feed_resolutor                            = InternalServerFeedResolutor{};
                server_fetch_feed_resolutor.request_handler_map             = this->request_handler_map.get();
                server_fetch_feed_resolutor.request_filterer_map            = this->request_filterer_map.get();
                server_fetch_feed_resolutor.mailbox_prep_feeder             = mailbox_prep_feeder.get();
                server_fetch_feed_resolutor.cache_map_feeder                = cache_map_insert_feeder.get();

                size_t trimmed_server_fetch_feed_cap                        = std::min(this->server_fetch_feed_cap, recv_buf_sz);
                size_t server_feeder_allocation_cost                        = dg_sock::network_producer_consumer::delvrsrv_kv_allocation_cost(&server_fetch_feed_resolutor, trimmed_server_fetch_feed_cap);
                dg_sock::network_stack_allocation::NoExceptRawAllocation<char[]> server_feeder_mem(server_feeder_allocation_cost);
                auto server_feeder                                          = dg_sock::network_exception_handler::nothrow_log(dg_sock::network_producer_consumer::delvrsrv_kv_open_preallocated_raiihandle(&server_fetch_feed_resolutor, trimmed_server_fetch_feed_cap, server_feeder_mem.get()));

                //---

                auto cache_server_fetch_feed_resolutor                      = InternalCacheServerFeedResolutor{}; 
                cache_server_fetch_feed_resolutor.cachewrite_uex_controller = this->cachewrite_uex_controller.get();
                cache_server_fetch_feed_resolutor.cachewrite_tfx_controller = this->cachewrite_traffic_controller.get();
                cache_server_fetch_feed_resolutor.server_feeder             = server_feeder.get();
                cache_server_fetch_feed_resolutor.mailbox_prep_feeder       = mailbox_prep_feeder.get();

                size_t trimmed_cache_server_fetch_feed_cap                  = std::min(std::min(std::min(this->cache_server_fetch_feed_cap, this->cachewrite_uex_controller->max_consume_size()), this->cachewrite_traffic_controller->thru_capacity()), recv_buf_sz);
                size_t cache_server_feeder_allocation_cost                  = dg_sock::network_producer_consumer::delvrsrv_allocation_cost(&cache_server_fetch_feed_resolutor, trimmed_cache_server_fetch_feed_cap);
                dg_sock::network_stack_allocation::NoExceptAllocation<char[]> cache_server_feeder_mem(cache_server_feeder_allocation_cost);
                auto cache_server_feeder                                    = dg_sock::network_exception_handler::nothrow_log(dg_sock::network_producer_consumer::delvrsrv_open_preallocated_raiihandle(&cache_server_fetch_feed_resolutor, trimmed_cache_server_fetch_feed_cap, cache_server_feeder_mem.get())); 

                //---

                auto cache_fetch_feed_resolutor                             = InternalCacheFeedResolutor{};
                cache_fetch_feed_resolutor.cache_server_feeder              = cache_server_feeder.get();
                cache_fetch_feed_resolutor.mailbox_prep_feeder              = mailbox_prep_feeder.get();
                cache_fetch_feed_resolutor.cache_controller                 = this->request_cache_controller.get();

                size_t trimmed_cache_fetch_feed_cap                         = std::min(this->cache_fetch_feed_cap, recv_buf_sz);
                size_t cache_fetch_feeder_allocation_cost                   = dg_sock::network_producer_consumer::delvrsrv_allocation_cost(&cache_fetch_feed_resolutor, trimmed_cache_fetch_feed_cap);
                dg_sock::network_stack_allocation::NoExceptRawAllocation<char[]> cache_fetch_feeder_mem(cache_fetch_feeder_allocation_cost);
                auto cache_fetch_feeder                                     = dg_sock::network_exception_handler::nothrow_log(dg_sock::network_producer_consumer::delvrsrv_open_preallocated_raiihandle(&cache_fetch_feed_resolutor, trimmed_cache_fetch_feed_cap, cache_fetch_feeder_mem.get()));

                for (size_t i = 0u; i < recv_buf_sz; ++i)
                {
                    std::expected<model::InternalRequest, exception_t> request = dg_sock::network_exception::to_cstyle_function(dg_sock::network_compact_serializer::integrity_deserialize<model::InternalRequest, dg_sock::string>)(recv_buf_arr[i], model::INTERNAL_REQUEST_SERIALIZATION_SECRET); 

                    if (!request.has_value())
                    {
                        dg_sock::network_log_stackdump::error_fast(dg_sock::network_exception::verbose(request.error()));
                        continue;
                    }

                    dg_sock::network_kernel_mailbox::Address requestor_addr = request->request.requestor;

                    auto now = std::chrono::utc_clock::now();

                    if (request->server_abs_timeout.has_value() && request->server_abs_timeout.value() <= now)
                    {
                        auto response   = model::InternalResponse{.response     = std::unexpected(dg_sock::network_exception::REST_SERVERSIDE_ABSTIMEOUT_TIMEOUT), 
                                                                  .ticket_id    = request->ticket_id};

                        auto prep_arg   = InternalMailBoxPrepFeedArgument{.to               = requestor_addr,
                                                                          .response         = std::move(response)}; 

                        dg_sock::network_producer_consumer::delvrsrv_deliver(mailbox_prep_feeder.get(), std::move(prep_arg));
                        continue;
                    }

                    ResourceAddress resource_path               = request->request.requestee_url;
                    RequestHandlerInterface * resource_handler  = this->request_handler_map->get_resolver(resource_path.resource_addr);

                    if (resource_handler == nullptr)
                    {
                        auto response   = model::InternalResponse{.response   = std::unexpected(dg_sock::network_exception::REST_INVALID_URL), 
                                                                  .ticket_id  = request->ticket_id};

                        auto prep_arg   = InternalMailBoxPrepFeedArgument{.to               = requestor_addr,
                                                                          .response         = std::move(response)};

                        dg_sock::network_producer_consumer::delvrsrv_deliver(mailbox_prep_feeder.get(), std::move(prep_arg));
                        continue;
                    }

                    if (request->has_unique_response)
                    {
                        if (!request->client_request_cache_id.has_value())
                        {
                            auto response   = model::InternalResponse{.response     = std::unexpected(dg_sock::network_exception::REST_INVALID_ARGUMENT),
                                                                      .ticket_id    = request->ticket_id};

                            auto prep_arg   = InternalMailBoxPrepFeedArgument{.to               = requestor_addr,
                                                                              .response         = std::move(response)};

                            dg_sock::network_producer_consumer::delvrsrv_deliver(mailbox_prep_feeder.get(), std::move(prep_arg)); 
                            continue;
                        }

                        auto arg   = InternalCacheFeedArgument{.to              = requestor_addr,
                                                               .local_uri_path  = dg_sock::string(resource_path.resource_addr),
                                                               .cache_id        = request->client_request_cache_id.value(),
                                                               .ticket_id       = request->ticket_id,
                                                               .request         = std::move(request->request)};

                        dg_sock::network_producer_consumer::delvrsrv_deliver(cache_fetch_feeder.get(), std::move(arg));                        
                        continue;
                    }

                    auto key_arg        = dg_sock::string(resource_path.resource_addr);
                    auto value_arg      = InternalServerFeedResolutorArgument{.to               = requestor_addr,
                                                                              .cache_write_id   = std::nullopt,
                                                                              .ticket_id        = request->ticket_id,
                                                                              .request          = std::move(request->request)};

                    dg_sock::network_producer_consumer::delvrsrv_kv_deliver(server_feeder.get(), key_arg, std::move(value_arg));
                }

                return recv_buf_sz >= this->busy_consume_sz;
            }
        
        private:

            struct InternalMailBoxArgument
            {
                Address to;
                dg_sock::string content;
            };

            struct InternalResponseFeedArgument
            {
                InternalMailBoxArgument mailbox_arg;
            };

            struct InternalResponseFeedResolutor: dg_sock::network_producer_consumer::ConsumerInterface<InternalResponseFeedArgument>
            {
                uint32_t send_channel;

                void push(std::move_iterator<InternalResponseFeedArgument *> data_arr, size_t sz) noexcept
                {
                    InternalResponseFeedArgument * base_data_arr = data_arr.base();

                    dg_sock::network_stack_allocation::NoExceptAllocation<exception_t[]> exception_arr(sz);
                    dg_sock::network_stack_allocation::NoExceptAllocation<Address[]> addr_arr(sz);
                    dg_sock::network_stack_allocation::NoExceptAllocation<dg_sock::string[]> content_arr(sz);

                    for (size_t i = 0u; i < sz; ++i)
                    {
                        addr_arr[i]     = base_data_arr[i].mailbox_arg.to;
                        content_arr[i]  = std::move(base_data_arr[i].mailbox_arg.content);
                    }

                    dg_sock::network_kernel_mailbox::send(this->send_channel,
                                                          addr_arr.get(), content_arr.get(), sz,
                                                          exception_arr.get());

                    for (size_t i = 0u; i < sz; ++i)
                    {
                        if (dg_sock::network_exception::is_failed(exception_arr[i]))
                        {
                            dg_sock::network_log_stackdump::error_fast(dg_sock::network_exception::verbose(exception_arr[i]));
                        }
                    }
                }
            };

            struct InternalMailBoxPrepFeedArgument
            {
                dg_sock::network_kernel_mailbox::Address to;
                InternalResponse response;
            };

            struct InternalMailBoxPrepFeedResolutor: dg_sock::network_producer_consumer::ConsumerInterface<InternalMailBoxPrepFeedArgument>
            {
                dg_sock::network_producer_consumer::DeliveryHandle<InternalResponseFeedArgument> * mailbox_feeder;

                void push(std::move_iterator<InternalMailBoxPrepFeedArgument *> data_arr, size_t sz) noexcept{

                    InternalMailBoxPrepFeedArgument * base_data_arr = data_arr.base();

                    for (size_t i = 0u; i < sz; ++i)
                    {
                        std::expected<dg_sock::string, exception_t> serialized_response = dg_sock::network_exception::to_cstyle_function(dg_sock::network_compact_serializer::dgstd_serialize<dg_sock::string, InternalResponse>)(base_data_arr[i].response, model::INTERNAL_RESPONSE_SERIALIZATION_SECRET);

                        if (!serialized_response.has_value())
                        {
                            dg_sock::network_log_stackdump::error_fast(dg_sock::network_exception::verbose(serialized_response.error()));
                            continue;
                        }

                        auto mailbox_arg        = InternalMailBoxArgument
                        {
                            .to         = base_data_arr[i].to,
                            .content    = std::move(serialized_response.value())
                        };

                        auto response_feed_arg  = InternalResponseFeedArgument
                        {
                            .mailbox_arg     = std::move(mailbox_arg)
                        };

                        dg_sock::network_producer_consumer::delvrsrv_deliver(this->mailbox_feeder, std::move(response_feed_arg));
                    }
                }
            };

            struct InternalCacheMapFeedArgument
            {
                cache_id_t cache_id;
                Response response;
            };

            struct InternalCacheMapFeedResolutor: dg_sock::network_producer_consumer::ConsumerInterface<InternalCacheMapFeedArgument>
            {
                InfiniteCacheControllerInterface * cache_controller;

                void push(std::move_iterator<InternalCacheMapFeedArgument *> data_arr, size_t sz) noexcept
                {
                    auto base_data_arr = data_arr.base();

                    dg_sock::network_stack_allocation::NoExceptAllocation<cache_id_t[]> cache_id_arr(sz);
                    dg_sock::network_stack_allocation::NoExceptAllocation<Response[]> response_arr(sz);
                    dg_sock::network_stack_allocation::NoExceptAllocation<std::expected<bool, exception_t>[]> rs_arr(sz);

                    for (size_t i = 0u; i < sz; ++i)
                    {
                        cache_id_arr[i] = base_data_arr[i].cache_id;
                        response_arr[i] = std::move(base_data_arr[i].response);
                    }

                    this->cache_controller->insert_cache(cache_id_arr.get(), std::make_move_iterator(response_arr.get()), sz, rs_arr.get());

                    for (size_t i = 0u; i < sz; ++i)
                    {
                        if (!rs_arr[i].has_value())
                        {
                            dg_sock::network_log_stackdump::error_fast_optional(dg_sock::network_exception::verbose(rs_arr[i].error()));
                            continue;
                        }

                        if (!rs_arr[i].value())
                        {
                            dg_sock::network_log_stackdump::error_fast_optional("REST_CACHEMAP BAD INSERT");
                            continue;
                        }
                    }
                }
            };

            struct InternalServerFeedResolutorArgument
            {
                dg_sock::network_kernel_mailbox::Address to;
                std::optional<cache_id_t> cache_write_id;
                ticket_id_t ticket_id;
                Request request;
            };

            struct InternalServerFeedResolutor: dg_sock::network_producer_consumer::KVConsumerInterface<dg_sock::string, InternalServerFeedResolutorArgument>{

                RequestHandlerRetrieverInterface * request_handler_map;
                RequestFiltererRetrieverInterface * request_filterer_map;

                dg_sock::network_producer_consumer::DeliveryHandle<InternalMailBoxPrepFeedArgument> * mailbox_prep_feeder;
                dg_sock::network_producer_consumer::DeliveryHandle<InternalCacheMapFeedArgument> * cache_map_feeder;

                void push(const dg_sock::string& local_uri_path, std::move_iterator<InternalServerFeedResolutorArgument *> data_arr, size_t sz) noexcept
                {
                    auto base_data_arr = data_arr.base();
                    dg_sock::network_stack_allocation::NoExceptAllocation<Request[]> request_arr(sz);
                    dg_sock::network_stack_allocation::NoExceptAllocation<Response[]> response_arr(sz);

                    for (size_t i = 0u; i < sz; ++i)
                    {
                        request_arr[i] = std::move(base_data_arr[i].request);
                    }

                    RequestHandlerInterface * resource_handler  = this->request_handler_map->get_resolver(local_uri_path);
                    RequestFiltererInterface * request_filterer = this->request_filterer_map->get_filterer(local_uri_path);

                    if constexpr(DEBUG_MODE_FLAG)
                    {
                        if (resource_handler == nullptr)
                        {
                            dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                            std::abort();
                        }
                    }

                    //get the responses for the requests, this cannot fail, so there is no std::expected<Response, exception_t>
                    //a fail of this request could denote a unique_write good resource leak, not bad leak
                    //the contract of worst case == that of one normal request signal remains 

                    this->handle(std::make_move_iterator(request_arr.get()), sz, response_arr.get(),
                                 resource_handler, request_filterer);

                    for (size_t i = 0u; i < sz; ++i)
                    {
                        //attempt to write the cache if there is a designated cache_id, if cache_write is failed, it is a silent fail
                        //we are still honoring the contract of all fails == one normal request signal fail 

                        if (base_data_arr[i].cache_write_id.has_value())
                        {
                            std::expected<Response, exception_t> cpy_response_arr = dg_sock::network_exception::cstyle_initialize<Response>(response_arr[i]);

                            if (!cpy_response_arr.has_value())
                            {
                                dg_sock::network_log_stackdump::error_fast_optional(dg_sock::network_exception::verbose(cpy_response_arr.error()));
                            }
                            else
                            {
                                auto cache_mapfeed_arg = InternalCacheMapFeedArgument{.cache_id = base_data_arr[i].cache_write_id.value(),
                                                                                      .response = std::move(cpy_response_arr.value())}; 

                                dg_sock::network_producer_consumer::delvrsrv_deliver(this->cache_map_feeder, std::move(cache_mapfeed_arg));
                            }
                        }

                        //returns the result to the user

                        auto response   = model::InternalResponse{.response   = std::move(response_arr[i]),
                                                                  .ticket_id  = base_data_arr[i].ticket_id}; 

                        auto prep_arg   = InternalMailBoxPrepFeedArgument{.to               = base_data_arr[i].to,
                                                                          .response         = std::move(response)};

                        dg_sock::network_producer_consumer::delvrsrv_deliver(this->mailbox_prep_feeder, std::move(prep_arg));
                    }
                }

                void handle(std::move_iterator<Request *> request_arr, size_t sz, Response * response_arr,
                            RequestHandlerInterface * resource_handler,
                            RequestFiltererInterface * resource_filterer) noexcept
                {
                    //this is complicated, we dont want to complicate this even though this should be another feeder
                    //I would settle for this just because this is easy to implement

                    dg_sock::network_stack_allocation::NoExceptAllocation<Request[]> thru_request_arr(sz);
                    dg_sock::network_stack_allocation::NoExceptAllocation<Response[]> thru_response_arr(sz);
                    dg_sock::network_stack_allocation::NoExceptAllocation<std::add_pointer_t<Response>[]> corresponding_response_ptr_arr(sz);

                    size_t thru_sz              = 0u;
                    Request * base_request_arr  = request_arr.base();

                    for (size_t i = 0u; i < sz; ++i)
                    {
                        try
                        {
                            exception_t err = resource_filterer->thru(base_request_arr[i]);

                            if (dg_sock::network_exception::is_failed(err))
                            {
                                dg_sock::network_exception::throw_exception(err);
                            }
                        }
                        catch (...)
                        {
                            response_arr[i] = Response
                            {
                                .response                       = {},
                                .response_serialization_format  = {},
                                .err_code                       = dg_sock::network_exception::wrap_std_exception(std::current_exception())
                            };

                            continue;
                        }

                        thru_request_arr[thru_sz]               = std::move(base_request_arr[i]);
                        corresponding_response_ptr_arr[thru_sz] = std::next(response_arr, i);
                        thru_sz                                 += 1u;
                    }

                    resource_handler->handle(std::make_move_iterator(thru_request_arr.get()), thru_sz, thru_response_arr.get());

                    for (size_t i = 0u; i < thru_sz; ++i)
                    {
                        *corresponding_response_ptr_arr[i] = std::move(thru_response_arr[i]);
                    }
                }
            };

            struct InternalCacheServerFeedArgument
            {
                dg_sock::network_kernel_mailbox::Address to;
                dg_sock::string local_uri_path;
                cache_id_t cache_id;
                ticket_id_t ticket_id;
                Request request;
            };

            struct InternalCacheServerFeedResolutor: dg_sock::network_producer_consumer::ConsumerInterface<InternalCacheServerFeedArgument>
            {
                InfiniteCacheUniqueWriteControllerInterface * cachewrite_uex_controller;
                CacheUniqueWriteTrafficControllerInterface * cachewrite_tfx_controller;
                dg_sock::network_producer_consumer::KVDeliveryHandle<dg_sock::string, InternalServerFeedResolutorArgument> * server_feeder;
                dg_sock::network_producer_consumer::DeliveryHandle<InternalMailBoxPrepFeedArgument> * mailbox_prep_feeder;

                void push(std::move_iterator<InternalCacheServerFeedArgument *> data_arr, size_t sz) noexcept
                {
                    auto base_data_arr = data_arr.base();

                    dg_sock::network_stack_allocation::NoExceptAllocation<cache_id_t[]> cache_id_arr(sz);
                    dg_sock::network_stack_allocation::NoExceptAllocation<std::expected<bool, exception_t>[]> cache_write_response_arr(sz);

                    for (size_t i = 0u; i < sz; ++i)
                    {
                        cache_id_arr[i] = base_data_arr[i].cache_id;
                    }

                    std::expected<bool, exception_t> thru_naive_status = this->cachewrite_tfx_controller->thru(sz); //if it is already uex_controller->thru, we got a leak, its interval leak, the crack between the read_cache and the thru, if it is not thru, then we are logically correct 
                    exception_t thru_status = {}; 

                    if (!thru_naive_status.has_value())
                    {
                        thru_status = thru_naive_status.error();
                    }
                    else
                    {
                        if (!thru_naive_status.value())
                        {
                            thru_status = dg_sock::network_exception::REST_CACHE_POPULATION_LIMIT_REACHED;
                        }
                    }

                    //not thru, returns bad signal

                    if (dg_sock::network_exception::is_failed(thru_status))
                    {
                        for (size_t i = 0u; i < sz; ++i)
                        {
                            auto response = model::InternalResponse{.response   = std::unexpected(thru_status),
                                                                    .ticket_id  = base_data_arr[i].ticket_id};

                            auto prep_arg   = InternalMailBoxPrepFeedArgument{.to               = base_data_arr[i].to,
                                                                              .response         = std::move(response)};

                            dg_sock::network_producer_consumer::delvrsrv_deliver(this->mailbox_prep_feeder, std::move(prep_arg));
                        }

                        return;
                    }

                    //thru, attempts to get the unique write 

                    this->cachewrite_uex_controller->thru(cache_id_arr.get(), sz, cache_write_response_arr.get());

                    for (size_t i = 0u; i < sz; ++i)
                    {
                        //internal server error, can't read the thruness
                        //we need to somewhat generalize our external error interface ... this is too confusing even for me

                        if (!cache_write_response_arr[i].has_value())
                        {
                            auto response = model::InternalResponse{.response   = std::unexpected(dg_sock::network_exception::REST_INTERNAL_SERVER_ERROR),
                                                                    .ticket_id  = base_data_arr[i].ticket_id};

                            auto prep_arg   = InternalMailBoxPrepFeedArgument{.to               = base_data_arr[i].to,
                                                                              .response         = std::move(response)};

                            dg_sock::network_producer_consumer::delvrsrv_deliver(this->mailbox_prep_feeder, std::move(prep_arg));
                            continue;
                        }

                        //somebody else gets the cache_write, we aren't unique...

                        if (!cache_write_response_arr[i].value())
                        {
                            auto response   = model::InternalResponse{.response   = std::unexpected(dg_sock::network_exception::REST_BAD_CACHE_UNIQUE_WRITE),
                                                                      .ticket_id  = base_data_arr[i].ticket_id};

                            auto prep_arg   = InternalMailBoxPrepFeedArgument{.to               = base_data_arr[i].to,
                                                                              .response         = std::move(response)};

                            dg_sock::network_producer_consumer::delvrsrv_deliver(this->mailbox_prep_feeder, std::move(prep_arg));
                            continue;
                        }

                        //we have the cache_write right, we'd make sure to dispatch this to the server feed resolutor to get a response and cache the response

                        auto arg    = InternalServerFeedResolutorArgument{.to               = base_data_arr[i].to,
                                                                          .cache_write_id   = base_data_arr[i].cache_id,
                                                                          .ticket_id        = base_data_arr[i].ticket_id,
                                                                          .request          = std::move(base_data_arr[i].request)};

                        dg_sock::network_producer_consumer::delvrsrv_kv_deliver(this->server_feeder, base_data_arr[i].local_uri_path, std::move(arg));
                    }
                }
            };

            struct InternalCacheFeedArgument
            {
                dg_sock::network_kernel_mailbox::Address to;
                dg_sock::string local_uri_path;
                cache_id_t cache_id;
                ticket_id_t ticket_id;
                Request request;
            };

            struct InternalCacheFeedResolutor: dg_sock::network_producer_consumer::ConsumerInterface<InternalCacheFeedArgument>
            {
                dg_sock::network_producer_consumer::DeliveryHandle<InternalCacheServerFeedArgument> * cache_server_feeder;
                dg_sock::network_producer_consumer::DeliveryHandle<InternalMailBoxPrepFeedArgument> * mailbox_prep_feeder; 
                InfiniteCacheControllerInterface * cache_controller;

                void push(std::move_iterator<InternalCacheFeedArgument *> data_arr, size_t sz) noexcept
                {
                    auto base_data_arr = data_arr.base();

                    dg_sock::network_stack_allocation::NoExceptAllocation<cache_id_t[]> cache_id_arr(sz);
                    dg_sock::network_stack_allocation::NoExceptAllocation<std::expected<std::optional<Response>, exception_t>[]> response_arr(sz);

                    for (size_t i = 0u; i < sz; ++i)
                    {
                        cache_id_arr[i] = base_data_arr[i].cache_id;                        
                    }

                    this->cache_controller->get_cache(cache_id_arr.get(), sz, response_arr.get());

                    for (size_t i = 0u; i < sz; ++i)
                    {
                        if (!response_arr[i].has_value())
                        {
                            auto response   = model::InternalResponse{.response     = std::unexpected(dg_sock::network_exception::REST_INTERNAL_SERVER_ERROR),
                                                                      .ticket_id    = base_data_arr[i].ticket_id};

                            auto prep_arg   = InternalMailBoxPrepFeedArgument{.to               = base_data_arr[i].to,
                                                                              .response         = std::move(response)};

                            dg_sock::network_producer_consumer::delvrsrv_deliver(this->mailbox_prep_feeder, std::move(prep_arg));
                            continue;
                        }

                        if (response_arr[i].value().has_value())
                        {
                            auto response = model::InternalResponse{.response   = std::move(response_arr[i].value().value()),
                                                                    .ticket_id  = base_data_arr[i].ticket_id};

                            auto prep_arg   = InternalMailBoxPrepFeedArgument{.to               = base_data_arr[i].to,
                                                                              .response         = std::move(response)};

                            dg_sock::network_producer_consumer::delvrsrv_deliver(this->mailbox_prep_feeder, std::move(prep_arg));
                            continue;
                        }

                        auto feed_arg    = InternalCacheServerFeedArgument{.to               = base_data_arr[i].to,
                                                                           .local_uri_path   = base_data_arr[i].local_uri_path,
                                                                           .cache_id         = base_data_arr[i].cache_id,
                                                                           .ticket_id        = base_data_arr[i].ticket_id,
                                                                           .request          = std::move(base_data_arr[i].request)};

                        dg_sock::network_producer_consumer::delvrsrv_deliver(this->cache_server_feeder, std::move(feed_arg));
                    }
                }
            };
    };

    class ComponentFactory
    {
        private:

            static auto get_cache_controller(size_t map_capacity,
                                             size_t response_capacity) -> std::unique_ptr<InfiniteCacheControllerInterface>
            {
                const size_t MIN_MAP_CAPACITY       = 1u;
                const size_t MAX_MAP_CAPACITY       = size_t{1} << 40;
                const size_t MIN_RESPONSE_CAPACITY  = 0u;
                const size_t MAX_RESPONSE_CAPACITY  = size_t{1} << 40; 
                const size_t CONSUME_DECAY_FACTOR   = 4u;

                if (std::clamp(map_capacity, MIN_MAP_CAPACITY, MAX_MAP_CAPACITY) != map_capacity)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                if (std::clamp(response_capacity, MIN_RESPONSE_CAPACITY, MAX_RESPONSE_CAPACITY) != response_capacity)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                size_t tentative_consume_sz_per_load    = map_capacity >> CONSUME_DECAY_FACTOR;
                const size_t MIN_CONSUME_SZ_PER_LOAD    = 1u;
                size_t consume_sz_per_load              = std::max(tentative_consume_sz_per_load, MIN_CONSUME_SZ_PER_LOAD);

                return std::make_unique<AdvancedInfiniteCacheController>(dg_sock::cyclic_unordered_node_map<cache_id_t, Response>(map_capacity),
                                                                         response_capacity,
                                                                         consume_sz_per_load);
            } 

            static auto get_mutex_controlled_cache_controller(std::unique_ptr<InfiniteCacheControllerInterface>&& arg) -> std::unique_ptr<InfiniteCacheControllerInterface>
            {
                if (arg == nullptr)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                return std::make_unique<MutexControlledInfiniteCacheController>(std::move(arg),
                                                                                stdxx::make_unique_fair_atomic_flag());
            }

            static auto get_cache_unique_write_controller(size_t set_capacity) -> std::unique_ptr<InfiniteCacheUniqueWriteControllerInterface>
            {
                const size_t MIN_SET_CAPACITY       = 1u;
                const size_t MAX_SET_CAPACITY       = size_t{1} << 40;
                const size_t CONSUME_DECAY_FACTOR   = 4u;

                if (std::clamp(set_capacity, MIN_SET_CAPACITY, MAX_SET_CAPACITY) != set_capacity)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                size_t tentative_consume_sz_per_load    = set_capacity >> CONSUME_DECAY_FACTOR;
                const size_t MIN_CONSUME_SZ_PER_LOAD    = 1u;
                size_t consume_sz_per_load              = std::max(tentative_consume_sz_per_load, MIN_CONSUME_SZ_PER_LOAD);

                return std::make_unique<AdvancedInfiniteCacheUniqueWriteController>(dg_sock::cyclic_unordered_node_set<cache_id_t>(set_capacity),
                                                                                    consume_sz_per_load);
            }

            static auto get_mutex_controlled_cache_unique_write_controller(std::unique_ptr<InfiniteCacheUniqueWriteControllerInterface>&& arg) -> std::unique_ptr<InfiniteCacheUniqueWriteControllerInterface>
            {
                if (arg == nullptr)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                return std::make_unique<MutexControlledInfiniteCacheWriteExclusionController>(std::move(arg),
                                                                                              stdxx::make_unique_fair_atomic_flag());
            }

            static auto get_cache_write_traffic_controller(size_t thru_cap) -> std::unique_ptr<CacheUniqueWriteTrafficControllerInterface>
            {
                return std::make_unique<CacheUniqueWriteTrafficController>(thru_cap);
            }

        public:

            static auto get_request_handler_dictionary() -> std::unique_ptr<RequestHandlerDictionaryInterface>
            {
                return std::make_unique<RequestHandlerDictionary>();
            }

            static auto get_request_handler_one_fetch_retriever(std::shared_ptr<RequestHandlerDictionaryInterface> arg) -> std::unique_ptr<RequestHandlerRetrieverInterface>
            {
                if (arg == nullptr)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                return std::make_unique<OneFetchRequestHandlerRetriever>(std::move(arg));
            }

            static auto get_request_filterer_dictionary() -> std::unique_ptr<RequestFiltererDictionaryInterface>
            {
                return std::make_unique<RequestFiltererDictionary>();
            }

            static auto get_request_filterer_one_fetch_retriever(std::shared_ptr<RequestFiltererDictionaryInterface> arg) -> std::unique_ptr<RequestFiltererRetrieverInterface>
            {
                if (arg == nullptr)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                return std::make_unique<OneFetchRequestFiltererRetriever>(std::move(arg));
            }

            static auto get_distributed_cache_controller(size_t map_capacity,
                                                         size_t response_capacity,
                                                         size_t concurrency_sz) -> std::unique_ptr<InfiniteCacheControllerInterface>
            {
                const size_t MIN_CONCURRENCY_SZ = 1u;
                const size_t MAX_CONCURRENCY_SZ = size_t{1} << 30;
                const size_t KEYVALUE_FEED_CAP  = size_t{1} << 6;

                if (std::clamp(concurrency_sz, MIN_CONCURRENCY_SZ, MAX_CONCURRENCY_SZ) != concurrency_sz)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                if (!stdxx::is_pow2(concurrency_sz))
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                std::unique_ptr<std::unique_ptr<InfiniteCacheControllerInterface>[]> cache_controller_arr = std::make_unique<std::unique_ptr<InfiniteCacheControllerInterface>[]>(concurrency_sz);

                for (size_t i = 0u; i < concurrency_sz; ++i)
                {
                    cache_controller_arr[i] = get_mutex_controlled_cache_controller(get_cache_controller(map_capacity, response_capacity));
                }

                size_t response_sz  = cache_controller_arr[0]->max_response_size();
                size_t consume_sz   = cache_controller_arr[0]->max_consume_size();
                
                return std::make_unique<DistributedCacheController>(std::move(cache_controller_arr),
                                                                    concurrency_sz,
                                                                    KEYVALUE_FEED_CAP,
                                                                    KEYVALUE_FEED_CAP,
                                                                    consume_sz,
                                                                    response_sz);
            }

            static auto get_distributed_unique_cache_write_right_controller(size_t set_capacity,
                                                                            size_t concurrency_sz) -> std::unique_ptr<InfiniteCacheUniqueWriteControllerInterface>
            {
                const size_t MIN_CONCURRENCY_SZ = 1u;
                const size_t MAX_CONCURRENCY_SZ = size_t{1} << 30;
                const size_t KEYVALUE_FEED_CAP  = size_t{1} << 6;

                if (std::clamp(concurrency_sz, MIN_CONCURRENCY_SZ, MAX_CONCURRENCY_SZ) != concurrency_sz)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                if (!stdxx::is_pow2(concurrency_sz))
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                std::unique_ptr<std::unique_ptr<InfiniteCacheUniqueWriteControllerInterface>[]> base_arr = std::make_unique<std::unique_ptr<InfiniteCacheUniqueWriteControllerInterface>[]>(concurrency_sz);

                for (size_t i = 0u; i < concurrency_sz; ++i)
                {
                    base_arr[i] = get_mutex_controlled_cache_unique_write_controller(get_cache_unique_write_controller(set_capacity));
                }

                size_t consume_sz   = base_arr[0]->max_consume_size();

                return std::make_unique<DistributedUniqueCacheWriteController>(std::move(base_arr),
                                                                               concurrency_sz,
                                                                               KEYVALUE_FEED_CAP,
                                                                               consume_sz);
            }

            static auto get_distributed_cache_write_traffic_controller(size_t elemental_thru_cap,
                                                                       size_t concurrency_sz) -> std::unique_ptr<CacheUniqueWriteTrafficControllerInterface>
            {
                const size_t MIN_CONCURRENCY_SZ = 1u;
                const size_t MAX_CONCURRENCY_SZ = size_t{1} << 30;
                
                if (std::clamp(concurrency_sz, MIN_CONCURRENCY_SZ, MAX_CONCURRENCY_SZ) != concurrency_sz)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                if (!stdxx::is_pow2(concurrency_sz))
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                std::unique_ptr<std::unique_ptr<CacheUniqueWriteTrafficControllerInterface>[]> base_arr = std::make_unique<std::unique_ptr<CacheUniqueWriteTrafficControllerInterface>[]>(concurrency_sz);

                for (size_t i = 0u; i < concurrency_sz; ++i)
                {
                    base_arr[i] = get_cache_write_traffic_controller(elemental_thru_cap);
                }

                size_t max_thru_sz = base_arr[0]->thru_capacity();
                
                return std::make_unique<DistributedCacheUniqueWriteTrafficController>(std::move(base_arr),
                                                                                      concurrency_sz,
                                                                                      max_thru_sz);
            }

            static auto get_self_update_traffic_controller(std::unique_ptr<CacheUniqueWriteTrafficControllerInterface> base,
                                                           std::chrono::nanoseconds reset_duration) -> std::unique_ptr<CacheUniqueWriteTrafficControllerInterface>
            {
                return std::make_unique<SelfUpdateCacheUniqueWriteTrafficController>(std::move(base),
                                                                                     reset_duration);
            }

            static auto get_one_request_adapter(std::shared_ptr<OneRequestHandlerInterface> request_handler) -> std::unique_ptr<RequestHandlerInterface>
            {
                if (request_handler == nullptr)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                return std::make_unique<OneRequestHandlerAdapter>(std::move(request_handler));
            }

            static auto get_request_resolver(std::shared_ptr<RequestHandlerRetrieverInterface> request_handler_map,
                                             std::shared_ptr<RequestFiltererRetrieverInterface> request_filterer_map,
                                             std::shared_ptr<InfiniteCacheControllerInterface> cache_controller,
                                             std::shared_ptr<InfiniteCacheUniqueWriteControllerInterface> cache_write_controller,
                                             std::shared_ptr<CacheUniqueWriteTrafficControllerInterface> traffic_controller,
                                             uint32_t recv_channel,
                                             uint32_t send_channel,
                                             size_t resolve_consume_sz,
                                             size_t mailbox_feed_cap,
                                             size_t mailbox_prep_feed_cap,
                                             size_t cache_controller_feed_cap,
                                             size_t server_fetch_feed_cap,
                                             size_t cache_server_fetch_feed_cap,
                                             size_t cache_fetch_feed_cap,
                                             size_t busy_consume_sz) -> std::unique_ptr<dg_sock::network_concurrency::WorkerInterface>
            {
                const size_t MIN_RESOLVE_CONSUME_SZ = 1u;
                const size_t MAX_RESOLVE_CONSUME_SZ = size_t{1} << 30;
                const size_t MIN_FEED_CAP           = 1u;
                const size_t MAX_FEED_CAP           = size_t{1} << 30;
                const size_t MIN_BUSY_CONSUME_SZ    = 0u;
                const size_t MAX_BUSY_CONSUME_SZ    = size_t{1} << 30;

                if (request_handler_map == nullptr)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                if (request_filterer_map == nullptr)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                if (cache_controller == nullptr)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                if (cache_write_controller == nullptr)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                if (traffic_controller == nullptr)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                if (std::clamp(resolve_consume_sz, MIN_RESOLVE_CONSUME_SZ, MAX_RESOLVE_CONSUME_SZ) != resolve_consume_sz)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                if (std::clamp(mailbox_feed_cap, MIN_FEED_CAP, MAX_FEED_CAP) != mailbox_feed_cap)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                if (std::clamp(mailbox_prep_feed_cap, MIN_FEED_CAP, MAX_FEED_CAP) != mailbox_prep_feed_cap)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                if (std::clamp(cache_controller_feed_cap, MIN_FEED_CAP, MAX_FEED_CAP) != cache_controller_feed_cap)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                if (std::clamp(server_fetch_feed_cap, MIN_FEED_CAP, MAX_FEED_CAP) != server_fetch_feed_cap)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                if (std::clamp(cache_server_fetch_feed_cap, MIN_FEED_CAP, MAX_FEED_CAP) != cache_server_fetch_feed_cap)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                if (std::clamp(cache_fetch_feed_cap, MIN_FEED_CAP, MAX_FEED_CAP) != cache_fetch_feed_cap)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                if (std::clamp(busy_consume_sz, MIN_BUSY_CONSUME_SZ, MAX_BUSY_CONSUME_SZ) != busy_consume_sz)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                return std::make_unique<RequestResolverWorker>(std::move(request_handler_map),
                                                               std::move(request_filterer_map),
                                                               std::move(cache_controller),
                                                               std::move(cache_write_controller),
                                                               std::move(traffic_controller),
                                                               recv_channel,
                                                               send_channel,
                                                               resolve_consume_sz,
                                                               mailbox_feed_cap,
                                                               mailbox_prep_feed_cap,
                                                               cache_controller_feed_cap,
                                                               server_fetch_feed_cap,
                                                               cache_server_fetch_feed_cap,
                                                               cache_fetch_feed_cap,
                                                               busy_consume_sz);
            }
    };
}

namespace dg_sock::network_rest_frame::server_instance
{
    using namespace dg_sock::network_rest_frame::server;

    struct BuilderConfig
    {
        uint64_t cache_each_capacity;
        uint64_t cache_response_capacity;
        uint64_t cache_concurrency_sz;

        uint64_t cache_unique_write_set_each_capacity;
        uint64_t cache_unique_write_set_concurrency_sz;

        uint32_t recv_channel;
        uint32_t send_channel;

        uint64_t cache_unique_write_traffic_controller_elemental_thru_cap;
        uint64_t cache_unique_write_traffic_controller_concurrency_sz;
        std::chrono::nanoseconds cache_unique_write_traffic_controller_reset_duration;

        uint64_t request_resolver_worker_sz;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(cache_each_capacity,
                      cache_response_capacity,
                      cache_concurrency_sz,
                      cache_unique_write_set_each_capacity,
                      cache_unique_write_set_concurrency_sz,
                      cache_unique_write_traffic_controller_elemental_thru_cap,
                      cache_unique_write_traffic_controller_concurrency_sz,
                      cache_unique_write_traffic_controller_reset_duration,
                      request_resolver_worker_sz);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(cache_each_capacity,
                      cache_response_capacity,
                      cache_concurrency_sz,
                      cache_unique_write_set_each_capacity,
                      cache_unique_write_set_concurrency_sz,
                      cache_unique_write_traffic_controller_elemental_thru_cap,
                      cache_unique_write_traffic_controller_concurrency_sz,
                      cache_unique_write_traffic_controller_reset_duration,
                      request_resolver_worker_sz);
        }
    };

    struct RestServerSolution
    {
        std::shared_ptr<RequestHandlerDictionaryInterface> rest_resolver_dictionary;
        std::shared_ptr<RequestFiltererDictionaryInterface> request_filterer_dictionary;
        std::shared_ptr<void> daemon_process;
    };

    class RestServerBuilder
    {
        private:

            uint64_t cache_each_capacity;
            uint64_t cache_response_capacity;
            uint64_t cache_concurrency_sz;

            uint64_t cache_unique_write_set_each_capacity;
            uint64_t cache_unique_write_set_concurrency_sz;

            uint32_t recv_channel;
            uint32_t send_channel;

            uint64_t cache_unique_write_traffic_controller_elemental_thru_cap;
            uint64_t cache_unique_write_traffic_controller_concurrency_sz;
            std::chrono::nanoseconds cache_unique_write_traffic_controller_reset_duration;

            uint64_t request_resolver_consume_sz;
            uint64_t request_resolver_mailbox_feed_cap;
            uint64_t request_resolver_mailbox_prep_feed_cap;
            uint64_t request_resolver_cache_controller_feed_cap;
            uint64_t request_resolver_server_fetch_feed_cap;
            uint64_t request_resolver_cache_server_fetch_feed_cap;
            uint64_t request_resolver_cache_fetch_feed_cap;
            uint64_t request_resolver_busy_consume_sz;

            uint64_t request_resolver_worker_sz;

            static inline constexpr size_t DEFAULT_BATCH_SZ         = size_t{1} << 8;
            static inline constexpr size_t DEFAULT_BUSY_CONSUME_SZ  = 0u;

        public:

            RestServerBuilder(): cache_each_capacity(),
                                 cache_response_capacity(),
                                 cache_concurrency_sz(),
                                 cache_unique_write_set_each_capacity(),
                                 cache_unique_write_set_concurrency_sz(),
                                 cache_unique_write_traffic_controller_elemental_thru_cap(),
                                 cache_unique_write_traffic_controller_concurrency_sz(),
                                 request_resolver_consume_sz(DEFAULT_BATCH_SZ),
                                 request_resolver_mailbox_feed_cap(DEFAULT_BATCH_SZ),
                                 request_resolver_mailbox_prep_feed_cap(DEFAULT_BATCH_SZ),
                                 request_resolver_cache_controller_feed_cap(DEFAULT_BATCH_SZ),
                                 request_resolver_server_fetch_feed_cap(DEFAULT_BATCH_SZ),
                                 request_resolver_cache_server_fetch_feed_cap(DEFAULT_BATCH_SZ),
                                 request_resolver_cache_fetch_feed_cap(DEFAULT_BATCH_SZ),
                                 request_resolver_busy_consume_sz(DEFAULT_BUSY_CONSUME_SZ),
                                 request_resolver_worker_sz(){}


            auto set_config(const BuilderConfig& config) -> RestServerBuilder&
            {
                this->cache_each_capacity                                       = config.cache_each_capacity;
                this->cache_response_capacity                                   = config.cache_response_capacity;
                this->cache_concurrency_sz                                      = config.cache_concurrency_sz;

                this->cache_unique_write_set_each_capacity                      = config.cache_unique_write_set_each_capacity;
                this->cache_unique_write_set_concurrency_sz                     = config.cache_unique_write_set_concurrency_sz;

                this->recv_channel                                              = config.recv_channel;
                this->send_channel                                              = config.send_channel;

                this->cache_unique_write_traffic_controller_elemental_thru_cap  = config.cache_unique_write_traffic_controller_elemental_thru_cap;
                this->cache_unique_write_traffic_controller_concurrency_sz      = config.cache_unique_write_traffic_controller_concurrency_sz;
                this->cache_unique_write_traffic_controller_reset_duration      = config.cache_unique_write_traffic_controller_reset_duration;

                this->request_resolver_worker_sz                                = config.request_resolver_worker_sz;

                return *this;
            }

            auto build() -> RestServerSolution
            {
                std::shared_ptr<RequestHandlerDictionaryInterface> handler_dict     = this->get_request_handler_dictionary();
                std::shared_ptr<RequestFiltererDictionaryInterface> filterer_dict   = this->get_request_filterer_dictionary();
                std::shared_ptr<void> daemon_process                                = this->get_daemon_process(handler_dict, filterer_dict);

                return RestServerSolution
                {
                    .rest_resolver_dictionary       = handler_dict,
                    .request_filterer_dictionary    = filterer_dict,
                    .daemon_process                 = daemon_process
                };
            }

        private:

            auto run_workable(std::unique_ptr<dg_sock::network_concurrency::WorkerInterface> workable) -> std::shared_ptr<void>
            {
                auto resource = dg_sock::network_exception_handler::throw_nolog(dg_sock::network_concurrency::daemon_saferegister(dg_sock::network_concurrency::REST_SERVER_DAEMON, std::move(workable))); 

                return std::make_shared<decltype(resource)>(std::move(resource));
            }

            auto run_workable_vec(std::vector<std::unique_ptr<dg_sock::network_concurrency::WorkerInterface>> workable_vec) -> std::shared_ptr<void>
            {
                std::vector<std::shared_ptr<void>> rs{};

                for (auto& workable: workable_vec)
                {
                    rs.push_back(this->run_workable(std::move(workable)));
                }

                return std::make_shared<std::vector<std::shared_ptr<void>>>(std::move(rs));
            }

            auto get_request_handler_dictionary() -> std::unique_ptr<RequestHandlerDictionaryInterface>
            {
                return dg_sock::network_rest_frame::server_impl1::ComponentFactory::get_request_handler_dictionary();
            }

            auto get_request_filterer_dictionary() -> std::unique_ptr<RequestFiltererDictionaryInterface>
            {
                return dg_sock::network_rest_frame::server_impl1::ComponentFactory::get_request_filterer_dictionary();
            }

            auto get_cache_controller() -> std::unique_ptr<InfiniteCacheControllerInterface>
            {
                return dg_sock::network_rest_frame::server_impl1::ComponentFactory::get_distributed_cache_controller(this->cache_each_capacity,
                                                                                                                     this->cache_response_capacity,
                                                                                                                     this->cache_concurrency_sz);
            }

            auto get_cache_unique_write_controller() -> std::unique_ptr<InfiniteCacheUniqueWriteControllerInterface>
            {
                return dg_sock::network_rest_frame::server_impl1::ComponentFactory::get_distributed_unique_cache_write_right_controller(this->cache_unique_write_set_each_capacity,
                                                                                                                                        this->cache_unique_write_set_concurrency_sz);
            }

            auto get_traffic_controller() -> std::unique_ptr<CacheUniqueWriteTrafficControllerInterface>
            {
                using namespace dg_sock::network_rest_frame::server_impl1;

                return ComponentFactory::get_self_update_traffic_controller(ComponentFactory::get_distributed_cache_write_traffic_controller(this->cache_unique_write_traffic_controller_elemental_thru_cap,
                                                                                                                                             this->cache_unique_write_traffic_controller_concurrency_sz),
                                                                            this->cache_unique_write_traffic_controller_reset_duration);
            }

            auto get_request_resolver_worker(const std::shared_ptr<RequestHandlerDictionaryInterface>& dictionary,
                                             const std::shared_ptr<RequestFiltererDictionaryInterface>& filterer_dictionary,
                                             const std::shared_ptr<InfiniteCacheControllerInterface>& cache_controller,
                                             const std::shared_ptr<InfiniteCacheUniqueWriteControllerInterface>& cache_unique_write_controller,
                                             const std::shared_ptr<CacheUniqueWriteTrafficControllerInterface>& traffic_controller) -> std::unique_ptr<dg_sock::network_concurrency::WorkerInterface>
            {
                using namespace dg_sock::network_rest_frame::server_impl1;

                return ComponentFactory::get_request_resolver(ComponentFactory::get_request_handler_one_fetch_retriever(dictionary),
                                                              ComponentFactory::get_request_filterer_one_fetch_retriever(filterer_dictionary),
                                                              cache_controller,
                                                              cache_unique_write_controller,
                                                              traffic_controller,
                                                              this->recv_channel,
                                                              this->send_channel,
                                                              this->request_resolver_consume_sz,
                                                              this->request_resolver_mailbox_feed_cap,
                                                              this->request_resolver_mailbox_prep_feed_cap,
                                                              this->request_resolver_cache_controller_feed_cap,
                                                              this->request_resolver_server_fetch_feed_cap,
                                                              this->request_resolver_cache_server_fetch_feed_cap,
                                                              this->request_resolver_cache_fetch_feed_cap,
                                                              this->request_resolver_busy_consume_sz);
            }

            auto get_daemon_process(const std::shared_ptr<RequestHandlerDictionaryInterface>& dictionary,
                                    const std::shared_ptr<RequestFiltererDictionaryInterface>& filterer_dictionary) -> std::shared_ptr<void>
            {
                std::vector<std::unique_ptr<dg_sock::network_concurrency::WorkerInterface>> worker_vec      = {};

                std::shared_ptr<InfiniteCacheControllerInterface> cache_controller                          = this->get_cache_controller();
                std::shared_ptr<InfiniteCacheUniqueWriteControllerInterface> cache_unique_write_controller  = this->get_cache_unique_write_controller();
                std::shared_ptr<CacheUniqueWriteTrafficControllerInterface> traffic_controller              = this->get_traffic_controller();

                for (size_t i = 0u; i < this->request_resolver_worker_sz; ++i)
                {
                    worker_vec.push_back(this->get_request_resolver_worker(dictionary, filterer_dictionary,
                                                                           cache_controller, cache_unique_write_controller,
                                                                           traffic_controller));
                }

                return this->run_workable_vec(std::move(worker_vec));
            }
    };

    struct Signature{};

    using SolutionSingleton   = stdxx::singleton<Signature, RestServerSolution>;

    void init(const BuilderConfig& config)
    {
        SolutionSingleton::get()    = RestServerBuilder{}.set_config(config).build();
        std::atomic_thread_fence(std::memory_order_seq_cst);
    }

    void deinit() noexcept
    {
        std::atomic_thread_fence(std::memory_order_seq_cst);
        SolutionSingleton::get()    = {};
    }

    void bind_filter(const ResourceAddress& resource_addr, std::shared_ptr<RequestFiltererInterface> filterer)
    {
        dg_sock::network_exception_handler::nothrow_log(SolutionSingleton::get().request_filterer_dictionary->add_filterer(resource_addr.resource_addr, filterer));
    }

    void hook(const ResourceAddress& resource_addr, std::shared_ptr<OneRequestHandlerInterface> request_handler)
    {
        dg_sock::network_exception_handler::nothrow_log(SolutionSingleton::get().rest_resolver_dictionary->add_resolver(resource_addr.resource_addr,
                                                                                                                        server_impl1::ComponentFactory::get_one_request_adapter(request_handler)));
    }

    void hook(const ResourceAddress& resource_addr, std::shared_ptr<RequestHandlerInterface> request_handler)
    {
        dg_sock::network_exception_handler::nothrow_log(SolutionSingleton::get().rest_resolver_dictionary->add_resolver(resource_addr.resource_addr, request_handler));
    }

    void unhook(const ResourceAddress& resource_addr) noexcept
    {
        SolutionSingleton::get().rest_resolver_dictionary->remove_resolver(resource_addr.resource_addr);
    }
}

namespace dg_sock::network_rest_frame::client_impl1{

    using namespace dg_sock::network_rest_frame::client; 

    static inline auto request_id_to_cache_id(const RequestID& request_id) noexcept -> CacheID
    {
        return CacheID
        {
            .ip = request_id.ip,
            .native_cache_id = request_id.native_request_id
        };
    }

    template <class T, std::enable_if_t<std::is_unsigned_v<T>, bool> = true>
    static inline auto to_id_storage(T value) noexcept -> native_id_storage_t
    {
        constexpr size_t VALUE_TRIVIAL_SIZE = dg_sock::network_trivial_serializer::size(T{}); 
        static_assert(VALUE_TRIVIAL_SIZE <= native_id_storage_t{}.size());

        native_id_storage_t result{}; 
        dg_sock::network_trivial_serializer::serialize_into(result.data(), value);

        return result;
    } 

    //clear
    class BatchRequestResponseBase
    {
        private:

            stdxx::inplace_hdi_container<std::atomic<intmax_t>> atomic_smp;
            dg_sock::vector<std::expected<Response, exception_t>> resp_vec; //alright, there are hardware destructive interference issues, we dont want to talk about that yet
            stdxx::inplace_hdi_container<std::atomic_flag> is_response_invoked;

            static void assert_all_expected_initialized(dg_sock::vector<std::expected<Response, exception_t>>& arg) noexcept
            {
                (void) arg;

                if constexpr(DEBUG_MODE_FLAG)
                {
                    for (size_t i = 0u; i < arg.size(); ++i)
                    {
                        if (!arg[i].has_value() && arg[i].error() == dg_sock::network_exception::EXPECTED_NOT_INITIALIZED)
                        {
                            dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(arg[i].error()));
                            std::abort();
                        }
                    }
                }
            }

            using self = BatchRequestResponseBase;

        public:

            BatchRequestResponseBase(size_t resp_sz): atomic_smp(std::in_place_t{}, -static_cast<intmax_t>(stdxx::zero_throw(resp_sz)) + 1),
                                                      resp_vec(stdxx::zero_throw(resp_sz), std::unexpected(dg_sock::network_exception::EXPECTED_NOT_INITIALIZED)),
                                                      is_response_invoked(std::in_place_t{}, false){}


            auto is_completed() noexcept -> bool
            {
                return this->atomic_smp.value.load(std::memory_order_relaxed) == 1;
            }

            void update(size_t idx, std::expected<Response, exception_t> response) noexcept
            {
                this->internal_update(idx, std::move(response));
            }

            auto response() noexcept -> std::expected<dg_sock::vector<std::expected<Response, exception_t>>, exception_t>
            {
                bool was_invoked = this->is_response_invoked.value.test_and_set(std::memory_order_relaxed);

                if (was_invoked)
                {
                    return std::unexpected(dg_sock::network_exception::REST_RESPONSE_DOUBLE_INVOKE);
                }

                this->atomic_smp.value.wait(0, std::memory_order_acquire);

                if constexpr(STRONG_MEMORY_ORDERING_FLAG)
                {
                    std::atomic_thread_fence(std::memory_order_seq_cst);
                }

                self::assert_all_expected_initialized(this->resp_vec);
                auto rs = dg_sock::vector<std::expected<Response, exception_t>>(std::move(this->resp_vec));

                if constexpr(STRONG_MEMORY_ORDERING_FLAG)
                {
                    std::atomic_thread_fence(std::memory_order_seq_cst);
                }
                else
                {
                    std::atomic_thread_fence(std::memory_order_release);
                }

                return rs;
            }

        private:

            void internal_update(size_t idx, std::expected<Response, exception_t> response) noexcept
            {
                if constexpr(DEBUG_MODE_FLAG)
                {
                    if (idx >= this->resp_vec.size())
                    {
                        dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }

                    if (!response.has_value() && response.error() == dg_sock::network_exception::EXPECTED_NOT_INITIALIZED)
                    {
                        dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }

                    if (this->resp_vec[idx].has_value())
                    {
                        dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                this->resp_vec[idx] = std::move(response);

                if constexpr(STRONG_MEMORY_ORDERING_FLAG)
                {
                    std::atomic_thread_fence(std::memory_order_seq_cst);
                }

                intmax_t old = this->atomic_smp.value.fetch_add(1, std::memory_order_release);

                if constexpr(DEBUG_MODE_FLAG)
                {
                    if (old > 0)
                    {
                        dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                if (old == 0)
                {
                    this->atomic_smp.value.notify_one();
                }
            }
    };

    //clear
    class BatchRequestResponse: public virtual BatchResponseInterface
    {
        private:

            class BatchRequestResponseBaseDesignatedObserver: public virtual ResponseObserverInterface
            {
                private:

                    std::shared_ptr<BatchRequestResponseBase> base;
                    size_t idx;

                public:

                    BatchRequestResponseBaseDesignatedObserver(std::shared_ptr<BatchRequestResponseBase> base, 
                                                               size_t idx) noexcept: base(std::move(base)),
                                                                                     idx(idx){}

                    void update(std::expected<Response, exception_t> response) noexcept
                    {
                        this->base->update(this->idx, std::move(response));
                    }
            };

            dg_sock::vector<std::shared_ptr<ResponseObserverInterface>> observer_arr; 
            std::shared_ptr<BatchRequestResponseBase> base;

        public:

            BatchRequestResponse(size_t resp_sz): observer_arr(resp_sz),
                                                  base(std::make_shared<BatchRequestResponseBase>(resp_sz))
            {
                for (size_t i = 0u; i < resp_sz; ++i)
                {
                    this->observer_arr[i] = std::make_shared<BatchRequestResponseBaseDesignatedObserver>(this->base, i);
                }
            }

            BatchRequestResponse(const BatchRequestResponse&) = delete;
            BatchRequestResponse& operator =(const BatchRequestResponse&) = delete;

            auto is_completed() noexcept -> bool
            {
                return this->base->is_completed();
            }

            auto response() noexcept -> std::expected<dg_sock::vector<std::expected<Response, exception_t>>, exception_t>
            {
                return this->base->response();
            }

            auto response_size() const noexcept -> size_t
            {
                return this->observer_arr.size();
            }

            auto get_observer(size_t idx) noexcept -> std::expected<std::shared_ptr<ResponseObserverInterface>, exception_t>
            {
                if (idx >= this->observer_arr.size())
                {
                    return std::unexpected(dg_sock::network_exception::INDEX_OUT_OF_RANGE);
                }

                return this->observer_arr[idx];
            }
    };

    //
    auto make_batch_request_response(size_t resp_sz) noexcept -> std::expected<dg_sock::unique_ptr<BatchRequestResponse>, exception_t>
    {
        return dg_sock::make_unique<BatchRequestResponse>(resp_sz);
    }

    class NonBlockingRequestContainer: public virtual RequestContainerInterface
    {
        private:

            dg_sock::pow2_cyclic_queue<dg_sock::vector<model::InternalRequest>> producer_queue;
            dg_sock::pow2_cyclic_queue<std::pair<std::binary_semaphore *, std::optional<dg_sock::vector<model::InternalRequest>> *>> waiting_queue;
            std::unique_ptr<stdxx::fair_atomic_flag> mtx;
        
        public:

            NonBlockingRequestContainer(dg_sock::pow2_cyclic_queue<dg_sock::vector<model::InternalRequest>> producer_queue,
                                        dg_sock::pow2_cyclic_queue<std::pair<std::binary_semaphore *, std::optional<dg_sock::vector<model::InternalRequest>> *>> waiting_queue,
                                        std::unique_ptr<stdxx::fair_atomic_flag> mtx) noexcept: producer_queue(std::move(producer_queue)),
                                                                                                waiting_queue(std::move(waiting_queue)),
                                                                                                mtx(std::move(mtx)){}


            auto push(dg_sock::vector<model::InternalRequest>&& request) noexcept -> exception_t
            {
                stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(*this->mtx);

                if (!this->waiting_queue.empty())
                {
                    auto [pending_smp, fetching_addr] = std::move(this->waiting_queue.front());
                    this->waiting_queue.pop_front();
                    *fetching_addr  = std::move(request);
                    pending_smp->release();

                    return dg_sock::network_exception::SUCCESS;
                }

                if (this->producer_queue.size() == this->producer_queue.capacity())
                {
                    return dg_sock::network_exception::QUEUE_FULL;
                }

                dg_sock::network_exception_handler::nothrow_log(this->producer_queue.push_back(std::move(request)));

                return dg_sock::network_exception::SUCCESS;
            }

            auto pop() noexcept -> dg_sock::vector<model::InternalRequest>
            {
                auto pending_smp        = std::binary_semaphore(0);
                auto internal_request   = std::optional<dg_sock::vector<model::InternalRequest>>{};

                {
                    stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(*this->mtx);

                    if (!this->producer_queue.empty())
                    {
                        auto rs = std::move(this->producer_queue.front());
                        this->producer_queue.pop_front();

                        return rs;
                    }

                    if constexpr(DEBUG_MODE_FLAG)
                    {
                        if (this->waiting_queue.size() == this->waiting_queue.capacity())
                        {
                            dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                            std::abort();   
                        }
                    }

                    dg_sock::network_exception_handler::nothrow_log(this->waiting_queue.push_back(std::make_pair(&pending_smp, &internal_request)));
                }

                pending_smp.acquire();

                return std::move(internal_request.value());
            }
    };

    //clear
    class RequestContainer: public virtual RequestContainerInterface
    {
        private:

            dg_sock::pow2_cyclic_queue<dg_sock::vector<model::InternalRequest>> producer_queue;
            dg_sock::pow2_cyclic_queue<std::pair<std::binary_semaphore *, std::optional<dg_sock::vector<model::InternalRequest>> *>> waiting_queue;
            dg_sock::pow2_cyclic_queue<std::pair<std::binary_semaphore *, dg_sock::vector<model::InternalRequest>>> push_waiting_queue;
            std::unique_ptr<stdxx::fair_atomic_flag> mtx;

        public:

            RequestContainer(dg_sock::pow2_cyclic_queue<dg_sock::vector<model::InternalRequest>> producer_queue,
                             dg_sock::pow2_cyclic_queue<std::pair<std::binary_semaphore *, std::optional<dg_sock::vector<model::InternalRequest>> *>> waiting_queue,
                             dg_sock::pow2_cyclic_queue<std::pair<std::binary_semaphore *, dg_sock::vector<model::InternalRequest>>> push_waiting_queue,
                             std::unique_ptr<stdxx::fair_atomic_flag> mtx) noexcept: producer_queue(std::move(producer_queue)),
                                                                                     waiting_queue(std::move(waiting_queue)),
                                                                                     push_waiting_queue(std::move(push_waiting_queue)),
                                                                                     mtx(std::move(mtx)){}

            auto push(dg_sock::vector<model::InternalRequest>&& request) noexcept -> exception_t
            {
                std::binary_semaphore wait_smp(0);

                bool need_wait = [&]() noexcept
                {
                    stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(*this->mtx);

                    if (!this->waiting_queue.empty())
                    {
                        auto [pending_smp, fetching_addr] = std::move(this->waiting_queue.front());
                        this->waiting_queue.pop_front();
                        *fetching_addr  = std::move(request);
                        pending_smp->release();

                        return false;
                    }

                    if (this->producer_queue.size() == this->producer_queue.capacity())
                    {
                        if constexpr(DEBUG_MODE_FLAG)
                        {
                            if (this->push_waiting_queue.size() == this->push_waiting_queue.capacity())
                            {
                                dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                                std::abort();
                            }

                            dg_sock::network_exception_handler::nothrow_log(this->push_waiting_queue.push_back({&wait_smp, std::move(request)}));
                        }

                        return true;
                    }

                    dg_sock::network_exception_handler::nothrow_log(this->producer_queue.push_back(std::move(request)));

                    return false;
                }();

                if (need_wait)
                {
                    wait_smp.acquire();
                }

                return dg_sock::network_exception::SUCCESS;
            }

            auto pop() noexcept -> dg_sock::vector<model::InternalRequest>
            {
                auto pending_smp        = std::binary_semaphore(0);
                auto internal_request   = std::optional<dg_sock::vector<model::InternalRequest>>{};

                {
                    stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(*this->mtx);

                    if (!this->push_waiting_queue.empty())
                    {
                        auto [pending_smp, request_data] = std::move(this->push_waiting_queue.front());
                        this->push_waiting_queue.pop_front();
                        pending_smp->release();

                        return request_data;
                    }

                    if (!this->producer_queue.empty())
                    {
                        auto rs = std::move(this->producer_queue.front());
                        this->producer_queue.pop_front();

                        return rs;
                    }

                    if constexpr(DEBUG_MODE_FLAG)
                    {
                        if (this->waiting_queue.size() == this->waiting_queue.capacity())
                        {
                            dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                            std::abort();   
                        }
                    }

                    dg_sock::network_exception_handler::nothrow_log(this->waiting_queue.push_back(std::make_pair(&pending_smp, &internal_request)));
                }

                pending_smp.acquire();

                return std::move(internal_request.value());
            }
    };

    //clear
    class TicketController: public virtual TicketControllerInterface
    {
        private:

            dg_sock::unordered_unstable_map<model::ticket_id_t, std::optional<std::shared_ptr<ResponseObserverInterface>>> ticket_resource_map;
            size_t ticket_resource_map_cap;
            model::ticket_id_t ticket_id_counter;
            std::unique_ptr<stdxx::fair_atomic_flag> mtx;
            stdxx::hdi_container<size_t> max_consume_per_load;

        public:

            TicketController(dg_sock::unordered_unstable_map<model::ticket_id_t, std::optional<std::shared_ptr<ResponseObserverInterface>>> ticket_resource_map,
                             size_t ticket_resource_map_cap,
                             model::ticket_id_t ticket_id_counter,
                             std::unique_ptr<stdxx::fair_atomic_flag> mtx,
                             stdxx::hdi_container<size_t> max_consume_per_load): ticket_resource_map(std::move(ticket_resource_map)),
                                                                                ticket_resource_map_cap(ticket_resource_map_cap),
                                                                                ticket_id_counter(ticket_id_counter),
                                                                                mtx(std::move(mtx)),
                                                                                max_consume_per_load(std::move(max_consume_per_load)){}

            auto open_ticket(size_t sz, model::ticket_id_t * out_ticket_arr) noexcept -> exception_t
            {
                if constexpr(DEBUG_MODE_FLAG)
                {
                    if (sz > this->max_consume_size())
                    {
                        dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(*this->mtx);

                size_t new_sz = this->ticket_resource_map.size() + sz;

                if (new_sz > this->ticket_resource_map_cap)
                {
                    return dg_sock::network_exception::RESOURCE_EXHAUSTION;
                }

                for (size_t i = 0u; i < sz; ++i)
                {
                    static_assert(std::is_unsigned_v<ticket_id_t>); //

                    model::ticket_id_t new_ticket_id    = this->ticket_id_counter++;
                    auto [map_ptr, status]              = this->ticket_resource_map.insert(std::make_pair(new_ticket_id, std::nullopt));
                    dg_sock::network_exception_handler::dg_assert(status);
                    out_ticket_arr[i]                   = new_ticket_id;
                }

                return dg_sock::network_exception::SUCCESS;
            }

            void assign_observer(model::ticket_id_t * ticket_id_arr, size_t sz,
                                 std::move_iterator<std::shared_ptr<ResponseObserverInterface> *> corresponding_observer_arr,
                                 std::expected<bool, exception_t> * exception_arr) noexcept
            {
                stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(*this->mtx);

                auto base_observer_arr = corresponding_observer_arr.base();

                for (size_t i = 0u; i < sz; ++i)
                {
                    model::ticket_id_t current_ticket_id = ticket_id_arr[i];
                    auto map_ptr = this->ticket_resource_map.find(current_ticket_id);

                    if (map_ptr == this->ticket_resource_map.end())
                    {
                        exception_arr[i] = std::unexpected(dg_sock::network_exception::REST_TICKET_NOT_FOUND);
                        continue;
                    }

                    if (map_ptr->second.has_value())
                    {
                        exception_arr[i] = false;
                        continue;
                    }

                    if (base_observer_arr[i] == nullptr)
                    {
                        exception_arr[i] = std::unexpected(dg_sock::network_exception::INVALID_ARGUMENT);
                        continue;
                    }

                    map_ptr->second     = std::move(base_observer_arr[i]);
                    exception_arr[i]    = true;
                }
            }

            void steal_observer(model::ticket_id_t * ticket_id_arr, size_t sz,
                                std::expected<std::shared_ptr<ResponseObserverInterface>, exception_t> * response_arr) noexcept
            {
                stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(*this->mtx);

                for (size_t i = 0u; i < sz; ++i)
                {
                    model::ticket_id_t current_ticket_id = ticket_id_arr[i];
                    auto map_ptr = this->ticket_resource_map.find(current_ticket_id);

                    if (map_ptr == this->ticket_resource_map.end())
                    {
                        response_arr[i] = std::unexpected(dg_sock::network_exception::REST_TICKET_NOT_FOUND);
                        continue;
                    }

                    if (!map_ptr->second.has_value())
                    {
                        response_arr[i] = std::unexpected(dg_sock::network_exception::REST_TICKET_OBSERVER_NOT_FOUND);
                        continue;
                    }

                    response_arr[i] = std::move(map_ptr->second.value());
                    map_ptr->second = std::nullopt;
                }
            }

            void close_ticket(model::ticket_id_t * ticket_id_arr, size_t sz) noexcept
            {
                stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(*this->mtx);

                for (size_t i = 0u; i < sz; ++i)
                {
                    size_t removed_sz = this->ticket_resource_map.erase(ticket_id_arr[i]);

                    if constexpr(DEBUG_MODE_FLAG)
                    {
                        if (removed_sz == 0u)
                        {
                            dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                            std::abort();
                        }
                    }
                }
            }

            auto max_consume_size() noexcept -> size_t
            {
                return this->max_consume_per_load.value;
            }
    };

    //clear
    class DistributedTicketController: public virtual TicketControllerInterface
    {
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

            auto open_ticket(size_t sz, model::ticket_id_t * rs) noexcept -> exception_t
            {
                if constexpr(DEBUG_MODE_FLAG)
                {
                    if (sz > this->max_consume_size())
                    {
                        dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                size_t tentative_discretization_sz  = sz / this->probe_arr_sz;
                size_t discretization_sz            = std::clamp(tentative_discretization_sz, this->minimum_discretization_sz, this->maximum_discretization_sz);
                size_t peeking_base_arr_sz          = sz / discretization_sz + static_cast<size_t>(sz % discretization_sz != 0u); 
                size_t success_sz                   = 0u;

                for (size_t i = 0u; i < peeking_base_arr_sz; ++i)
                {
                    size_t first        = i * discretization_sz;
                    size_t last         = std::min(static_cast<size_t>((i + 1) * discretization_sz), sz);
                    size_t sub_sz       = last - first; 
                    size_t random_clue  = dg_sock::network_randomizer::randomize_int<size_t>(); 
                    size_t base_arr_idx = random_clue & (this->pow2_base_arr_sz - 1u);

                    exception_t err     = this->base_arr[base_arr_idx]->open_ticket(sub_sz, std::next(rs, first));

                    if (dg_sock::network_exception::is_failed(err))
                    {
                        this->close_ticket(rs, success_sz);
                        return err;
                    }

                    for (size_t i = 0u; i < sub_sz; ++i)
                    {
                        rs[first + i] = this->internal_encode_ticket_id(rs[first + i], base_arr_idx);                        
                    }

                    success_sz += sub_sz;
                }

                return dg_sock::network_exception::SUCCESS;
            }

            void assign_observer(model::ticket_id_t * ticket_id_arr, size_t sz,
                                 std::move_iterator<std::shared_ptr<ResponseObserverInterface> *> assigning_observer_arr,
                                 std::expected<bool, exception_t> * exception_arr) noexcept
            {
                auto base_observer_arr              = assigning_observer_arr.base();

                auto feed_resolutor                 = InternalAssignObserverFeedResolutor{};
                feed_resolutor.controller_arr       = this->base_arr.get();

                size_t trimmed_keyvalue_feed_cap    = std::min(this->keyvalue_feed_cap, sz);
                size_t feeder_allocation_cost       = dg_sock::network_producer_consumer::delvrsrv_kv_allocation_cost(&feed_resolutor, trimmed_keyvalue_feed_cap);
                dg_sock::network_stack_allocation::NoExceptRawAllocation<char[]> feeder_mem(feeder_allocation_cost);
                auto feeder                         = dg_sock::network_exception_handler::nothrow_log(dg_sock::network_producer_consumer::delvrsrv_kv_open_preallocated_raiihandle(&feed_resolutor, trimmed_keyvalue_feed_cap, feeder_mem.get()));

                for (size_t i = 0u; i < sz; ++i)
                {
                    ticket_id_t base_ticket_id;
                    size_t partitioned_idx;
                    std::tie(base_ticket_id, partitioned_idx)   = this->internal_decode_ticket_id(ticket_id_arr[i]);

                    if (partitioned_idx >= this->pow2_base_arr_sz)
                    {
                        exception_arr[i] = std::unexpected(dg_sock::network_exception::REST_TICKET_NOT_FOUND);
                        continue;
                    }

                    auto feed_arg           = InternalAssignObserverFeedArgument{};

                    feed_arg.base_ticket_id = base_ticket_id;
                    feed_arg.observer       = std::next(base_observer_arr, i);
                    feed_arg.exception_ptr  = std::next(exception_arr, i);

                    dg_sock::network_producer_consumer::delvrsrv_kv_deliver(feeder.get(), partitioned_idx, std::move(feed_arg));
                }
            }

            void steal_observer(model::ticket_id_t * ticket_id_arr, size_t sz,
                                std::expected<std::shared_ptr<ResponseObserverInterface>, exception_t> * out_observer_arr) noexcept
            {
                auto feed_resolutor                 = InternalStealObserverFeedResolutor{};
                feed_resolutor.controller_arr       = this->base_arr.get();

                size_t trimmed_keyvalue_feed_cap    = std::min(this->keyvalue_feed_cap, sz);
                size_t feeder_allocation_cost       = dg_sock::network_producer_consumer::delvrsrv_kv_allocation_cost(&feed_resolutor, trimmed_keyvalue_feed_cap);
                dg_sock::network_stack_allocation::NoExceptRawAllocation<char[]> feeder_mem(feeder_allocation_cost);
                auto feeder                         = dg_sock::network_exception_handler::nothrow_log(dg_sock::network_producer_consumer::delvrsrv_kv_open_preallocated_raiihandle(&feed_resolutor, trimmed_keyvalue_feed_cap, feeder_mem.get()));

                for (size_t i = 0u; i < sz; ++i)
                {
                    ticket_id_t base_ticket_id;
                    size_t partitioned_idx;
                    std::tie(base_ticket_id, partitioned_idx)   = this->internal_decode_ticket_id(ticket_id_arr[i]);

                    if (partitioned_idx >= this->pow2_base_arr_sz)
                    {
                        out_observer_arr[i] = std::unexpected(dg_sock::network_exception::REST_TICKET_NOT_FOUND);
                        continue;
                    }

                    auto feed_arg               = InternalStealObserverFeedArgument{};
                    feed_arg.base_ticket_id     = base_ticket_id;
                    feed_arg.out_observer_ptr   = std::next(out_observer_arr, i);

                    dg_sock::network_producer_consumer::delvrsrv_kv_deliver(feeder.get(), partitioned_idx, feed_arg);
                }
            }

            void close_ticket(model::ticket_id_t * ticket_id_arr, size_t sz) noexcept
            {
                auto feed_resolutor                 = InternalCloseTicketFeedResolutor{};
                feed_resolutor.controller_arr       = this->base_arr.get();

                size_t trimmed_keyvalue_feed_cap    = std::min(this->keyvalue_feed_cap, sz);
                size_t feeder_allocation_cost       = dg_sock::network_producer_consumer::delvrsrv_kv_allocation_cost(&feed_resolutor, trimmed_keyvalue_feed_cap);
                dg_sock::network_stack_allocation::NoExceptRawAllocation<char[]> feeder_mem(feeder_allocation_cost);
                auto feeder                         = dg_sock::network_exception_handler::nothrow_log(dg_sock::network_producer_consumer::delvrsrv_kv_open_preallocated_raiihandle(&feed_resolutor, trimmed_keyvalue_feed_cap, feeder_mem.get()));

                for (size_t i = 0u; i < sz; ++i)
                {
                    ticket_id_t base_ticket_id;
                    size_t partitioned_idx;
                    std::tie(base_ticket_id, partitioned_idx)   = this->internal_decode_ticket_id(ticket_id_arr[i]);

                    if constexpr(DEBUG_MODE_FLAG)
                    {
                        if (partitioned_idx >= this->pow2_base_arr_sz)
                        {
                            dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION)); //we are very unforgiving about the inverse operation, because it hints a serious corruption has occurred
                            std::abort();
                        }
                    }

                    dg_sock::network_producer_consumer::delvrsrv_kv_deliver(feeder.get(), partitioned_idx, base_ticket_id);
                }
            }

            auto max_consume_size() noexcept -> size_t
            {
                return this->max_consume_per_load;
            }

        private:

            auto internal_encode_ticket_id(ticket_id_t base_ticket_id, size_t base_arr_idx) noexcept -> ticket_id_t
            {
                static_assert(std::is_unsigned_v<ticket_id_t>);

                size_t popcount = std::countr_zero(this->pow2_base_arr_sz);
                return stdxx::safe_unsigned_lshift(base_ticket_id, popcount) | base_arr_idx;
            }

            auto internal_decode_ticket_id(ticket_id_t current_ticket_id) noexcept -> std::pair<ticket_id_t, size_t>
            {
                static_assert(std::is_unsigned_v<ticket_id_t>);

                size_t popcount             = std::countr_zero(this->pow2_base_arr_sz);
                size_t bitmask              = stdxx::lowones_bitgen<size_t>(popcount);
                size_t base_arr_idx         = current_ticket_id & bitmask;
                ticket_id_t base_ticket_id  = current_ticket_id >> popcount;

                return std::make_pair(base_ticket_id, base_arr_idx);
            }

            struct InternalAssignObserverFeedArgument
            {
                ticket_id_t base_ticket_id;
                std::shared_ptr<ResponseObserverInterface> * observer;
                std::expected<bool, exception_t> * exception_ptr;
            };

            struct InternalAssignObserverFeedResolutor: dg_sock::network_producer_consumer::KVConsumerInterface<size_t, InternalAssignObserverFeedArgument>
            {
                std::unique_ptr<TicketControllerInterface> * controller_arr;

                void push(const size_t& partitioned_idx, std::move_iterator<InternalAssignObserverFeedArgument *> data_arr, size_t sz) noexcept
                {
                    InternalAssignObserverFeedArgument * base_data_arr = data_arr.base();

                    dg_sock::network_stack_allocation::NoExceptAllocation<ticket_id_t[]> ticket_id_arr(sz);
                    dg_sock::network_stack_allocation::NoExceptAllocation<std::shared_ptr<ResponseObserverInterface>[]> assigning_observer_arr(sz);
                    dg_sock::network_stack_allocation::NoExceptAllocation<std::expected<bool, exception_t>[]> exception_arr(sz);

                    for (size_t i = 0u; i < sz; ++i)
                    {
                        ticket_id_arr[i]            = base_data_arr[i].base_ticket_id;
                        assigning_observer_arr[i]   = std::move(*base_data_arr[i].observer);
                    }

                    this->controller_arr[partitioned_idx]->assign_observer(ticket_id_arr.get(), sz,
                                                                           std::make_move_iterator(assigning_observer_arr.get()),
                                                                           exception_arr.get());

                    for (size_t i = 0u; i < sz; ++i)
                    {
                        if (!exception_arr[i].has_value())
                        {
                            *base_data_arr[i].observer = std::move(assigning_observer_arr[i]);
                        }

                        *base_data_arr[i].exception_ptr = exception_arr[i];
                    }
                }
            };

            struct InternalStealObserverFeedArgument
            {
                ticket_id_t base_ticket_id;
                std::expected<std::shared_ptr<ResponseObserverInterface>, exception_t> * out_observer_ptr;
            };

            struct InternalStealObserverFeedResolutor: dg_sock::network_producer_consumer::KVConsumerInterface<size_t, InternalStealObserverFeedArgument>
            {
                std::unique_ptr<TicketControllerInterface> * controller_arr;

                void push(const size_t& partitioned_idx, std::move_iterator<InternalStealObserverFeedArgument *> data_arr, size_t sz) noexcept
                {
                    InternalStealObserverFeedArgument * base_data_arr = data_arr.base();

                    dg_sock::network_stack_allocation::NoExceptAllocation<ticket_id_t[]> ticket_id_arr(sz);
                    dg_sock::network_stack_allocation::NoExceptAllocation<std::expected<std::shared_ptr<ResponseObserverInterface>, exception_t>[]> stealing_observer_arr(sz);

                    for (size_t i = 0u; i < sz; ++i)
                    {
                        ticket_id_arr[i] = base_data_arr[i].base_ticket_id;
                    }

                    this->controller_arr[partitioned_idx]->steal_observer(ticket_id_arr.get(), sz, stealing_observer_arr.get());

                    for (size_t i = 0u; i < sz; ++i)
                    {
                        *base_data_arr[i].out_observer_ptr = std::move(stealing_observer_arr[i]);
                    }
                }
            };

            struct InternalCloseTicketFeedResolutor: dg_sock::network_producer_consumer::KVConsumerInterface<size_t, ticket_id_t>
            {
                std::unique_ptr<TicketControllerInterface> * controller_arr;

                void push(const size_t& partitioned_idx, std::move_iterator<ticket_id_t *> data_arr, size_t sz) noexcept
                {
                    this->controller_arr[partitioned_idx]->close_ticket(data_arr.base(), sz);
                }
            };
    };

    //clear
    class IncrementingRequestIDGenerator: public virtual RequestIDGeneratorInterface
    {
        private:

            std::array<char, 8u> ip;
            uint8_t ip_factory_id;
            stdxx::inplace_hdi_container<std::atomic<uint64_t>> id_counter;

        public:

            IncrementingRequestIDGenerator(std::array<char, 8u> ip,
                                           uint8_t ip_factory_id,
                                           uint64_t id_counter) noexcept: ip(ip),
                                                                          ip_factory_id(ip_factory_id),
                                                                          id_counter(std::in_place_t{}, id_counter){}

            auto get(size_t ticket_sz, RequestID * output_request_id_arr) noexcept -> exception_t
            {
                uint64_t start_id   = this->id_counter.value.fetch_add(ticket_sz, std::memory_order_relaxed);
                uint64_t current_id = start_id;

                for (size_t i = 0u; i < ticket_sz; ++i)
                {
                    output_request_id_arr[i] = RequestID{.ip                = this->ip,
                                                         .native_request_id = client_impl1::to_id_storage(current_id++)};
                }

                return dg_sock::network_exception::SUCCESS;
            }
    };

    //clear
    class InBoundWorker: public virtual dg_sock::network_concurrency::WorkerInterface
    {
        private:

            std::shared_ptr<TicketControllerInterface> ticket_controller;
            std::shared_ptr<TicketTimeoutManagerInterface> timeout_manager;
            uint32_t channel;
            size_t ticket_controller_feed_cap;
            size_t recv_consume_sz;
            size_t busy_consume_sz;

        public:

            InBoundWorker(std::shared_ptr<TicketControllerInterface> ticket_controller,
                          std::shared_ptr<TicketTimeoutManagerInterface> timeout_manager,
                          uint32_t channel,
                          size_t ticket_controller_feed_cap,
                          size_t recv_consume_sz,
                          size_t busy_consume_sz) noexcept: ticket_controller(std::move(ticket_controller)),
                                                            timeout_manager(std::move(timeout_manager)),
                                                            channel(channel),
                                                            ticket_controller_feed_cap(ticket_controller_feed_cap),
                                                            recv_consume_sz(recv_consume_sz),
                                                            busy_consume_sz(busy_consume_sz){}

            bool run_one_epoch() noexcept
            {
                size_t buf_arr_cap  = this->recv_consume_sz;
                size_t buf_arr_sz   = {};
                dg_sock::network_stack_allocation::NoExceptAllocation<dg_sock::string[]> buf_arr(buf_arr_cap); 

                dg_sock::network_kernel_mailbox::recv(this->channel, buf_arr.get(), buf_arr_sz, buf_arr_cap);

                auto feed_resolutor                         = InternalFeedResolutor{};
                feed_resolutor.ticket_controller            = this->ticket_controller.get();
                feed_resolutor.timeout_manager              = this->timeout_manager.get();

                size_t trimmed_ticket_controller_feed_cap   = std::min(this->ticket_controller_feed_cap, buf_arr_sz);
                size_t feeder_allocation_cost               = dg_sock::network_producer_consumer::delvrsrv_allocation_cost(&feed_resolutor, trimmed_ticket_controller_feed_cap);
                dg_sock::network_stack_allocation::NoExceptRawAllocation<char[]> feeder_mem(feeder_allocation_cost);
                auto feeder                                 = dg_sock::network_exception_handler::nothrow_log(dg_sock::network_producer_consumer::delvrsrv_open_preallocated_raiihandle(&feed_resolutor, trimmed_ticket_controller_feed_cap, feeder_mem.get())); 

                for (size_t i = 0u; i < buf_arr_sz; ++i)
                {
                    std::expected<model::InternalResponse, exception_t> response = dg_sock::network_exception::to_cstyle_function(dg_sock::network_compact_serializer::dgstd_deserialize<model::InternalResponse, dg_sock::string>)(buf_arr[i], model::INTERNAL_RESPONSE_SERIALIZATION_SECRET);

                    if (!response.has_value())
                    {
                        dg_sock::network_log_stackdump::error_fast_optional(dg_sock::network_exception::verbose(response.error()));
                        continue;
                    }

                    dg_sock::network_producer_consumer::delvrsrv_deliver(feeder.get(), std::move(response.value()));                    
                }

                return buf_arr_sz >= this->busy_consume_sz;
            }

        private:

            struct InternalFeedResolutor: dg_sock::network_producer_consumer::ConsumerInterface<model::InternalResponse>
            {
                TicketControllerInterface * ticket_controller;
                TicketTimeoutManagerInterface * timeout_manager;

                void push(std::move_iterator<model::InternalResponse *> response_arr, size_t sz) noexcept
                {
                    model::InternalResponse * base_response_arr = response_arr.base();

                    dg_sock::network_stack_allocation::NoExceptAllocation<model::ticket_id_t[]> ticket_id_arr(sz);
                    dg_sock::network_stack_allocation::NoExceptAllocation<std::expected<std::shared_ptr<ResponseObserverInterface>, exception_t>[]> observer_arr(sz);

                    for (size_t i = 0u; i < sz; ++i)
                    {
                        ticket_id_arr[i] = base_response_arr[i].ticket_id;
                    }

                    this->ticket_controller->steal_observer(ticket_id_arr.get(), sz, observer_arr.get());

                    for (size_t i = 0u; i < sz; ++i)
                    {
                        if (!observer_arr[i].has_value())
                        {
                            dg_sock::network_log_stackdump::error_fast_optional(dg_sock::network_exception::verbose(dg_sock::network_exception::REST_LOST_RESPONSE));
                            continue;
                        }

                        stdxx::safe_ptr_access(observer_arr[i].value().get())->update(std::move(base_response_arr[i].response)); //declare expectations
                    }

                    this->timeout_manager->void_ticket(ticket_id_arr.get(), sz);
                }
            };
    };

    //clear
    class OutBoundWorker: public virtual dg_sock::network_concurrency::WorkerInterface
    {
        private:

            std::shared_ptr<RequestContainerInterface> request_container;
            size_t mailbox_feed_cap;
            uint32_t send_channel;

        public:

            OutBoundWorker(std::shared_ptr<RequestContainerInterface> request_container,
                           size_t mailbox_feed_cap,
                           uint32_t send_channel) noexcept: request_container(std::move(request_container)),
                                                            mailbox_feed_cap(mailbox_feed_cap),
                                                            send_channel(send_channel){}

            bool run_one_epoch() noexcept
            {
                dg_sock::vector<model::InternalRequest> request_vec = this->request_container->pop();

                auto feed_resolutor             = InternalFeedResolutor{};
                feed_resolutor.send_channel     = this->send_channel;

                size_t trimmed_mailbox_feed_cap = std::min(std::min(this->mailbox_feed_cap, dg_sock::network_kernel_mailbox::max_consume_size()), static_cast<size_t>(request_vec.size()));
                size_t feeder_allocation_cost   = dg_sock::network_producer_consumer::delvrsrv_allocation_cost(&feed_resolutor, trimmed_mailbox_feed_cap);
                dg_sock::network_stack_allocation::NoExceptRawAllocation<char[]> feeder_mem(feeder_allocation_cost);
                auto feeder                     = dg_sock::network_exception_handler::nothrow_log(dg_sock::network_producer_consumer::delvrsrv_open_preallocated_raiihandle(&feed_resolutor, trimmed_mailbox_feed_cap, feeder_mem.get()));

                for (model::InternalRequest& request: request_vec)
                {
                    Address remote_addr = request.request.requestee_url.remote_addr; 
                    std::expected<dg_sock::string, exception_t> bstream = dg_sock::network_exception::to_cstyle_function(dg_sock::network_compact_serializer::dgstd_serialize<dg_sock::string, model::InternalRequest>)(request, model::INTERNAL_REQUEST_SERIALIZATION_SECRET);

                    if (!bstream.has_value())
                    {
                        dg_sock::network_log_stackdump::error_fast_optional(dg_sock::network_exception::verbose(bstream.error()));
                        continue;
                    }

                    auto feed_arg   = InternalMailBoxArgument
                    {
                        .to         = remote_addr,
                        .content    = std::move(bstream.value())
                    };

                    dg_sock::network_producer_consumer::delvrsrv_deliver(feeder.get(), std::move(feed_arg));
                }

                return true;
            }

        private:

            struct InternalMailBoxArgument
            {
                Address to;
                dg_sock::string content;
            };

            struct InternalFeedResolutor: dg_sock::network_producer_consumer::ConsumerInterface<InternalMailBoxArgument>
            {
                uint32_t send_channel;

                void push(std::move_iterator<InternalMailBoxArgument *> mailbox_arg, size_t sz) noexcept
                {
                    dg_sock::network_stack_allocation::NoExceptAllocation<exception_t[]> exception_arr(sz);
                    dg_sock::network_stack_allocation::NoExceptAllocation<Address[]> addr_arr(sz);
                    dg_sock::network_stack_allocation::NoExceptAllocation<dg_sock::string[]> content_arr(sz);

                    auto base_mailbox_arg = mailbox_arg.base(); 

                    for (size_t i = 0u; i < sz; ++i)
                    {
                        addr_arr[i]     = base_mailbox_arg[i].to;
                        content_arr[i]  = std::move(base_mailbox_arg[i].content);
                    }

                    dg_sock::network_kernel_mailbox::send(this->send_channel,
                                                          addr_arr.get(), content_arr.get(), sz,
                                                          exception_arr.get());

                    for (size_t i = 0u; i < sz; ++i)
                    {
                        if (dg_sock::network_exception::is_failed(exception_arr[i]))
                        {
                            dg_sock::network_log_stackdump::error_fast_optional(dg_sock::network_exception::verbose(exception_arr[i]));
                        }
                    }
                }
            };
    };

    //clear
    class ExpiryWorker: public virtual dg_sock::network_concurrency::WorkerInterface
    {
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

            bool run_one_epoch() noexcept
            {
                size_t expired_ticket_arr_cap       = this->timeout_consume_sz;
                size_t expired_ticket_arr_sz        = {};
                dg_sock::network_stack_allocation::NoExceptAllocation<model::ticket_id_t[]> expired_ticket_arr(expired_ticket_arr_cap);
                this->ticket_timeout_manager->get_expired_ticket(expired_ticket_arr.get(), expired_ticket_arr_sz, expired_ticket_arr_cap);

                auto feed_resolutor                 = InternalFeedResolutor{};
                feed_resolutor.ticket_controller    = this->ticket_controller.get();

                size_t trimmed_observer_steal_cap   = std::min(this->ticketcontroller_observer_steal_cap, expired_ticket_arr_sz);
                size_t feeder_allocation_cost       = dg_sock::network_producer_consumer::delvrsrv_allocation_cost(&feed_resolutor, trimmed_observer_steal_cap);
                dg_sock::network_stack_allocation::NoExceptRawAllocation<char[]> feeder_mem(feeder_allocation_cost);
                auto feeder                         = dg_sock::network_exception_handler::nothrow_log(dg_sock::network_producer_consumer::delvrsrv_open_preallocated_raiihandle(&feed_resolutor, trimmed_observer_steal_cap, feeder_mem.get()));

                for (size_t i = 0u; i < expired_ticket_arr_sz; ++i)
                {
                    dg_sock::network_producer_consumer::delvrsrv_deliver(feeder.get(), expired_ticket_arr[i]);
                }

                return expired_ticket_arr_sz >= this->busy_timeout_consume_sz;
            }

        private:

            struct InternalFeedResolutor: dg_sock::network_producer_consumer::ConsumerInterface<model::ticket_id_t>
            {
                TicketControllerInterface * ticket_controller;

                void push(std::move_iterator<model::ticket_id_t *> ticket_id_arr, size_t ticket_id_arr_sz) noexcept
                {
                    model::ticket_id_t * base_ticket_id_arr = ticket_id_arr.base();
                    dg_sock::network_stack_allocation::NoExceptAllocation<std::expected<std::shared_ptr<ResponseObserverInterface>, exception_t>[]> stolen_response_observer_arr(ticket_id_arr_sz);

                    this->ticket_controller->steal_observer(base_ticket_id_arr, ticket_id_arr_sz, stolen_response_observer_arr.get());

                    for (size_t i = 0u; i < ticket_id_arr_sz; ++i)
                    {
                        if (!stolen_response_observer_arr[i].has_value())
                        {
                            continue;
                        }

                        stdxx::safe_ptr_access(stolen_response_observer_arr[i].value().get())->update(std::unexpected(dg_sock::network_exception::REST_CLIENTSIDE_TIMEOUT));
                    }
                }
            };
    };

    //clear
    class RestController: public virtual RestControllerInterface
    {
        private:

            std::shared_ptr<void> daemon;
            std::shared_ptr<RequestContainerInterface> request_container;
            std::shared_ptr<TicketControllerInterface> ticket_controller;
            std::shared_ptr<TicketTimeoutManagerInterface> ticket_timeout_manager;
            std::unique_ptr<RequestIDGeneratorInterface> request_id_generator;
            stdxx::hdi_container<size_t> max_consume_per_load;

        public:

            using self = RestController;

            RestController(std::shared_ptr<void> daemon,
                           std::shared_ptr<RequestContainerInterface> request_container,
                           std::shared_ptr<TicketControllerInterface> ticket_controller,
                           std::shared_ptr<TicketTimeoutManagerInterface> ticket_timeout_manager,
                           std::unique_ptr<RequestIDGeneratorInterface> request_id_generator,
                           stdxx::hdi_container<size_t> max_consume_per_load) noexcept: daemon(std::move(daemon)),
                                                                                        request_container(std::move(request_container)),
                                                                                        ticket_controller(std::move(ticket_controller)),
                                                                                        ticket_timeout_manager(std::move(ticket_timeout_manager)),
                                                                                        request_id_generator(std::move(request_id_generator)),
                                                                                        max_consume_per_load(std::move(max_consume_per_load)){}

            auto request(model::ClientRequest&& client_request) noexcept -> std::expected<dg_sock::unique_ptr<ResponseInterface>, exception_t>
            {
                std::expected<dg_sock::unique_ptr<BatchResponseInterface>, exception_t> resp = this->batch_request(std::make_move_iterator(std::addressof(client_request)), 1u);

                if (!resp.has_value())
                {
                    return std::unexpected(resp.error());
                }

                return self::internal_make_single_response(static_cast<dg_sock::unique_ptr<BatchResponseInterface>&&>(resp.value()));
            }

            auto batch_request(std::move_iterator<model::ClientRequest *> client_request_arr, size_t sz) noexcept -> std::expected<dg_sock::unique_ptr<BatchResponseInterface>, exception_t>
            {
                if (sz > this->max_consume_size())
                {
                    return std::unexpected(dg_sock::network_exception::REST_MAX_CONSUME_SIZE_EXCEEDED);
                }

                if (sz == 0u)
                {
                    return std::unexpected(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                model::ClientRequest * base_client_request_arr  = client_request_arr.base();
                std::chrono::nanoseconds max_timeout_dur        = this->ticket_timeout_manager->max_clockin_dur(); 

                for (size_t i = 0u; i < sz; ++i)
                {
                    if (base_client_request_arr[i].client_timeout_dur > max_timeout_dur)
                    {
                        return std::unexpected(dg_sock::network_exception::REST_INVALID_TIMEOUT);
                    }
                }

                dg_sock::network_stack_allocation::NoExceptAllocation<model::ticket_id_t[]> ticket_id_arr(sz);
                exception_t err = this->ticket_controller->open_ticket(sz, ticket_id_arr.get());

                if (dg_sock::network_exception::is_failed(err))
                {
                    return std::unexpected(err);
                }

                std::expected<dg_sock::unique_ptr<InternalBatchResponse>, exception_t> response = self::internal_make_batch_request_response(sz, ticket_id_arr.get(), this->ticket_controller); //open batch_response associated with the tickets, take ticket responsibility

                if (!response.has_value())
                {
                    this->ticket_controller->close_ticket(ticket_id_arr.get(), sz);

                    return std::unexpected(response.error()); //failed to open response, close the tickets 
                }

                dg_sock::network_stack_allocation::NoExceptAllocation<std::shared_ptr<ResponseObserverInterface>[]> response_observer_arr(sz);
                dg_sock::network_stack_allocation::NoExceptAllocation<std::expected<bool, exception_t>[]> response_observer_exception_arr(sz);

                for (size_t i = 0u; i < sz; ++i)
                {
                    response_observer_arr[i] = dg_sock::network_exception_handler::nothrow_log(response.value()->get_observer(i));
                }

                this->ticket_controller->assign_observer(ticket_id_arr.get(), sz,
                                                         std::make_move_iterator(response_observer_arr.get()),
                                                         response_observer_exception_arr.get()); //bind observers -> ticket_controller to listen for responses

                for (size_t i = 0u; i < sz; ++i)
                {
                    if (!response_observer_exception_arr[i].has_value())
                    {
                        return std::unexpected(response_observer_exception_arr[i].error());
                    }

                    dg_sock::network_exception_handler::dg_assert(response_observer_exception_arr[i].value());
                }

                std::expected<dg_sock::vector<model::InternalRequest>, exception_t> pushing_container = this->internal_make_internal_request(std::make_move_iterator(base_client_request_arr), ticket_id_arr.get(), sz);

                if (!pushing_container.has_value())
                {
                    return std::unexpected(pushing_container.error());
                }

                exception_t push_err = this->request_container->push(static_cast<dg_sock::vector<model::InternalRequest>&&>(pushing_container.value())); //push the outbound request

                if (dg_sock::network_exception::is_failed(push_err))
                {
                    dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(push_err));
                    std::abort();
                }

                dg_sock::network_stack_allocation::NoExceptAllocation<ClockInArgument[]> clockin_arr(sz);
                dg_sock::network_stack_allocation::NoExceptAllocation<exception_t[]> clockin_exception_arr(sz);

                for (size_t i = 0u; i < sz; ++i)
                {
                    clockin_arr[i] = ClockInArgument
                    {
                        .clocked_in_ticket = ticket_id_arr[i], 
                        .expiry_dur        = base_client_request_arr[i].client_timeout_dur
                    };
                }

                this->ticket_timeout_manager->clock_in(clockin_arr.get(), sz, clockin_exception_arr.get()); //clock in the tickets to rescue

                for (size_t i = 0u; i < sz; ++i)
                {
                    if (dg_sock::network_exception::is_failed(clockin_exception_arr[i]))
                    {
                        dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(clockin_exception_arr[i])); //unable to fail, resource leaks + deadlock otherwise, very dangerous, rather terminate
                        std::abort();
                    }
                }

                return dg_sock::unique_ptr<BatchResponseInterface>(std::move(response.value()));
            }

            auto get_designated_request_id(size_t request_id_sz, RequestID * out_request_id_arr) noexcept -> exception_t
            {
                if (request_id_sz > this->max_consume_size())
                {
                    return dg_sock::network_exception::REST_MAX_CONSUME_SIZE_EXCEEDED;
                }

                return this->request_id_generator->get(request_id_sz, out_request_id_arr);
            }

            auto max_consume_size() noexcept -> size_t
            {
                return this->max_consume_per_load.value;
            }

        private:

            class InternalBatchResponse: public virtual BatchResponseInterface
            {
                private:

                    dg_sock::unique_ptr<BatchRequestResponse> base;
                    dg_sock::unique_ptr<model::ticket_id_t[]> ticket_id_arr;
                    size_t ticket_id_arr_sz;
                    std::shared_ptr<TicketControllerInterface> ticket_controller;
                    bool was_released;

                public:

                    //defer construction -> factory, all deferred constructions are marked as noexcept to avoid leaks, such is malloc() -> inplace -> return, fails can only happen at malloc 

                    InternalBatchResponse(dg_sock::unique_ptr<BatchRequestResponse> base,
                                          dg_sock::unique_ptr<model::ticket_id_t[]> ticket_id_arr,
                                          size_t ticket_id_arr_sz,
                                          std::shared_ptr<TicketControllerInterface> ticket_controller,
                                          bool was_released) noexcept: base(std::move(base)),
                                                                       ticket_id_arr(std::move(ticket_id_arr)),
                                                                       ticket_id_arr_sz(ticket_id_arr_sz),
                                                                       ticket_controller(std::move(ticket_controller)),
                                                                       was_released(was_released){}
                    
                    ~InternalBatchResponse() noexcept
                    {
                        this->release_ticket();
                    }

                    auto is_completed() noexcept -> bool
                    {
                        return this->base->is_completed();
                    }

                    auto response() noexcept -> std::expected<dg_sock::vector<std::expected<Response, exception_t>>, exception_t>
                    {
                        auto rs = this->base->response();
                        std::atomic_signal_fence(std::memory_order_seq_cst);
                        this->release_ticket();

                        return rs;
                    }

                    auto response_size() const noexcept -> size_t
                    {
                        return this->base->response_size();
                    }

                    auto get_observer(size_t idx) noexcept -> std::expected<std::shared_ptr<ResponseObserverInterface>, exception_t>
                    {
                        return this->base->get_observer(idx);
                    }

                private:

                    void release_ticket() noexcept
                    {
                        if (std::exchange(this->was_released, true))
                        {
                            return;
                        }

                        this->ticket_controller->close_ticket(this->ticket_id_arr.get(), this->ticket_id_arr_sz);
                    }
            };

            class InternalSingleResponse: public virtual ResponseInterface
            {
                private:

                    dg_sock::unique_ptr<BatchResponseInterface> base;
                
                public:

                    InternalSingleResponse(dg_sock::unique_ptr<BatchResponseInterface> base) noexcept: base(std::move(base)){}

                    auto is_completed() noexcept -> bool
                    {
                        return this->base->is_completed();
                    }

                    auto response() noexcept -> std::expected<Response, exception_t>
                    {
                        auto rs = this->base->response();

                        if (!rs.has_value())
                        {
                            return std::unexpected(rs.error());
                        }

                        if constexpr(DEBUG_MODE_FLAG)
                        {
                            if (rs->size() != 1u)
                            {
                                dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                                std::abort();
                            }
                        }

                        static_assert(std::is_nothrow_move_constructible_v<Response>);
                        return std::expected<Response, exception_t>(std::move(rs->front()));
                    }
            };

            static auto internal_make_batch_request_response(size_t request_sz, ticket_id_t * ticket_id_arr,
                                                             std::shared_ptr<TicketControllerInterface> ticket_controller) noexcept -> std::expected<dg_sock::unique_ptr<InternalBatchResponse>, exception_t>
            {
                if (request_sz == 0u)
                {
                    return std::unexpected(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                if (ticket_controller == nullptr)
                {
                    return std::unexpected(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                std::expected<dg_sock::unique_ptr<BatchRequestResponse>, exception_t> base = dg_sock::network_rest_frame::client_impl1::make_batch_request_response(request_sz);

                if (!base.has_value())
                {
                    return std::unexpected(base.error());
                }

                std::expected<dg_sock::unique_ptr<model::ticket_id_t[]>, exception_t> cpy_ticket_id_arr = dg_sock::network_allocation::make_unique<ticket_id_t[]>(request_sz);

                if (!cpy_ticket_id_arr.has_value())
                {
                    return std::unexpected(cpy_ticket_id_arr.error());
                }

                std::copy(stdxx::safe_ptr_access(ticket_id_arr), std::next(ticket_id_arr, request_sz), cpy_ticket_id_arr.value().get());
                std::expected<dg_sock::unique_ptr<InternalBatchResponse>, exception_t> rs = dg_sock::network_allocation::make_unique<InternalBatchResponse>(std::move(base.value()), 
                                                                                                                                                            std::move(cpy_ticket_id_arr.value()), 
                                                                                                                                                            request_sz, 
                                                                                                                                                            std::move(ticket_controller), 
                                                                                                                                                            false);

                if (!rs.has_value())
                {
                    return std::unexpected(rs.error());
                }

                return rs;
            }

            static auto internal_make_single_response(dg_sock::unique_ptr<BatchResponseInterface>&& base) noexcept -> std::expected<dg_sock::unique_ptr<ResponseInterface>, exception_t>
            {
                return dg_sock::network_allocation::make_unique<InternalSingleResponse>(static_cast<dg_sock::unique_ptr<BatchResponseInterface>&&>(base));
            }

            static auto internal_make_internal_request(std::move_iterator<model::ClientRequest *> request_arr, ticket_id_t * ticket_id_arr, size_t request_arr_sz) noexcept -> std::expected<dg_sock::vector<model::InternalRequest>, exception_t>
            {
                model::ClientRequest * base_request_arr                             = request_arr.base();
                std::expected<dg_sock::vector<model::InternalRequest>, exception_t> rs   = dg_sock::network_exception::cstyle_initialize<dg_sock::vector<model::InternalRequest>>(request_arr_sz);

                if (!rs.has_value())
                {
                    return std::unexpected(rs.error());
                }

                for (size_t i = 0u; i < request_arr_sz; ++i)
                {
                    static_assert(std::is_nothrow_move_assignable_v<model::InternalRequest>);
                    static_assert(std::is_nothrow_move_constructible_v<dg_sock::string>);

                    rs.value()[i] = InternalRequest{.request    = Request{.requestee_url                = std::move(base_request_arr[i].requestee_url),
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
    };

    //clear
    class DistributedRestController: public virtual RestControllerInterface
    {
        private:

            std::unique_ptr<std::unique_ptr<RestControllerInterface>[]> rest_controller_arr;
            size_t pow2_rest_controller_arr_sz;
            stdxx::hdi_container<size_t> max_consume_per_load;

        public:

            DistributedRestController(std::unique_ptr<std::unique_ptr<RestControllerInterface>[]> rest_controller_arr,
                                      size_t pow2_rest_controller_arr_sz,
                                      stdxx::hdi_container<size_t> max_consume_per_load) noexcept: rest_controller_arr(std::move(rest_controller_arr)),
                                                                                                   pow2_rest_controller_arr_sz(pow2_rest_controller_arr_sz),
                                                                                                   max_consume_per_load(std::move(max_consume_per_load)){} 

            auto request(model::ClientRequest&& request) noexcept -> std::expected<dg_sock::unique_ptr<ResponseInterface>, exception_t>
            {
                size_t random_clue  = dg_sock::network_randomizer::randomize_int<size_t>();
                size_t idx          = random_clue & (this->pow2_rest_controller_arr_sz - 1u);

                return this->rest_controller_arr[idx]->request(static_cast<model::ClientRequest&&>(request));
            }

            auto batch_request(std::move_iterator<model::ClientRequest *> request_arr, size_t request_arr_sz) noexcept -> std::expected<dg_sock::unique_ptr<BatchResponseInterface>, exception_t>
            {
                if (request_arr_sz > this->max_consume_size())
                {
                    return std::unexpected(dg_sock::network_exception::REST_MAX_CONSUME_SIZE_EXCEEDED);
                }

                size_t random_clue  = dg_sock::network_randomizer::randomize_int<size_t>();
                size_t idx          = random_clue & (this->pow2_rest_controller_arr_sz - 1u);

                return this->rest_controller_arr[idx]->batch_request(request_arr, request_arr_sz);
            }

            //let's not overcomplicate, such is request_id is just a unique identifier for a Request, and does not hold another special bookkept semantic meaning or special dispatch 
            //we can't really implement a feature that is not going to be used, and actually slow down the code, it's often bad design, this is bad design the fact that we are asking the question

            auto get_designated_request_id(size_t request_id_sz, RequestID * out_request_id_arr) noexcept -> exception_t
            {
                if (request_id_sz > this->max_consume_size())
                {
                    return dg_sock::network_exception::REST_MAX_CONSUME_SIZE_EXCEEDED;
                }

                size_t random_clue  = dg_sock::network_randomizer::randomize_int<size_t>();
                size_t idx          = random_clue & (this->pow2_rest_controller_arr_sz - 1u);

                return this->rest_controller_arr[idx]->get_designated_request_id(request_id_sz, out_request_id_arr);
            }

            auto max_consume_size() noexcept -> size_t
            {
                return this->max_consume_per_load.value;
            }
    };

    class ComponentFactory
    {
        public:

            static auto get_non_blocking_request_container(size_t queue_cap,
                                                           size_t recv_concurrency_queue_sz) -> std::unique_ptr<RequestContainerInterface>
            {
                const size_t MIN_QUEUE_CAP                  = 1u;
                const size_t MAX_QUEUE_CAP                  = size_t{1} << 30;
                const size_t MIN_RECV_CONCURRENCY_QUEUE_SZ  = 1u;
                const size_t MAX_RECV_CONCURRENCY_QUEUE_SZ  = size_t{1} << 30;

                if (std::clamp(queue_cap, MIN_QUEUE_CAP, MAX_QUEUE_CAP) != queue_cap)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                if (!stdxx::is_pow2(queue_cap))
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                if (std::clamp(recv_concurrency_queue_sz, MIN_RECV_CONCURRENCY_QUEUE_SZ, MAX_RECV_CONCURRENCY_QUEUE_SZ) != recv_concurrency_queue_sz)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                if (!stdxx::is_pow2(recv_concurrency_queue_sz))
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                return std::make_unique<NonBlockingRequestContainer>(dg_sock::pow2_cyclic_queue<dg_sock::vector<model::InternalRequest>>(stdxx::ulog2(queue_cap)),
                                                                     dg_sock::pow2_cyclic_queue<std::pair<std::binary_semaphore *, std::optional<dg_sock::vector<model::InternalRequest>> *>>(stdxx::ulog2(recv_concurrency_queue_sz)),
                                                                     stdxx::make_unique_fair_atomic_flag());
            }

            static auto get_request_container(size_t queue_cap,
                                              size_t recv_concurrency_queue_sz,
                                              size_t push_concurrency_queue_sz) -> std::unique_ptr<RequestContainerInterface>
            {
                const size_t MIN_QUEUE_CAP                  = 1u;
                const size_t MAX_QUEUE_CAP                  = size_t{1} << 30;
                const size_t MIN_RECV_CONCURRENCY_QUEUE_SZ  = 1u;
                const size_t MAX_RECV_CONCURRENCY_QUEUE_SZ  = size_t{1} << 30;
                const size_t MIN_PUSH_CONCURRENCY_QUEUE_SZ  = 1u;
                const size_t MAX_PUSH_CONCURRENCY_QUEUE_SZ  = size_t{1} << 30;

                if (std::clamp(queue_cap, MIN_QUEUE_CAP, MAX_QUEUE_CAP) != queue_cap)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                if (!stdxx::is_pow2(queue_cap))
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                if (std::clamp(recv_concurrency_queue_sz, MIN_RECV_CONCURRENCY_QUEUE_SZ, MAX_RECV_CONCURRENCY_QUEUE_SZ) != recv_concurrency_queue_sz)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                if (!stdxx::is_pow2(recv_concurrency_queue_sz))
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                if (std::clamp(push_concurrency_queue_sz, MIN_PUSH_CONCURRENCY_QUEUE_SZ, MAX_PUSH_CONCURRENCY_QUEUE_SZ) != push_concurrency_queue_sz)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                if (!stdxx::is_pow2(push_concurrency_queue_sz))
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                return std::make_unique<RequestContainer>(dg_sock::pow2_cyclic_queue<dg_sock::vector<model::InternalRequest>>(stdxx::ulog2(queue_cap)),
                                                          dg_sock::pow2_cyclic_queue<std::pair<std::binary_semaphore *, std::optional<dg_sock::vector<model::InternalRequest>> *>>(stdxx::ulog2(recv_concurrency_queue_sz)),
                                                          dg_sock::pow2_cyclic_queue<std::pair<std::binary_semaphore *, dg_sock::vector<model::InternalRequest>>>(stdxx::ulog2(push_concurrency_queue_sz)),
                                                          stdxx::make_unique_fair_atomic_flag());

            }

            static auto get_ticket_controller(size_t ticket_cap) -> std::unique_ptr<TicketControllerInterface>
            {
                const size_t MIN_TICKET_CAP = 1u;
                const size_t MAX_TICKET_CAP = size_t{1} << 30;
                const size_t MIN_CONSUME_SZ = 1u;

                if (std::clamp(ticket_cap, MIN_TICKET_CAP, MAX_TICKET_CAP) != ticket_cap)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);                    
                }

                size_t tentative_consume_sz = ticket_cap >> 4;
                size_t actual_consume_sz    = std::max(MIN_CONSUME_SZ, tentative_consume_sz);

                return std::make_unique<TicketController>(dg_sock::unordered_unstable_map<model::ticket_id_t, std::optional<std::shared_ptr<ResponseObserverInterface>>>(),
                                                          ticket_cap,
                                                          0u,
                                                          stdxx::make_unique_fair_atomic_flag(),
                                                          stdxx::hdi_container<size_t>(actual_consume_sz));
            }

            static auto get_distributed_ticket_controller(size_t base_ticket_cap,
                                                          size_t concurrency_sz) -> std::unique_ptr<TicketControllerInterface>
            {
                const size_t MIN_CONCURRENCY_SZ = 1u;
                const size_t MAX_CONCURRENCY_SZ = size_t{1} << 30;

                if (std::clamp(concurrency_sz, MIN_CONCURRENCY_SZ, MAX_CONCURRENCY_SZ) != concurrency_sz)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                if (!stdxx::is_pow2(concurrency_sz))
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                std::unique_ptr<std::unique_ptr<TicketControllerInterface>[]> controller_arr = std::make_unique<std::unique_ptr<TicketControllerInterface>[]>(concurrency_sz);

                for (size_t i = 0u; i < concurrency_sz; ++i)
                {
                    controller_arr[i] = get_ticket_controller(base_ticket_cap);
                }
                
                const size_t IDEAL_PROBE_SZ         = size_t{1} << 3;
                const size_t KEYVALUE_FEED_CAP      = size_t{1} << 6;
                const size_t MIN_DISCRETIZATION_SZ  = 1u;
                const size_t MAX_DISCRETIZATION_SZ  = controller_arr[0]->max_consume_size();

                size_t probe_sz                     = std::min(concurrency_sz, IDEAL_PROBE_SZ);
                size_t consume_sz                   = controller_arr[0]->max_consume_size();

                return std::make_unique<DistributedTicketController>(std::move(controller_arr),
                                                                     concurrency_sz,
                                                                     probe_sz,
                                                                     KEYVALUE_FEED_CAP,
                                                                     MIN_DISCRETIZATION_SZ,
                                                                     MAX_DISCRETIZATION_SZ,
                                                                     consume_sz);
            }

            static auto get_ticket_timeout_manager(size_t push_concurrency_queue_sz,
                                                   size_t pop_concurrency_queue_sz,
                                                   size_t concurrent_request_cap,
                                                   std::chrono::nanoseconds max_wait_dur) -> std::unique_ptr<TicketTimeoutManagerInterface>
            {
                return dg_sock::ticket_system::ComponentFactory::get_ticket_timeout_manager<ticket_id_t>(push_concurrency_queue_sz,
                                                                                                         pop_concurrency_queue_sz,
                                                                                                         concurrent_request_cap,
                                                                                                         max_wait_dur);
            }

            static auto get_inbound_worker(std::shared_ptr<TicketControllerInterface> ticket_controller,
                                           std::shared_ptr<TicketTimeoutManagerInterface> timeout_manager,
                                           uint32_t recv_channel) -> std::unique_ptr<dg_sock::network_concurrency::WorkerInterface>
            {
                if (ticket_controller == nullptr)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                if (timeout_manager == nullptr)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                size_t ticket_controller_feed_cap   = size_t{1} << 6;
                size_t recv_consume_sz              = size_t{1} << 8;
                size_t busy_consume_sz              = 0u;

                return std::make_unique<InBoundWorker>(std::move(ticket_controller),
                                                       std::move(timeout_manager),
                                                       recv_channel,
                                                       ticket_controller_feed_cap,
                                                       recv_consume_sz,
                                                       busy_consume_sz);
            }

            static auto get_random_id_generator() -> std::unique_ptr<RequestIDGeneratorInterface>
            {
                static std::mutex mtx{};

                std::lock_guard<std::mutex> lck_grd(mtx);

                static auto random_generator    = std::bind(std::uniform_int_distribution<uint64_t>(), std::mt19937_64{static_cast<uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())});
                uint64_t id_counter             = random_generator();
                std::array<char, 8u> ip         = {};

                for (size_t i = 0u; i < ip.size(); ++i)
                {
                    ip[i] = std::bit_cast<char>(static_cast<uint8_t>(random_generator()));
                }

                uint8_t ip_factory_id           = 0u;

                return std::make_unique<IncrementingRequestIDGenerator>(ip, ip_factory_id, id_counter);
            }

            static auto get_outbound_worker(std::shared_ptr<RequestContainerInterface> request_container,
                                            uint32_t send_channel) -> std::unique_ptr<dg_sock::network_concurrency::WorkerInterface>
            {
                if (request_container == nullptr)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                size_t mailbox_feed_cap = size_t{1} << 6;

                return std::make_unique<OutBoundWorker>(std::move(request_container),
                                                        mailbox_feed_cap,
                                                        send_channel);
            }

            //we'd attempt to solve the problem of worker pollution on a hollistic scale
            //

            static auto get_expiry_worker(std::shared_ptr<TicketControllerInterface> ticket_controller,
                                          std::shared_ptr<TicketTimeoutManagerInterface> ticket_timeout_manager,
                                          size_t busy_consume_sz = 1u) -> std::unique_ptr<dg_sock::network_concurrency::WorkerInterface>
            {
                if (ticket_controller == nullptr)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                if (ticket_timeout_manager == nullptr)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                size_t timeout_consume_sz   = size_t{1} << 8;
                size_t observer_steal_cap   = size_t{1} << 8;

                return std::make_unique<ExpiryWorker>(std::move(ticket_controller),
                                                      std::move(ticket_timeout_manager),
                                                      timeout_consume_sz,
                                                      observer_steal_cap,
                                                      busy_consume_sz);
            }
    };
}

namespace dg_sock::network_rest_frame::client_instance
{
    using namespace dg_sock::network_rest_frame::model;
    using namespace dg_sock::network_rest_frame::client;

    struct SolutionBuilder
    {
        private:

            uint64_t base_ticket_cap;
            uint64_t ticket_controller_concurrency_sz;
            uint64_t system_thread_count;
            uint64_t concurrent_request_cap;
            std::chrono::nanoseconds max_wait_dur;
            std::optional<uint32_t> recv_channel;
            std::optional<uint32_t> send_channel;
            uint64_t outbound_worker_sz;
            uint64_t inbound_worker_sz;
            uint64_t expiry_worker_sz;
            bool is_wait_request;

            static inline constexpr size_t DEFAULT_BASE_TICKET_CAP                  = size_t{1} << 16;
            static inline constexpr size_t DEFAULT_TICKET_CONTROLLER_CONCURRENCY_SZ = 1u;
            static inline constexpr size_t DEFAULT_CONCURRENT_REQUEST_CAP           = size_t{1} << 10;
            static inline const std::chrono::nanoseconds DEFAULT_MAX_WAIT_DUR       = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::hours(1));
            static inline constexpr size_t DEFAULT_OUTBOUND_WORKER_SZ               = 1u;
            static inline constexpr size_t DEFAULT_INBOUND_WORKER_SZ                = 1u;
            static inline constexpr size_t DEFAULT_EXPIRY_WORKER_SZ                 = 1u;

        public:

            SolutionBuilder(): base_ticket_cap(DEFAULT_BASE_TICKET_CAP),
                               ticket_controller_concurrency_sz(DEFAULT_TICKET_CONTROLLER_CONCURRENCY_SZ),
                               system_thread_count(dg_sock::network_concurrency::get_thread_count()),
                               concurrent_request_cap(DEFAULT_CONCURRENT_REQUEST_CAP),
                               max_wait_dur(DEFAULT_MAX_WAIT_DUR),
                               recv_channel(std::nullopt),
                               send_channel(std::nullopt),
                               outbound_worker_sz(DEFAULT_OUTBOUND_WORKER_SZ),
                               inbound_worker_sz(DEFAULT_INBOUND_WORKER_SZ),
                               expiry_worker_sz(DEFAULT_EXPIRY_WORKER_SZ),
                               is_wait_request(false){}

            auto set_wait_request() -> SolutionBuilder&
            {
                this->is_wait_request = true;

                return *this;
            }

            auto set_no_wait_request() -> SolutionBuilder&
            {
                this->is_wait_request = false;

                return *this;
            }

            auto set_base_ticket_controller_capacity(size_t cap) -> SolutionBuilder&
            {
                if (cap == 0u)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                this->base_ticket_cap = cap;

                return *this;
            }

            auto set_ticket_controller_concurrency_size(size_t sz) -> SolutionBuilder&
            {
                size_t ceil_sz                          = stdxx::ceil2(sz);
                this->ticket_controller_concurrency_sz  = ceil_sz;

                return *this;
            }

            auto set_concurrent_request_cap(size_t cap) -> SolutionBuilder&
            {
                if (cap == 0u)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                this->concurrent_request_cap = cap;

                return *this;
            }

            auto set_max_wait_duration(std::chrono::nanoseconds wait_dur) -> SolutionBuilder&
            {
                if (wait_dur < std::chrono::nanoseconds(0))
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                this->max_wait_dur = wait_dur;

                return *this;
            }

            auto set_recv_channel(uint32_t channel) -> SolutionBuilder&
            {
                this->recv_channel = channel;

                return *this;
            }

            auto set_send_channel(uint32_t channel) -> SolutionBuilder&
            {
                this->send_channel = channel;

                return *this;
            }

            auto set_outbound_worker_size(size_t sz) -> SolutionBuilder&
            {
                if (sz == 0u)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                this->outbound_worker_sz = sz;

                return *this;
            }

            auto set_inbound_worker_size(size_t sz) -> SolutionBuilder&
            {
                if (sz == 0u)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                this->inbound_worker_sz = sz;

                return *this;
            }

            auto set_expiry_worker_size(size_t sz) -> SolutionBuilder&
            {
                if (sz == 0u)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                this->expiry_worker_sz = sz;

                return *this;
            }

            auto get() -> std::unique_ptr<client::RestControllerInterface>
            {
                std::shared_ptr<RequestContainerInterface> request_container        = this->get_request_container();
                std::shared_ptr<TicketControllerInterface> ticket_controller        = this->get_ticket_controller();
                std::shared_ptr<TicketTimeoutManagerInterface> timeout_manager      = this->get_ticket_timeout_manager();
                std::unique_ptr<RequestIDGeneratorInterface> request_id_generator   = this->get_request_id_generator();

                std::shared_ptr<void> inbound_worker                                = this->get_inbound_worker(ticket_controller, timeout_manager);
                std::shared_ptr<void> outbound_worker                               = this->get_outbound_worker(request_container);
                std::shared_ptr<void> expiry_worker                                 = this->get_expiry_worker(ticket_controller, timeout_manager);
                std::shared_ptr<void> master_worker                                 = this->get_master_worker({inbound_worker, outbound_worker, expiry_worker});

                size_t max_consume_per_load                                         = std::min(ticket_controller->max_consume_size(), ticket_controller->max_consume_size());

                return std::make_unique<client_impl1::RestController>(std::move(master_worker),
                                                                      std::move(request_container),
                                                                      std::move(ticket_controller),
                                                                      std::move(timeout_manager),
                                                                      std::move(request_id_generator),
                                                                      stdxx::hdi_container<size_t>(max_consume_per_load));

            }

            template <class Reflector>
            void dg_reflect(const Reflector& reflector) const
            {
                reflector(base_ticket_cap, ticket_controller_concurrency_sz,
                          system_thread_count, concurrent_request_cap,
                          max_wait_dur, recv_channel, send_channel,
                          outbound_worker_sz, inbound_worker_sz,
                          expiry_worker_sz);
            }

            template <class Reflector>
            void dg_reflect(const Reflector& reflector)
            {
                reflector(base_ticket_cap, ticket_controller_concurrency_sz,
                          system_thread_count, concurrent_request_cap,
                          max_wait_dur, recv_channel, send_channel,
                          outbound_worker_sz, inbound_worker_sz,
                          expiry_worker_sz);   
            }

        private:

            auto get_concurrent_timeout_request_cap() -> size_t
            {
                using request_cap_ratio         = std::ratio<3, 4>;

                const size_t MIN_REQUEST_CAP    = 1u;
                const size_t MAX_REQUEST_CAP    = this->concurrent_request_cap;
                size_t tentative_request_cap    = this->concurrent_request_cap / request_cap_ratio::den * request_cap_ratio::num;
                size_t actual_request_cap       = std::clamp(tentative_request_cap, MIN_REQUEST_CAP, MAX_REQUEST_CAP);

                return actual_request_cap;
            }

            auto get_request_container() -> std::unique_ptr<RequestContainerInterface>
            {
                if (this->is_wait_request)
                {
                    return client_impl1::ComponentFactory::get_request_container(stdxx::ceil2(this->concurrent_request_cap),
                                                                                 stdxx::ceil2(this->system_thread_count),
                                                                                 stdxx::ceil2(this->system_thread_count));
                }

                return client_impl1::ComponentFactory::get_non_blocking_request_container(stdxx::ceil2(this->concurrent_request_cap),
                                                                                          stdxx::ceil2(this->system_thread_count));

            }

            auto get_ticket_controller() -> std::unique_ptr<TicketControllerInterface>
            {
                if (this->ticket_controller_concurrency_sz == 0u)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                if (this->ticket_controller_concurrency_sz == 1u)
                {
                    return client_impl1::ComponentFactory::get_ticket_controller(this->base_ticket_cap);
                }

                return client_impl1::ComponentFactory::get_distributed_ticket_controller(this->base_ticket_cap,
                                                                                         this->ticket_controller_concurrency_sz);
            }

            auto get_ticket_timeout_manager() -> std::unique_ptr<TicketTimeoutManagerInterface>
            {
                return client_impl1::ComponentFactory::get_ticket_timeout_manager(stdxx::ceil2(this->system_thread_count),
                                                                                  stdxx::ceil2(this->system_thread_count),
                                                                                  this->get_concurrent_timeout_request_cap(),
                                                                                  this->max_wait_dur);
            }

            auto get_request_id_generator() -> std::unique_ptr<RequestIDGeneratorInterface>
            {
                return client_impl1::ComponentFactory::get_random_id_generator();
            }

            auto get_inbound_worker(std::shared_ptr<TicketControllerInterface> ticket_controller,
                                    std::shared_ptr<TicketTimeoutManagerInterface> timeout_manager) -> std::shared_ptr<void>
            {
                if (ticket_controller == nullptr)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                if (timeout_manager == nullptr)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                if (!this->recv_channel.has_value())
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                std::unique_ptr<dg_sock::network_concurrency::WorkerInterface> worker = client_impl1::ComponentFactory::get_inbound_worker(ticket_controller,
                                                                                                                                           timeout_manager,
                                                                                                                                           this->recv_channel.value());

                auto worker_handle = dg_sock::network_exception_handler::throw_nolog(dg_sock::network_concurrency::daemon_saferegister(dg_sock::network_concurrency::REST_CLIENT_DAEMON, std::move(worker)));

                return std::make_shared<decltype(worker_handle)>(std::move(worker_handle));
            }

            auto get_outbound_worker(std::shared_ptr<RequestContainerInterface> request_container) -> std::shared_ptr<void>
            {
                if (request_container == nullptr)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                if (!this->send_channel.has_value())
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                std::unique_ptr<dg_sock::network_concurrency::WorkerInterface> worker = client_impl1::ComponentFactory::get_outbound_worker(request_container,
                                                                                                                                            this->send_channel.value()); //its best practice to just wait here, we'd be back for optimizations

                auto worker_handle = dg_sock::network_exception_handler::throw_nolog(dg_sock::network_concurrency::daemon_saferegister(dg_sock::network_concurrency::REST_CLIENT_DAEMON, std::move(worker)));

                return std::make_shared<decltype(worker_handle)>(std::move(worker_handle));
            }

            auto get_expiry_worker(std::shared_ptr<TicketControllerInterface> ticket_controller,
                                   std::shared_ptr<TicketTimeoutManagerInterface> ticket_timeout_manager) -> std::shared_ptr<void>
            {
                if (ticket_controller == nullptr)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                if (ticket_timeout_manager == nullptr)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                std::unique_ptr<dg_sock::network_concurrency::WorkerInterface> worker = client_impl1::ComponentFactory::get_expiry_worker(ticket_controller,
                                                                                                                                          ticket_timeout_manager,
                                                                                                                                          0u);

                auto worker_handle = dg_sock::network_exception_handler::throw_nolog(dg_sock::network_concurrency::daemon_saferegister(dg_sock::network_concurrency::REST_CLIENT_DAEMON, std::move(worker)));

                return std::make_shared<decltype(worker_handle)>(std::move(worker_handle));
            }

            auto get_master_worker(std::vector<std::shared_ptr<void>> worker_vec) -> std::shared_ptr<void>
            {
                return std::make_shared<decltype(worker_vec)>(std::move(worker_vec));
            }
    };

    struct Signature{};

    struct ClientInstanceResource
    {
        std::unique_ptr<client::RestControllerInterface> instance;
        Address addr;
    };

    using RestControllerSingleton = stdxx::singleton<Signature, ClientInstanceResource>;

    void init(std::unique_ptr<client::RestControllerInterface> instance,
              const Address& instance_addr)
    {
        RestControllerSingleton::get() =
        {
            .instance   = std::move(instance),
            .addr       = instance_addr
        };

        std::atomic_thread_fence(std::memory_order_seq_cst);
    }

    void deinit() noexcept
    {
        std::atomic_thread_fence(std::memory_order_seq_cst);
        RestControllerSingleton::get() = {};
    }

    auto get_instance() -> const std::unique_ptr<client::RestControllerInterface>&
    {
        return RestControllerSingleton::get().instance;
    }

    auto address() noexcept -> const Address&
    {
        return RestControllerSingleton::get().addr;
    }
}

namespace dg_sock::network_rest_frame::client
{
    using Url               = dg_sock::network_rest_frame::model::Url;
    using ClientRequest     = dg_sock::network_rest_frame::model::ClientRequest;
    using ClientResponse    = dg_sock::network_rest_frame::model::ClientResponse;

    template <class T>
    class Promise
    {
        public:

            virtual ~Promise() noexcept = default;

            virtual auto is_completed() noexcept -> bool = 0;
            virtual auto wait() -> T = 0;
    };

    template <class T>
    class PromiseFactoryInterface
    {
        public:

            virtual ~PromiseFactoryInterface() noexcept = default;

            virtual auto get() -> dg_sock::unique_ptr<Promise<T>> = 0;
    };

    class RetryExceptionRuleInterface
    {
        public:

            virtual ~RetryExceptionRuleInterface() noexcept = default;

            virtual auto can_retry(exception_t err) noexcept -> bool = 0;
    };

    template <class T>
    class RequestRetryMachineInterface
    {
        public:

            virtual ~RequestRetryMachineInterface() noexcept = default;

            virtual auto get_retryable_promise(dg_sock::unique_ptr<PromiseFactoryInterface<T>>&& factory,
                                               dg_sock::unique_ptr<RetryExceptionRuleInterface>&& exception_rule) -> dg_sock::unique_ptr<Promise<T>> = 0;
    };

    class TrueOnAllRetryExceptionRule: public virtual RetryExceptionRuleInterface
    {
        public:

            auto can_retry(exception_t err) noexcept -> bool
            {
                return true;
            }
    };

    template <class T>
    class Base2ExponentialRetryPromise: public virtual Promise<T>
    {
        private:

            struct Bucket
            {
                std::atomic<bool> complete_status;
                std::optional<T> result;
                exception_t err;
            };

            std::shared_ptr<Bucket> bucket;
            bool wait_broke;

        public:

            Base2ExponentialRetryPromise(std::chrono::nanoseconds first_retry_dur,
                                         std::chrono::nanoseconds max_retry_dur,
                                         std::chrono::nanoseconds timeout_dur,
                                         size_t retry_count,
                                         dg_sock::unique_ptr<PromiseFactoryInterface<T>>&& promise_factory,
                                         dg_sock::unique_ptr<RetryExceptionRuleInterface>&& exception_rule)
            {
                const std::chrono::nanoseconds MIN_DUR  = std::chrono::nanoseconds(0);
                const std::chrono::nanoseconds MAX_DUR  = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::hours(1));

                if (std::clamp(first_retry_dur, MIN_DUR, MAX_DUR) != first_retry_dur)
                {
                    throw std::invalid_argument("bad first retry duration, duration out of range");
                }

                if (std::clamp(max_retry_dur, MIN_DUR, MAX_DUR) != max_retry_dur)
                {
                    throw std::invalid_argument("bad max retry duration, duration out of range");
                }

                if (std::clamp(timeout_dur, MIN_DUR, MAX_DUR) != timeout_dur)
                {
                    throw std::invalid_argument("bad timeout duration, duration out of range");
                }

                if (promise_factory == nullptr)
                {
                    throw std::invalid_argument("bad promise factory, null");
                }

                if (exception_rule == nullptr)
                {
                    throw std::invalid_argument("bad exception rule, null");
                }

                this->bucket                = dg_sock::network_allocation::make_shared<Bucket>();
                this->bucket->complete_status.exchange(false, std::memory_order_relaxed);
                this->bucket->result        = std::nullopt;
                this->bucket->err           = dg_sock::network_exception::SUCCESS;
                this->wait_broke            = false;

                coroutine_x::run_detached(dg_sock::network_allocation::make_shared<InternalCoroutine>(this->bucket,
                                                                                                      first_retry_dur,
                                                                                                      max_retry_dur,
                                                                                                      timeout_dur,
                                                                                                      retry_count,
                                                                                                      std::move(promise_factory),
                                                                                                      std::move(exception_rule)),
                                          coroutine_x::NETWORK_COROUTINE);
            }

            auto is_completed() noexcept -> bool
            {
                return this->bucket->complete_status.load(std::memory_order_relaxed);
            }

            auto wait() -> T
            {
                if (std::exchange(this->wait_broke, true))
                {
                    throw std::invalid_argument("second wait");
                }

                this->bucket->complete_status.wait(false, std::memory_order_acquire);
                stdxx::memtransaction_guard grd; 

                if (!this->bucket->result.has_value())
                {
                    dg_sock::network_exception::throw_exception(this->bucket->err);
                }

                return std::move(this->bucket->result.value()); //I guess this is dangerous in the sense of memory ownership, where we'd rely on the shared_ptr<> for deallocation synchronizations
            }

        private:

            class InternalCoroutine: public virtual coroutine_x::CoroutineableInterface
            {
                private:

                    std::shared_ptr<Bucket> bucket;

                    dg_sock::unique_ptr<Promise<T>> current_promise;
                    dg_sock::unique_ptr<PromiseFactoryInterface<T>> promise_factory;
                    dg_sock::unique_ptr<RetryExceptionRuleInterface> exception_rule;

                    std::chrono::nanoseconds first_retry_dur;
                    std::chrono::nanoseconds max_retry_dur;
                    std::chrono::nanoseconds timeout_dur;

                    size_t max_retry_count;
                    size_t retry_idx;

                    std::chrono::time_point<std::chrono::steady_clock> since;
                    bool is_concluded;

                    bool is_on_retry;
                    std::chrono::time_point<std::chrono::steady_clock> on_retry_since;
                    size_t on_retry_idx;                    

                public:

                    InternalCoroutine(std::shared_ptr<Bucket> bucket,
                                      std::chrono::nanoseconds first_retry_dur,
                                      std::chrono::nanoseconds max_retry_dur,
                                      std::chrono::nanoseconds timeout_dur,
                                      size_t max_retry_count,
                                      dg_sock::unique_ptr<PromiseFactoryInterface<T>>&& promise_factory,
                                      dg_sock::unique_ptr<RetryExceptionRuleInterface>&& exception_rule)
                    {
                        this->current_promise   = this->promise_factory->get();

                        this->bucket            = std::move(bucket);
                        this->promise_factory   = std::move(promise_factory);
                        this->exception_rule    = std::move(exception_rule);

                        this->first_retry_dur   = first_retry_dur;
                        this->max_retry_dur     = max_retry_dur;
                        this->timeout_dur       = timeout_dur;
                        
                        this->max_retry_count   = max_retry_count;
                        this->retry_idx         = 0u;

                        this->since             = std::chrono::steady_clock::now();
                        this->is_concluded      = false;

                        this->is_on_retry       = false;
                        this->on_retry_since    = {};
                        this->on_retry_idx      = {};
                    }

                    auto next() noexcept -> bool
                    {
                        if constexpr(DEBUG_MODE_FLAG)
                        {
                            if (this->is_concluded)
                            {
                                dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                                std::abort();
                            }
                        }

                        std::chrono::time_point<std::chrono::steady_clock> now  = std::chrono::steady_clock::now();
                        std::chrono::nanoseconds lapsed                         = now - this->since;

                        if (lapsed >= this->timeout_dur)
                        {
                            this->error_finalize(dg_sock::network_exception::wrap_std_exception(std::current_exception()));
                            return true;
                        }

                        if (this->is_on_retry)
                        {
                            if (lapsed < this->get_retry_sleep_duration())
                            {
                                return false;
                            }

                            try
                            {
                                this->current_promise = this->promise_factory->get();
                            }
                            catch (...)
                            {
                                this->error_finalize(dg_sock::network_exception::wrap_std_exception(std::current_exception()));
                                return true;
                            }

                            this->is_on_retry = false;
                            return true;
                        }

                        if (!this->current_promise->is_completed())
                        {
                            return false;
                        }

                        try
                        {
                            T result = this->current_promise->wait();
                            this->result_finalize(std::move(result));

                            return true;
                        }
                        catch (...)
                        {
                            if (this->retry_idx == this->max_retry_count)
                            {
                                this->error_finalize(dg_sock::network_exception::wrap_std_exception(std::current_exception()));
                                return true;
                            }

                            if (!this->exception_rule->can_retry(dg_sock::network_exception::wrap_std_exception(std::current_exception())))
                            {
                                this->error_finalize(dg_sock::network_exception::wrap_std_exception(std::current_exception()));
                                return true;
                            }

                            if constexpr(DEBUG_MODE_FLAG)
                            {
                                if (this->is_on_retry)
                                {
                                    dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                                    std::abort();
                                }
                            }

                            this->is_on_retry       = true;
                            this->on_retry_since    = now;
                            this->on_retry_idx      = this->retry_idx;
                            this->retry_idx         += 1u;

                            return true;
                        }
                    }

                    auto has_next() noexcept -> bool
                    {
                        if (this->is_concluded)
                        {
                            return false;
                        }

                        return true;
                    }
                
                private:

                    auto get_retry_sleep_duration() -> std::chrono::nanoseconds
                    {
                        return std::min(std::chrono::duration_cast<std::chrono::nanoseconds>(this->first_retry_dur * (size_t{1} << this->on_retry_idx)),
                                        this->max_retry_dur);
                    }

                    void result_finalize(T result) noexcept
                    {
                        if constexpr(DEBUG_MODE_FLAG)
                        {
                            if (this->is_concluded)
                            {
                                dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                                std::abort();
                            }
                        }

                        static_assert(std::is_nothrow_move_assignable_v<T>);
                        
                        this->bucket->result    = std::move(result);

                        if constexpr(STRONG_MEMORY_ORDERING_FLAG)
                        {
                            std::atomic_thread_fence(std::memory_order_seq_cst);
                        }

                        this->bucket->complete_status.exchange(true, std::memory_order_release);
                        this->is_concluded = true;
                    }

                    void error_finalize(exception_t err) noexcept
                    {
                        if constexpr(DEBUG_MODE_FLAG)
                        {
                            if (this->is_concluded)
                            {
                                dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                                std::abort();
                            }
                        }

                        this->bucket->result    = std::nullopt;
                        this->bucket->err       = err;

                        if constexpr(STRONG_MEMORY_ORDERING_FLAG)
                        {
                            std::atomic_thread_fence(std::memory_order_seq_cst);   
                        }

                        this->bucket->complete_status.exchange(true, std::memory_order_release);
                        this->is_concluded = true;
                    }
            };
    };

    template <class T>
    class Base2ExponentialRequestRetryMachine: public virtual RequestRetryMachineInterface<T>
    {
        private:

            std::chrono::nanoseconds first_retry_dur;
            std::chrono::nanoseconds max_retry_dur;
            std::chrono::nanoseconds timeout_dur;
            size_t retry_count;

        public:

            Base2ExponentialRequestRetryMachine(std::chrono::nanoseconds first_retry_dur,
                                                std::chrono::nanoseconds max_retry_dur,
                                                std::chrono::nanoseconds timeout_dur,
                                                size_t retry_count): first_retry_dur(first_retry_dur),
                                                                     max_retry_dur(max_retry_dur),
                                                                     timeout_dur(timeout_dur),
                                                                     retry_count(retry_count){}

            auto get_retryable_promise(dg_sock::unique_ptr<PromiseFactoryInterface<T>>&& factory,
                                       dg_sock::unique_ptr<RetryExceptionRuleInterface>&& exception_rule) -> dg_sock::unique_ptr<Promise<T>>
            {
                return dg_sock::make_unique<Base2ExponentialRetryPromise<T>>(this->first_retry_dur,
                                                                             this->max_retry_dur,
                                                                             this->timeout_dur,
                                                                             this->retry_count,
                                                                             std::move(factory),
                                                                             std::move(exception_rule));
            }
    };

    template <class T = void>
    class RequestRetryMachineFactory
    {
        public:

            using retry_policy_t = uint8_t;

            static inline constexpr retry_policy_t EXPONENTIAL_EASY     = 1u;
            static inline constexpr retry_policy_t EXPONENTIAL_MEDIUM   = 2u;
            static inline constexpr retry_policy_t EXPONENTIAL_HARD     = 3u;

            auto get(retry_policy_t retry_policy) -> dg_sock::unique_ptr<RequestRetryMachineInterface<T>>
            {
                switch (retry_policy)
                {
                    case EXPONENTIAL_EASY:
                    {
                        return this->get_exponential_easy();
                    }
                    case EXPONENTIAL_MEDIUM:
                    {
                        return this->get_exponential_medium();
                    }
                    case EXPONENTIAL_HARD:
                    {
                        return this->get_exponential_hard();
                    }
                    default:
                    {
                        throw std::invalid_argument("bad retry policy, enumeration out of range");
                    }
                }
            }

        private:

            auto get_exponential_easy() -> dg_sock::unique_ptr<RequestRetryMachineInterface<T>>
            {
                const std::chrono::nanoseconds FIRST_RETRY_DUR_ARG  = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(1));
                const std::chrono::nanoseconds MAX_RETRY_DUR_ARG    = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(4));
                const std::chrono::nanoseconds TIMEOUT_DUR_ARG      = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(20));
                const size_t RETRY_COUNT_ARG                        = 3;

                return dg_sock::make_unique<Base2ExponentialRequestRetryMachine<T>>(FIRST_RETRY_DUR_ARG,
                                                                                    MAX_RETRY_DUR_ARG,
                                                                                    TIMEOUT_DUR_ARG,
                                                                                    RETRY_COUNT_ARG);
            }

            auto get_exponential_medium() -> dg_sock::unique_ptr<RequestRetryMachineInterface<T>>
            {
                const std::chrono::nanoseconds FIRST_RETRY_DUR_ARG  = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(1));
                const std::chrono::nanoseconds MAX_RETRY_DUR_ARG    = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(16));
                const std::chrono::nanoseconds TIMEOUT_DUR_ARG      = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::minutes(1));
                const size_t RETRY_COUNT_ARG                        = 10;

                return dg_sock::make_unique<Base2ExponentialRequestRetryMachine<T>>(FIRST_RETRY_DUR_ARG,
                                                                                    MAX_RETRY_DUR_ARG,
                                                                                    TIMEOUT_DUR_ARG,
                                                                                    RETRY_COUNT_ARG);
            }

            auto get_exponential_hard() -> dg_sock::unique_ptr<RequestRetryMachineInterface<T>>
            {
                const std::chrono::nanoseconds FIRST_RETRY_DUR_ARG  = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(1));
                const std::chrono::nanoseconds MAX_RETRY_DUR_ARG    = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::minutes(1));
                const std::chrono::nanoseconds TIMEOUT_DUR_ARG      = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::minutes(10));
                const size_t RETRY_COUNT_ARG                        = 20;

                return dg_sock::make_unique<Base2ExponentialRequestRetryMachine<T>>(FIRST_RETRY_DUR_ARG,
                                                                                    MAX_RETRY_DUR_ARG,
                                                                                    TIMEOUT_DUR_ARG,
                                                                                    RETRY_COUNT_ARG);
            }
    };

    using retry_policy_t = typename RequestRetryMachineFactory<>::retry_policy_t;

    class RequestFactory
    {
        private:

            std::optional<Url> _url;
            std::optional<dg_sock::string> _payload;
            std::optional<dg_sock::string> _serialization_method;

            static inline constexpr std::chrono::nanoseconds DEFAULT_CLIENT_TIMEOUT_DUR = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::minutes(1));

        public:

            RequestFactory(): _url(std::nullopt),
                              _payload(std::nullopt),
                              _serialization_method(std::nullopt){}

            auto url(const Url& url_arg) -> RequestFactory&
            {
                this->_url = url_arg;

                return *this;
            }

            auto payload(std::string_view payload_arg) -> RequestFactory&
            {
                this->_payload = payload_arg;

                return *this;
            }

            auto serialization_method(std::string_view method_arg) -> RequestFactory&
            {
                this->_serialization_method = method_arg;

                return *this;
            }

            auto get() -> ClientRequest
            {
                if (!this->_url.has_value())
                {
                    throw std::invalid_argument("bad url, url not set");
                }

                if (!this->_payload.has_value())
                {
                    throw std::invalid_argument("bad payload, payload not set");
                }

                if (!this->_serialization_method.has_value())
                {
                    throw std::invalid_argument("bad serialization method, serialization method not set");
                }

                return ClientRequest
                {
                    .requestee_url                  = this->_url.value(),
                    .requestor                      = dg_sock::network_rest_frame::client_instance::address(),
                    .payload                        = this->_payload.value(),
                    .payload_serialization_format   = this->_serialization_method.value(),
                    .client_timeout_dur             = DEFAULT_CLIENT_TIMEOUT_DUR,
                    .server_abs_timeout             = std::nullopt,
                    .designated_request_id          = std::nullopt
                };
            }
    };

    template <class T>
    class SelfReflectionTransformer
    {
        public:

            template <class ValueLike>
            constexpr auto operator()(ValueLike&& arg) -> T
            {
                return T(std::forward<ValueLike>(arg));
            }
    };

    template <class Resolutor = SelfReflectionTransformer<Response>>
    class RequestDispatcher
    {
        private:

            template <class U>
            friend class RequestDispatcher;

            Resolutor resolutor;
            std::optional<retry_policy_t> retry_policy;
            std::shared_ptr<ClientRequest> client_request;
            bool is_distinct_request;

            RequestDispatcher(Resolutor resolutor,
                              std::optional<retry_policy_t> retry_policy,
                              std::shared_ptr<ClientRequest> client_request,
                              bool is_distinct_request): resolutor(std::move(resolutor)),
                                                         retry_policy(retry_policy),
                                                         client_request(std::move(client_request)),
                                                         is_distinct_request(is_distinct_request){}

            using self              = RequestDispatcher;

            static inline const std::chrono::nanoseconds DEFAULT_REQUEST_CLIENT_TIMEOUT_DURATION    = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(10));
            static inline const std::chrono::nanoseconds DEFAULT_REQUEST_SERVER_TIMEOUT_DURATION    = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(10));

        public:

            using value_type        = decltype(std::declval<Resolutor&>()(std::declval<const Response&>()));
            using resolutor_type    = Resolutor;

            template <class ResolutorLike = Resolutor, std::enable_if_t<std::is_default_constructible_v<ResolutorLike>, bool> = true>
            RequestDispatcher(): RequestDispatcher(ResolutorLike{},
                                                   std::nullopt,
                                                   nullptr,
                                                   false){}

            RequestDispatcher(Resolutor resolutor): RequestDispatcher(std::move(resolutor),
                                                                      std::nullopt,
                                                                      nullptr,
                                                                      false){}

            auto set_retry_policy(retry_policy_t retry_policy) -> self&
            {
                this->check_and_throw_retry_policy(retry_policy);
                this->retry_policy = retry_policy;

                return *this;
            }

            auto set_request(ClientRequest client_request) -> self&
            {
                this->client_request = dg_sock::network_allocation::make_shared<ClientRequest>(std::move(client_request));

                return *this;
            }

            template <class NewResolutor>
            auto set_resolutor(NewResolutor&& resolutor) -> RequestDispatcher<std::decay_t<NewResolutor>>
            {
                RequestDispatcher<std::decay_t<NewResolutor>> rs(std::forward<NewResolutor>(resolutor),
                                                                 this->retry_policy,
                                                                 this->client_request,
                                                                 this->is_distinct_request);

                return rs;
            }

            auto unique() -> self&
            {
                this->is_distinct_request   = true;

                return *this;
            }

            auto non_unique() -> self&
            {
                this->is_distinct_request   = false;

                return *this;
            }

            auto get_promise() -> dg_sock::unique_ptr<Promise<value_type>>
            {
                if (this->retry_policy.has_value())
                {
                    return RequestRetryMachineFactory<value_type>{}.get(this->retry_policy.value())->get_retryable_promise(this->get_promise_factory(),
                                                                                                                           this->get_exception_rule());
                }

                return this->get_raw_promise();
            }

            auto get() -> value_type
            {
                return this->get_promise()->wait();
            }

        private:

            auto get_full_request_client_timeout_duration() -> std::chrono::nanoseconds
            {
                return DEFAULT_REQUEST_CLIENT_TIMEOUT_DURATION;
            }

            auto get_full_request_server_absolute_timeout_duration() -> std::optional<std::chrono::time_point<std::chrono::utc_clock>>
            {
                return std::chrono::utc_clock::now() + DEFAULT_REQUEST_SERVER_TIMEOUT_DURATION;
            }

            auto get_full_request_designated_request_id() -> std::optional<request_id_t>
            {
                if (!this->is_distinct_request)
                {
                    return std::nullopt;
                }                

                model::RequestID request_id;
                client_instance::get_instance()->get_designated_request_id(1u, &request_id);

                return request_id;
            }

            auto get_full_request() -> ClientRequest
            {
                if (this->client_request == nullptr)
                {
                    throw std::invalid_argument("bad client request, null");
                }

                return ClientRequest
                {
                    .requestee_url                  = this->client_request->requestee_url,
                    .requestor                      = this->client_request->requestor,
                    .payload                        = this->client_request->payload,
                    .payload_serialization_format   = this->client_request->payload_serialization_format,
                    .client_timeout_dur             = this->get_full_request_client_timeout_duration(),
                    .server_abs_timeout             = this->get_full_request_server_absolute_timeout_duration(),
                    .designated_request_id          = this->get_full_request_designated_request_id()
                };
            }

            auto get_promise_factory() -> dg_sock::unique_ptr<PromiseFactoryInterface<value_type>>
            {
                return dg_sock::make_unique<InternalPromiseFactory>(this->get_full_request(),
                                                                    this->resolutor);
            }

            auto get_raw_promise() -> dg_sock::unique_ptr<Promise<value_type>>
            {
                return this->get_promise_factory()->get();
            }

            auto get_exception_rule() -> dg_sock::unique_ptr<RetryExceptionRuleInterface>
            {
                return dg_sock::make_unique<TrueOnAllRetryExceptionRule>();
            }

            void check_and_throw_retry_policy(retry_policy_t retry_policy)
            {
                RequestRetryMachineFactory<value_type>{}.get(retry_policy);
            }

            class PromiseWrapper: public virtual Promise<value_type>
            {
                private:

                    dg_sock::unique_ptr<dg_sock::network_rest_frame::client::ResponseInterface> base;
                    Resolutor resolutor;

                public:

                    PromiseWrapper(dg_sock::unique_ptr<dg_sock::network_rest_frame::client::ResponseInterface> base,
                                   Resolutor resolutor): base(std::move(base)),
                                                         resolutor(std::move(resolutor)){}

                    auto is_completed() noexcept -> bool
                    {
                        return this->base->is_completed();
                    }

                    auto wait() -> value_type
                    {
                        std::expected<ClientResponse, exception_t> response = this->base->response();

                        if (!response.has_value())
                        {
                            dg_sock::network_exception::throw_exception(response.error());
                        }

                        return this->resolutor(std::move(response.value()));
                    }
            };

            class InternalPromiseFactory: public virtual PromiseFactoryInterface<value_type>
            {
                private:

                    ClientRequest request;
                    Resolutor resolutor;

                public:

                    InternalPromiseFactory(ClientRequest request,
                                           Resolutor resolutor): request(std::move(request)),
                                                                 resolutor(std::move(resolutor)){}

                    auto get() -> dg_sock::unique_ptr<Promise<value_type>>
                    {
                        ClientRequest cpy = this->request;
                        std::expected<dg_sock::unique_ptr<ResponseInterface>, exception_t> rs = client_instance::get_instance()->request(std::move(cpy));

                        if (!rs.has_value())
                        {
                            dg_sock::network_exception::throw_exception(rs.error());
                        }

                        return dg_sock::make_unique<PromiseWrapper>(std::move(rs.value()), this->resolutor);
                    }
            };
    };

    class RequestClient
    {
        private:

            std::optional<retry_policy_t> retry_policy;
            bool is_unique_request;

            using self = RequestClient;

        public:

            RequestClient(): retry_policy(std::nullopt),
                             is_unique_request(false){}

            auto set_retry_policy(retry_policy_t retry_policy) -> self&
            {
                this->check_and_throw_retry_policy(retry_policy);
                this->retry_policy = retry_policy;

                return *this;
            }

            auto set_multiple_request_uniqueness(bool is_unique) -> self&
            {
                this->is_unique_request = is_unique;

                return *this;
            }

            auto request(ClientRequest client_request) -> RequestDispatcher<>
            {
                auto rs = RequestDispatcher<>{};

                if (this->retry_policy.has_value())
                {
                    rs.set_retry_policy(this->retry_policy.value());
                }

                rs.set_request(std::move(client_request));

                if (this->is_unique_request)
                {
                    rs.unique();
                }
                else
                {
                    rs.non_unique();
                }

                return rs;
            }

        private:

            void check_and_throw_retry_policy(retry_policy_t retry_policy)
            {
                RequestRetryMachineFactory<int>{}.get(retry_policy);
            }
    };
}

#endif