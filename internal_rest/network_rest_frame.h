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
 
namespace dg_sock::network_rest_frame::model
{
    using ticket_id_t   = uint64_t; //I've thought long and hard, it's better to do bitshift, because the otherwise would be breaking single responsibilities, breach of extensions
    using clock_id_t    = uint64_t; 

    static inline constexpr uint32_t INTERNAL_REQUEST_SERIALIZATION_SECRET  = 3312354321ULL;
    static inline constexpr uint32_t INTERNAL_RESPONSE_SERIALIZATION_SECRET = 3554488158ULL;
    static inline constexpr std::string_view REST_FRAME_VERSION_SUFFX       = std::string_view("REST_FRAME_V1"); //this is to actually solve the serialization problem, we can weed out the version problems, for the bad packets, we are guaranteed to filter that using the hashed value 

    using ipv6_storage_t        = std::array<char, 8u>;
    using ipv4_storage_t        = std::array<char, 4u>;
    using ip_storage_t          = std::variant<ipv4_storage_t, ipv6_storage_t>; 
    using native_id_storage_t   = std::array<char, 8u>; 
    using MailBoxArgument       = dg_sock::network_kernel_mailbox::MailBoxArgument;
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

    struct ClockInArgument
    {
        ticket_id_t clocked_in_ticket;
        std::chrono::nanoseconds expiry_dur;
    };
}

namespace dg_sock::network_rest_frame::server
{
    using namespace dg_sock::network_rest_frame::model;

    //it just seems to me that every request (or flash_stream_x) should be <= 64KB

    //and the pipe capacity should be shared and fixed, leaving a small room to do pings, healthchecks or not at all in the virtual network of packet routing
    //because when I tried to solve the problem of equality or fairness in the virtual transmission network (the network that have fixed bidirectional edges and fixed outdegree and weights), I can't seem to find a better solutions in terms of no-connections and fair
    //I have been able to prove that with a reasonable pipe velocity on client side, the packets can indeed be transmitted with 100% success rate, with a fail rate of around 10 ** -40 if enough retransmissions are done

    //alright, so we have solved the problem of fairness of transmission and the problem of max consumption, hint: it's maxflow + betweenness centrality + really capping the retranmission queue -> 1-10 KB / client + 10 outdegree ...

    //due to the speed difference of local RAM and socket, we now face the problem of capping the "awaiting buffers" and the worst recovery time of a certain transmission node in the middle of a transmission, or a temporary site
    //if we increase the worst recovery time, we'd increase some of the traffic, which we'd want to reduce by increasing the processing capacity + network bandwidth from A -> B

    //these are the not-quantifiables, because we'd expect a system to be working all the time, just like RAM or CPU, since the logics to solve this is way more complicated than you would ever think

    //the other way would be to have multiple virtual networks of transferring from A -> B, and we'd have to "switch" from this network to another network to hit that recovery time equilibrium
    //that is an advanced topic for a fiber network company to figure out, we usually scale nodes transfer 1TB -> 100TB for each of the node, but the methodology remains
    //there is unlikely that recovery or temporary site is necessary, because it's a maxflow problem
    
    //but in our specific application, we only need to do min(process_speed, socket_speed) and transmit accordingly for ease of usage
    //and we do synchronizations, so ... recovery is rarely a problem if we hold unique usage over the socket

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

        virtual auto get_resolver(std::string_view resource_addr) noexcept -> RequestHandlerInterface * = 0;
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

    struct TicketTimeoutManagerInterface
    {
        virtual ~TicketTimeoutManagerInterface() noexcept = default;

        virtual void clock_in(ClockInArgument * clockin_arr, size_t sz, exception_t * exception_arr) noexcept = 0;
        virtual void get_expired_ticket(model::ticket_id_t * output_arr, size_t& output_arr_sz, size_t output_arr_cap) noexcept = 0;
        virtual void void_ticket(model::ticket_id_t * ticket_arr, size_t sz) noexcept = 0;
        virtual auto max_clockin_dur() const noexcept -> std::chrono::nanoseconds = 0;
        virtual auto max_consume_size() noexcept -> size_t = 0;
    };

    struct RestControllerInterface
    {
        virtual ~RestControllerInterface() noexcept = default;

        virtual auto request(model::ClientRequest&&) noexcept -> std::expected<std::unique_ptr<ResponseInterface>, exception_t> = 0;
        virtual auto batch_request(std::move_iterator<model::ClientRequest *> client_request_arr, size_t client_request_arr_sz) noexcept -> std::expected<std::unique_ptr<BatchResponseInterface>, exception_t> = 0;
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

                if (incoming_sz > this->thru_capacity()){
                    return std::unexpected(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                size_t random_clue  = dg_sock::network_randomizer::randomize_int<size_t>();
                size_t base_arr_idx = random_clue & (this->pow2_base_arr_sz - 1u);

                return this->base_arr[base_arr_idx]->thru(incoming_sz);
            }

            void reset() noexcept
            {
                for (size_t i = 0u; i < this->pow2_base_arr_sz; ++i){
                    this->base_arr[i]->reset();
                }
            }

            auto thru_capacity() const noexcept -> size_t
            {
                return this->max_thru_sz;
            }
    };

    //clear
    class SubscriptibleWrappedTrafficController: public virtual UpdatableInterface
    {
        private:

            stdxx::hdi_container<std::chrono::nanoseconds> update_dur;
            stdxx::inplace_hdi_container<std::atomic<std::chrono::time_point<std::chrono::steady_clock>>> last_updated;
            std::shared_ptr<CacheUniqueWriteTrafficControllerInterface> updating_component;

        public:

            SubscriptibleWrappedTrafficController(std::chrono::nanoseconds update_dur,
                                                  std::chrono::time_point<std::chrono::steady_clock> last_updated,
                                                  std::shared_ptr<CacheUniqueWriteTrafficControllerInterface> updating_component) noexcept: update_dur(stdxx::hdi_container<std::chrono::nanoseconds>{update_dur}),
                                                                                                                                            last_updated(std::in_place_t{}, last_updated),
                                                                                                                                            updating_component(std::move(updating_component)){}

            void update() noexcept
            {
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
    class UpdateWorker: public virtual dg_sock::network_concurrency::WorkerInterface
    {
        private:

            std::shared_ptr<UpdatableInterface> updatable;
            std::chrono::nanoseconds heartbeat_dur;

        public:

            UpdateWorker(std::shared_ptr<UpdatableInterface> updatable,
                         std::chrono::nanoseconds heartbeat_dur) noexcept: updatable(std::move(updatable)),
                                                                           heartbeat_dur(heartbeat_dur){}

            auto run_one_epoch() noexcept -> bool
            {
                this->updatable->update();
                std::this_thread::sleep_for(this->heartbeat_dur);

                return true;
            }
    };

    class RequestHandlerDictionary: public virtual RequestHandlerDictionaryInterface
    {
        private:

            dg_sock::unordered_unstable_map<dg_sock::string, std::shared_ptr<RequestHandlerInterface>> request_map;
            std::unique_ptr<stdxx::fair_atomic_flag> mtx;

        public:

            RequestHandlerDictionary(dg_sock::unordered_unstable_map<dg_sock::string, std::shared_ptr<RequestHandlerInterface>> request_map,
                                     std::unique_ptr<stdxx::fair_atomic_flag> mtx) noexcept: request_map(std::move(request_map)),
                                                                                            mtx(std::move(mtx)){}

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

    class OneFetchRequestHandlerRetriever: public virtual RequestHandlerRetrieverInterface
    {
        private:

            std::shared_ptr<RequestHandlerDictionaryInterface> global_pool;
            dg_sock::unordered_unstable_map<dg_sock::string, std::shared_ptr<RequestHandlerInterface>> request_map;

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

        public:

            OneFetchRequestHandlerRetriever(std::shared_ptr<RequestHandlerDictionaryInterface> global_pool,
                                            dg_sock::unordered_unstable_map<dg_sock::string, std::shared_ptr<RequestHandlerInterface>> request_map) noexcept: global_pool(std::move(global_pool)),
                                                                                                                                                    request_map(std::move(request_map)){}

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
    };

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
                    dg_sock::network_stack_allocation::NoExceptAllocation<MailBoxArgument[]> mailbox_arr(sz);

                    for (size_t i = 0u; i < sz; ++i)
                    {
                        mailbox_arr[i].to           = base_data_arr[i].mailbox_arg.to;
                        mailbox_arr[i].content      = static_cast<const void *>(base_data_arr[i].mailbox_arg.content.data());
                        mailbox_arr[i].content_sz   = base_data_arr[i].mailbox_arg.content.size();
                    }

                    dg_sock::network_kernel_mailbox::send(this->send_channel, mailbox_arr.get(), sz, exception_arr.get());

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

                    RequestHandlerInterface * resource_handler = this->request_handler_map->get_resolver(local_uri_path);

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

                    resource_handler->handle(std::make_move_iterator(request_arr.get()), sz, response_arr.get());

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
                return std::make_unique<RequestHandlerDictionary>(dg_sock::unordered_unstable_map<dg_sock::string, std::shared_ptr<RequestHandlerInterface>>{},
                                                                  stdxx::make_unique_fair_atomic_flag());
            }

            static auto get_request_handler_one_fetch_retriever(std::shared_ptr<RequestHandlerDictionaryInterface> arg) -> std::unique_ptr<RequestHandlerRetrieverInterface>
            {
                if (arg == nullptr)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                return std::make_unique<OneFetchRequestHandlerRetriever>(std::move(arg),
                                                                         dg_sock::unordered_unstable_map<dg_sock::string, std::shared_ptr<RequestHandlerInterface>>{});
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

            static auto get_traffic_controller_wrapper(std::shared_ptr<CacheUniqueWriteTrafficControllerInterface> updatable,
                                                       std::chrono::nanoseconds update_dur) -> std::unique_ptr<UpdatableInterface>
            {
                const std::chrono::nanoseconds MIN_UPDATE_DUR   = std::chrono::nanoseconds(0);
                const std::chrono::nanoseconds MAX_UPDATE_DUR   = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::minutes(1));

                if (updatable == nullptr)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                if (std::clamp(update_dur, MIN_UPDATE_DUR, MAX_UPDATE_DUR) != update_dur)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                return std::make_unique<SubscriptibleWrappedTrafficController>(update_dur,
                                                                               std::chrono::steady_clock::now(),
                                                                               std::move(updatable));
            } 

            static auto get_one_request_adapter(std::shared_ptr<OneRequestHandlerInterface> request_handler) -> std::unique_ptr<RequestHandlerInterface>
            {
                if (request_handler == nullptr)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                return std::make_unique<OneRequestHandlerAdapter>(std::move(request_handler));
            }

            static auto get_update_worker(std::shared_ptr<UpdatableInterface> updatable,
                                          std::chrono::nanoseconds update_dur) -> std::unique_ptr<dg_sock::network_concurrency::WorkerInterface>
            {
                const std::chrono::nanoseconds MIN_UPDATE_DUR   = std::chrono::nanoseconds(0);
                const std::chrono::nanoseconds MAX_UPDATE_DUR   = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::minutes(1));

                if (updatable == nullptr)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                if (std::clamp(update_dur, MIN_UPDATE_DUR, MAX_UPDATE_DUR) != update_dur)
                {
                    dg_sock::network_exception::throw_exception(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                return std::make_unique<UpdateWorker>(std::move(updatable),
                                                      update_dur);
            }

            static auto get_request_resolver(std::shared_ptr<RequestHandlerRetrieverInterface> request_handler_map,
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

            static inline constexpr uint32_t REST_SERVER_RECV_CHANNEL   = 1134950404UL;
            static inline constexpr uint32_t REST_SERVER_SEND_CHANNEL   = 1000431304UL;

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

                this->cache_unique_write_traffic_controller_elemental_thru_cap  = config.cache_unique_write_traffic_controller_elemental_thru_cap;
                this->cache_unique_write_traffic_controller_concurrency_sz      = config.cache_unique_write_traffic_controller_concurrency_sz;
                this->cache_unique_write_traffic_controller_reset_duration      = config.cache_unique_write_traffic_controller_reset_duration;

                this->request_resolver_worker_sz                                = config.request_resolver_worker_sz;

                return *this;
            }

            auto build() -> RestServerSolution
            {
                std::shared_ptr<RequestHandlerDictionaryInterface> handler_dict = this->get_request_handler_dictionary();
                std::shared_ptr<void> daemon_process                            = this->get_daemon_process(handler_dict);

                return RestServerSolution
                {
                    .rest_resolver_dictionary   = handler_dict,
                    .daemon_process             = daemon_process
                };
            }

        private:

            auto run_workable(std::unique_ptr<dg_sock::network_concurrency::WorkerInterface> workable) -> std::shared_ptr<void>
            {
                auto resource = dg_sock::network_exception_handler::throw_nolog(dg_sock::network_concurrency::daemon_saferegister(dg_sock::network_concurrency::COMPUTING_DAEMON, std::move(workable))); 

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
                return dg_sock::network_rest_frame::server_impl1::ComponentFactory::get_distributed_cache_write_traffic_controller(this->cache_unique_write_traffic_controller_elemental_thru_cap,
                                                                                                                              this->cache_unique_write_traffic_controller_concurrency_sz);
            }

            auto get_traffic_update_worker(const std::shared_ptr<CacheUniqueWriteTrafficControllerInterface>& traffic_controller) -> std::unique_ptr<dg_sock::network_concurrency::WorkerInterface>
            {
                using namespace dg_sock::network_rest_frame::server_impl1;

                return ComponentFactory::get_update_worker(ComponentFactory::get_traffic_controller_wrapper(traffic_controller, this->cache_unique_write_traffic_controller_reset_duration),
                                                           this->cache_unique_write_traffic_controller_reset_duration);
            }

            auto get_request_resolver_worker(const std::shared_ptr<RequestHandlerDictionaryInterface>& dictionary,
                                             const std::shared_ptr<InfiniteCacheControllerInterface>& cache_controller,
                                             const std::shared_ptr<InfiniteCacheUniqueWriteControllerInterface>& cache_unique_write_controller,
                                             const std::shared_ptr<CacheUniqueWriteTrafficControllerInterface>& traffic_controller) -> std::unique_ptr<dg_sock::network_concurrency::WorkerInterface>
            {
                using namespace dg_sock::network_rest_frame::server_impl1;

                return ComponentFactory::get_request_resolver(ComponentFactory::get_request_handler_one_fetch_retriever(dictionary),
                                                              cache_controller,
                                                              cache_unique_write_controller,
                                                              traffic_controller,
                                                              REST_SERVER_RECV_CHANNEL,
                                                              REST_SERVER_SEND_CHANNEL,
                                                              this->request_resolver_consume_sz,
                                                              this->request_resolver_mailbox_feed_cap,
                                                              this->request_resolver_mailbox_prep_feed_cap,
                                                              this->request_resolver_cache_controller_feed_cap,
                                                              this->request_resolver_server_fetch_feed_cap,
                                                              this->request_resolver_cache_server_fetch_feed_cap,
                                                              this->request_resolver_cache_fetch_feed_cap,
                                                              this->request_resolver_busy_consume_sz);
            }

            auto get_daemon_process(std::shared_ptr<RequestHandlerDictionaryInterface> dictionary) -> std::shared_ptr<void>
            {
                std::vector<std::unique_ptr<dg_sock::network_concurrency::WorkerInterface>> worker_vec           = {};

                std::shared_ptr<InfiniteCacheControllerInterface> cache_controller                          = this->get_cache_controller();
                std::shared_ptr<InfiniteCacheUniqueWriteControllerInterface> cache_unique_write_controller  = this->get_cache_unique_write_controller();
                std::shared_ptr<CacheUniqueWriteTrafficControllerInterface> traffic_controller              = this->get_traffic_controller();

                worker_vec.push_back(this->get_traffic_update_worker(traffic_controller));

                for (size_t i = 0u; i < this->request_resolver_worker_sz; ++i)
                {
                    worker_vec.push_back(this->get_request_resolver_worker(dictionary, cache_controller, cache_unique_write_controller, traffic_controller));
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
        SolutionSingleton::get()    = {};
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

    //what took me 1 year to realize is that the cyclic unordered map or advanced cache map are very useful, and it should be the sole metric to drop resources
    //we don't expect packets to be dropped nor unfair implementations from end-to-end
    //what we'd want is unique reference of the finite transportation stack, and it has to be thru, there is no room for recovery or anything like that
    //though we have anticipated for every scenerio of how this could work out
    //the only worst case scenerio is server processing a request twice, which we'd apply counter measurements by implementing the traffic controller, and the timeout

    //we have reached 100% transmission rate for the kernel_mailbox_impl1_x on finite fixed pipe for multiple concurrent users
    //this is one step more to compromise what could go wrong, and this is very important that this should not go wrong

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

                return dg_sock::vector<std::expected<Response, exception_t>>(std::move(this->resp_vec));
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

            BatchRequestResponse(size_t resp_sz): observer_arr(resp_sz),
                                                  base(std::make_shared<BatchRequestResponseBase>(resp_sz))
            {
                for (size_t i = 0u; i < resp_sz; ++i)
                {
                    this->observer_arr[i] = std::make_shared<BatchRequestResponseBaseDesignatedObserver>(this->base, i);
                }
            }

            friend auto make_batch_request_response(size_t resp_sz) noexcept -> std::expected<std::unique_ptr<BatchRequestResponse>, exception_t>; 

        public:

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
    auto make_batch_request_response(size_t resp_sz) noexcept -> std::expected<std::unique_ptr<BatchRequestResponse>, exception_t>
    {
        return std::unique_ptr<BatchRequestResponse>(new BatchRequestResponse(resp_sz));
    }

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

    template <class T, class StatelessIdExtractor, class ClockType = std::chrono::steady_clock>
    class temporal_ordered_item_map
    {
        public:

            using value_type    = T;
            using id_type       = decltype(std::declval<StatelessIdExtractor&>()(std::declval<const T&>()));
            using clock_type    = ClockType; 

        private:

            struct HeapNode
            {
                T item;
                std::chrono::time_point<ClockType> sched_time;
                size_t heap_idx;
            };

            dg_sock::unordered_unstable_map<id_type, HeapNode *> id_heap_map;
            dg_sock::vector<std::unique_ptr<HeapNode>> temporal_heap;
            size_t temporal_heap_sz;
            
        public:

            temporal_ordered_item_map(size_t cap): id_heap_map(),
                                                   temporal_heap(),
                                                   temporal_heap_sz(0u)
            {
                this->id_heap_map.reserve(cap);

                for (size_t i = 0u; i < cap; ++i)
                {
                    this->temporal_heap.push_back(std::make_unique<HeapNode>(HeapNode{}));
                }
            }

            template <class TypeLike>
            auto add(TypeLike&& item,
                     std::chrono::time_point<ClockType> expiry_time) noexcept -> exception_t
            {   
                if (this->id_heap_map.contains(this->get_id(item)))
                {
                    return dg_sock::network_exception::DUPLICATE_ENTRY;
                }

                std::expected<HeapNode *, exception_t> reference_node = this->add_heap_node(std::forward<TypeLike>(item), expiry_time);

                if (!reference_node.has_value())
                {
                    return reference_node.error();
                }

                try
                {
                    auto [map_ptr, status] = id_heap_map.insert(std::make_pair(this->get_id(reference_node.value()->item), reference_node.value()));
                    dg_sock::network_exception_handler::dg_assert(status);
                }
                catch (...)
                {
                    dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                    std::abort();
                }

                return dg_sock::network_exception::SUCCESS;
            }

            void erase(id_type item_id) noexcept
            {
                auto map_ptr = this->id_heap_map.find(item_id);

                if (map_ptr == this->id_heap_map.end())
                {
                    return;
                }

                size_t idx = stdxx::safe_ptr_access(map_ptr->second)->heap_idx;
                this->id_heap_map.erase(map_ptr);
                this->erase_heap_node_at(idx);
            }

            auto get_expired_item(std::chrono::time_point<ClockType> time_bar) noexcept -> std::optional<T>
            {
                if (this->temporal_heap_sz == 0u)
                {
                    return std::nullopt;
                }

                std::unique_ptr<HeapNode>& front_value = this->temporal_heap.front();

                if (front_value->sched_time >= time_bar)
                {
                    return std::nullopt;
                }

                T result = std::move(front_value->item);
                id_type associated_id = this->get_id(result);

                this->id_heap_map.erase(associated_id);
                this->pop_heap_node();

                return std::optional<T>(std::move(result));
            }

            auto has_expired_item(std::chrono::time_point<ClockType> time_bar) const noexcept -> bool
            {
                if (this->temporal_heap_sz == 0u)
                {
                    return false;
                }

                const std::unique_ptr<HeapNode>& front_value = this->temporal_heap.front();

                if (front_value->sched_time >= time_bar)
                {
                    return false;
                }

                return true;
            }
            
            auto size() const noexcept -> size_t
            {
                return this->temporal_heap_sz;
            }

            auto capacity() const noexcept -> size_t
            {
                return this->temporal_heap.size();
            }

            auto empty() const noexcept -> bool
            {
                return this->size() == 0u;
            }

        private:

            auto get_id(const T& item) -> id_type
            {
                return StatelessIdExtractor{}(item);
            }

            static void nullify_heap_node(std::unique_ptr<HeapNode>& arg) noexcept
            {
                arg->item       = {};
                arg->sched_time = {};
                arg->heap_idx   = {};
            }

            static void swap_heap_node(std::unique_ptr<HeapNode>& lhs,
                                       std::unique_ptr<HeapNode>& rhs) noexcept
            {
                std::swap(lhs->heap_idx, rhs->heap_idx);
                std::swap(lhs, rhs);
            }

            static auto is_less_than(const std::unique_ptr<HeapNode>& lhs,
                                     const std::unique_ptr<HeapNode>& rhs) noexcept -> bool
            {
                return lhs->sched_time < rhs->sched_time;
            }

            void correct_heap_node_up_at(size_t idx) noexcept
            {
                if constexpr(DEBUG_MODE_FLAG)
                {
                    if (idx >= this->temporal_heap_sz)
                    {
                        dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                if (idx == 0u)
                {
                    return;
                }

                size_t parent_idx = (idx - 1) >> 1;

                if (!is_less_than(this->temporal_heap[idx], this->temporal_heap[parent_idx]))
                {
                    return;
                }

                this->swap_heap_node(this->temporal_heap[idx], this->temporal_heap[parent_idx]);
                this->correct_heap_node_up_at(parent_idx);
            }

            void correct_heap_node_down_at(size_t idx) noexcept
            {
                if constexpr(DEBUG_MODE_FLAG)
                {
                    if (idx >= this->temporal_heap_sz)
                    {
                        dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                size_t cand_idx = idx * 2 + 1;

                if (cand_idx >= this->temporal_heap_sz)
                {
                    return;
                }

                if (cand_idx + 1 < this->temporal_heap_sz && is_less_than(this->temporal_heap[cand_idx + 1], this->temporal_heap[cand_idx]))
                {
                    cand_idx += 1;
                }

                if (!is_less_than(this->temporal_heap[cand_idx], this->temporal_heap[idx]))
                {
                    return;
                }

                this->swap_heap_node(this->temporal_heap[idx], this->temporal_heap[cand_idx]);
                this->correct_heap_node_down_at(cand_idx);
            } 

            void correct_heap_node_at(size_t idx) noexcept
            {
                if constexpr(DEBUG_MODE_FLAG)
                {
                    if (idx >= this->temporal_heap_sz)
                    {
                        dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                this->correct_heap_node_up_at(idx);
                this->correct_heap_node_down_at(idx);
            }

            template <class TypeLike>
            auto add_heap_node(TypeLike&& item,
                               std::chrono::time_point<ClockType> sched_time) noexcept -> std::expected<HeapNode *, exception_t>
            {
                if (this->temporal_heap_sz == this->temporal_heap.size())
                {
                    return std::unexpected(dg_sock::network_exception::RESOURCE_EXHAUSTION);
                }

                HeapNode * operating_node   = stdxx::safe_ptr_access(this->temporal_heap[this->temporal_heap_sz].get());

                operating_node->item        = std::forward<TypeLike>(item);
                operating_node->sched_time  = sched_time;
                operating_node->heap_idx    = this->temporal_heap_sz;

                this->temporal_heap_sz      += 1;

                this->correct_heap_node_up_at(this->temporal_heap_sz - 1);

                return operating_node;
            }

            void erase_heap_node_at(size_t idx) noexcept
            {
                if constexpr(DEBUG_MODE_FLAG)
                {
                    if (idx >= this->temporal_heap_sz)
                    {
                        dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                size_t back_node_idx = this->temporal_heap_sz - 1u;

                if (back_node_idx == idx)
                {
                    this->nullify_heap_node(this->temporal_heap[back_node_idx]);
                    this->temporal_heap_sz -= 1u;
                }
                else
                {
                    this->swap_heap_node(this->temporal_heap[idx], this->temporal_heap[back_node_idx]);
                    this->nullify_heap_node(this->temporal_heap[back_node_idx]);
                    this->temporal_heap_sz -= 1u;
                    this->correct_heap_node_at(idx);
                }
            }

            void pop_heap_node() noexcept
            {
                if constexpr(DEBUG_MODE_FLAG)
                {
                    if (this->temporal_heap_sz == 0u)
                    {
                        dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                size_t back_node_idx = this->temporal_heap_sz - 1u;

                if (back_node_idx == 0u)
                {
                    this->nullify_heap_node(this->temporal_heap[back_node_idx]);
                    this->temporal_heap_sz -= 1u;
                }
                else
                {
                    this->swap_heap_node(this->temporal_heap.front(), this->temporal_heap[back_node_idx]);
                    this->nullify_heap_node(this->temporal_heap[back_node_idx]);
                    this->temporal_heap_sz -= 1u;
                    this->correct_heap_node_down_at(0u);
                }
            }
    };

    //clear
    class TicketTimeoutManager: public virtual TicketTimeoutManagerInterface,
                                public virtual UpdatableInterface
    {
        public:

            struct TicketIdExtractor
            {
                constexpr auto operator()(const model::ticket_id_t& arg) -> model::ticket_id_t
                {
                    return arg;
                }
            };

            struct PushWaitBucket
            {
                ClockInArgument ** clock_in_ptr_arr;
                exception_t ** exception_ptr_arr;
                size_t clock_in_arr_sz;
                std::binary_semaphore * smp;
                std::chrono::time_point<std::chrono::steady_clock> since;
            };

            struct PopWaitBucket
            {
                model::ticket_id_t * output_arr;
                size_t * output_arr_sz;
                size_t output_arr_cap;
                std::binary_semaphore * smp;
            };

        private:

            dg_sock::pow2_cyclic_queue<PushWaitBucket> push_wait_bucket_vec;
            dg_sock::pow2_cyclic_queue<PopWaitBucket> pop_wait_bucket_vec;
            temporal_ordered_item_map<model::ticket_id_t, TicketIdExtractor, std::chrono::steady_clock> expiry_bucket_queue;
            std::unique_ptr<stdxx::fair_atomic_flag> mtx;
            stdxx::hdi_container<std::chrono::nanoseconds> max_dur;
            stdxx::hdi_container<size_t> max_consume_per_load;

        public:

            TicketTimeoutManager(dg_sock::pow2_cyclic_queue<PushWaitBucket> push_wait_bucket_vec,
                                 dg_sock::pow2_cyclic_queue<PopWaitBucket> pop_wait_bucket_vec,
                                 temporal_ordered_item_map<model::ticket_id_t, TicketIdExtractor, std::chrono::steady_clock> expiry_bucket_queue,
                                 std::unique_ptr<stdxx::fair_atomic_flag> mtx,
                                 stdxx::hdi_container<std::chrono::nanoseconds> max_dur,
                                 stdxx::hdi_container<size_t> max_consume_per_load) noexcept: push_wait_bucket_vec(std::move(push_wait_bucket_vec)),
                                                                                             pop_wait_bucket_vec(std::move(pop_wait_bucket_vec)),
                                                                                             expiry_bucket_queue(std::move(expiry_bucket_queue)),
                                                                                             mtx(std::move(mtx)),
                                                                                             max_dur(std::move(max_dur)),
                                                                                             max_consume_per_load(std::move(max_consume_per_load)){}

            void clock_in(ClockInArgument * registering_arr, size_t sz, exception_t * exception_arr) noexcept
            {
                if constexpr(DEBUG_MODE_FLAG)
                {
                    if (sz > this->max_consume_size())
                    {
                        dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                dg_sock::network_stack_allocation::NoExceptAllocation<std::add_pointer_t<ClockInArgument>[]> push_wait_bucket_arr(sz);
                dg_sock::network_stack_allocation::NoExceptAllocation<std::add_pointer_t<exception_t>[]> exception_ptr_arr(sz);
                size_t wait_sz = 0u;
                std::binary_semaphore smp(0);

                {
                    stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(*this->mtx);

                    auto now        = std::chrono::steady_clock::now(); 

                    for (size_t i = 0u; i < sz; ++i)
                    {
                        auto [ticket_id, current_dur] = std::make_pair(registering_arr[i].clocked_in_ticket, registering_arr[i].expiry_dur);

                        if (current_dur > this->max_clockin_dur())
                        {
                            exception_arr[i] = dg_sock::network_exception::REST_INVALID_TIMEOUT;
                            continue;
                        }

                        if (this->expiry_bucket_queue.size() == this->expiry_bucket_queue.capacity() || !this->push_wait_bucket_vec.empty())
                        {
                            push_wait_bucket_arr[wait_sz]   = std::next(registering_arr, i);
                            exception_ptr_arr[wait_sz]      = std::next(exception_arr, i);
                            wait_sz                         += 1u;

                            continue;
                        }

                        dg_sock::network_exception_handler::nothrow_log(this->expiry_bucket_queue.add(ticket_id, now + current_dur));
                        exception_arr[i] = dg_sock::network_exception::SUCCESS;
                    }

                    if (wait_sz != 0u)
                    {
                        dg_sock::network_exception_handler::nothrow_log(this->push_wait_bucket_vec.push_back(PushWaitBucket
                        {
                            .clock_in_ptr_arr   = push_wait_bucket_arr.get(),
                            .exception_ptr_arr  = exception_ptr_arr.get(),
                            .clock_in_arr_sz    = wait_sz,
                            .smp                = &smp,
                            .since              = now
                        }));
                    }
                }

                if (wait_sz != 0u)
                {
                    smp.acquire();
                }

            }

            void get_expired_ticket(model::ticket_id_t * ticket_arr, size_t& ticket_arr_sz, size_t ticket_arr_cap) noexcept
            {
                std::binary_semaphore smp(0);

                bool need_wait = [&]() noexcept
                {
                    stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(*this->mtx);

                    ticket_arr_sz   = 0u;
                    auto now        = std::chrono::steady_clock::now();

                    while (true)
                    {
                        if (ticket_arr_sz == ticket_arr_cap)
                        {
                            return false;
                        }

                        if (!this->expiry_bucket_queue.has_expired_item(now))
                        {
                            if (ticket_arr_sz == 0u)
                            {
                                dg_sock::network_exception_handler::nothrow_log(this->pop_wait_bucket_vec.push_back(PopWaitBucket
                                {
                                    .output_arr     = ticket_arr,
                                    .output_arr_sz  = &ticket_arr_sz,
                                    .output_arr_cap = ticket_arr_cap,
                                    .smp            = &smp
                                }));

                                return true;
                            }

                            return false;
                        }

                        ticket_arr[ticket_arr_sz++] = this->expiry_bucket_queue.get_expired_item(now).value();
                    }
                }();

                if (need_wait)
                {
                    smp.acquire();
                }
            }

            void void_ticket(model::ticket_id_t * ticket_arr, size_t sz) noexcept
            {
                stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(*this->mtx);

                for (size_t i = 0u; i < sz; ++i)
                {
                    this->expiry_bucket_queue.erase(ticket_arr[i]);
                }

                this->internal_update();
            }

            void update() noexcept
            {
                stdxx::xlock_guard<stdxx::fair_atomic_flag> lck_grd(*this->mtx);

                this->internal_update();
            }

            auto max_clockin_dur() const noexcept -> std::chrono::nanoseconds
            {
                return this->max_dur.value;
            }

            auto max_consume_size() noexcept -> size_t
            {
                return this->max_consume_per_load.value;
            }
        
        private:

            void internal_update() noexcept
            {
                auto now = std::chrono::steady_clock::now(); 

                while (true)
                {
                    if (this->expiry_bucket_queue.size() != this->expiry_bucket_queue.capacity())
                    {
                        bool can_progress   = !this->push_wait_bucket_vec.empty();

                        if (can_progress)
                        {
                            this->resolve_one_push_doable();
                            continue;
                        }
                    }

                    {
                        bool can_progress_1 = !this->pop_wait_bucket_vec.empty(); 
                        bool can_progress_2 = this->expiry_bucket_queue.has_expired_item(now);
                        bool can_progress   = can_progress_1 & can_progress_2;

                        if (can_progress)
                        {
                            this->resolve_one_pop_doable();
                            continue;
                        }
                    }

                    break;
                }
            }

            void resolve_one_push_doable() noexcept
            {
                if (this->expiry_bucket_queue.size() == this->expiry_bucket_queue.capacity())
                {
                    return;
                }

                if (this->push_wait_bucket_vec.empty())
                {
                    return;
                }

                PushWaitBucket& bucket = this->push_wait_bucket_vec.front(); 

                if constexpr(DEBUG_MODE_FLAG)
                {
                    if (bucket.clock_in_arr_sz == 0u)
                    {
                        dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                        std::abort();
                    }
                }

                dg_sock::network_exception_handler::nothrow_log(this->expiry_bucket_queue.add(bucket.clock_in_ptr_arr[0]->clocked_in_ticket,
                                                                                              bucket.since + bucket.clock_in_ptr_arr[0]->expiry_dur));

                *bucket.exception_ptr_arr[0]    = dg_sock::network_exception::SUCCESS;                
                bucket.clock_in_ptr_arr         = std::next(bucket.clock_in_ptr_arr);
                bucket.exception_ptr_arr        = std::next(bucket.exception_ptr_arr);
                bucket.clock_in_arr_sz          -= 1;

                if (bucket.clock_in_arr_sz == 0u)
                {
                    bucket.smp->release();
                    this->push_wait_bucket_vec.pop_front();

                    return;
                }
            }

            void resolve_one_pop_doable() noexcept
            {
                if (this->pop_wait_bucket_vec.empty())
                {
                    return;
                }

                auto now            = std::chrono::steady_clock::now(); 
                bool need_release   = false;

                [&]() noexcept
                {
                    while (true)
                    {
                        if (!this->expiry_bucket_queue.has_expired_item(now))
                        {
                            return;
                        }

                        PopWaitBucket& bucket                           = this->pop_wait_bucket_vec.front(); 

                        if constexpr(DEBUG_MODE_FLAG)
                        {
                            if (*bucket.output_arr_sz == bucket.output_arr_cap)
                            {
                                dg_sock::network_log_stackdump::critical(dg_sock::network_exception::verbose(dg_sock::network_exception::INTERNAL_CORRUPTION));
                                std::abort();
                            }
                        }

                        bucket.output_arr[(*bucket.output_arr_sz)++]    = this->expiry_bucket_queue.get_expired_item(now).value();
                        need_release                                    = true;

                        if (*bucket.output_arr_sz == bucket.output_arr_cap)
                        {
                            bucket.smp->release();
                            this->pop_wait_bucket_vec.pop_front();
                            need_release = false;

                            return;
                        }
                    }
                }();

                if (need_release)
                {
                    this->pop_wait_bucket_vec.front().smp->release();
                    this->pop_wait_bucket_vec.pop_front();
                } 
            }
    };

    //clear
    class DistributedTicketTimeoutManager: public virtual TicketTimeoutManagerInterface
    {
        private:

            std::unique_ptr<std::unique_ptr<TicketTimeoutManagerInterface>[]> base_arr;
            size_t pow2_base_arr_sz;
            size_t keyvalue_feed_cap;
            std::chrono::nanoseconds max_dur;
            size_t drain_peek_cap_per_container;
            size_t max_consume_per_load;

        public:

            DistributedTicketTimeoutManager(std::unique_ptr<std::unique_ptr<TicketTimeoutManagerInterface>[]> base_arr,
                                            size_t pow2_base_arr_sz,
                                            size_t keyvalue_feed_cap,
                                            std::chrono::nanoseconds max_dur,
                                            size_t drain_peek_cap_per_container,
                                            size_t max_consume_per_load) noexcept: base_arr(std::move(base_arr)),
                                                                                   pow2_base_arr_sz(pow2_base_arr_sz),
                                                                                   keyvalue_feed_cap(keyvalue_feed_cap),
                                                                                   max_dur(max_dur),
                                                                                   drain_peek_cap_per_container(drain_peek_cap_per_container),
                                                                                   max_consume_per_load(max_consume_per_load){}

            void clock_in(ClockInArgument * registering_arr, size_t sz, exception_t * exception_arr) noexcept
            {
                auto feed_resolutor                 = InternalClockInFeedResolutor{};
                feed_resolutor.manager_arr          = this->base_arr.get(); 

                size_t trimmed_keyvalue_feed_cap    = std::min(this->keyvalue_feed_cap, sz);
                size_t feeder_allocation_cost       = dg_sock::network_producer_consumer::delvrsrv_kv_allocation_cost(&feed_resolutor, trimmed_keyvalue_feed_cap);
                dg_sock::network_stack_allocation::NoExceptRawAllocation<char[]> feeder_mem(feeder_allocation_cost);
                auto feeder                         = dg_sock::network_exception_handler::nothrow_log(dg_sock::network_producer_consumer::delvrsrv_kv_open_preallocated_raiihandle(&feed_resolutor, trimmed_keyvalue_feed_cap, feeder_mem.get()));

                for (size_t i = 0u; i < sz; ++i)
                {
                    if (registering_arr[i].expiry_dur > this->max_clockin_dur())
                    {
                        exception_arr[i] = dg_sock::network_exception::REST_INVALID_TIMEOUT;
                        continue;
                    }

                    size_t hashed_value     = dg_sock::network_hash::hash_reflectible(registering_arr[i].clocked_in_ticket);
                    size_t partitioned_idx  = hashed_value & (this->pow2_base_arr_sz - 1u);

                    auto feed_arg           = InternalClockInFeedArgument{};
                    feed_arg.ticket_id      = registering_arr[i].clocked_in_ticket;
                    feed_arg.dur            = registering_arr[i].expiry_dur;
                    feed_arg.exception_ptr  = std::next(exception_arr, i); 

                    dg_sock::network_producer_consumer::delvrsrv_kv_deliver(feeder.get(), partitioned_idx, feed_arg);
                }
            }

            void get_expired_ticket(model::ticket_id_t * ticket_arr, size_t& ticket_arr_sz, size_t ticket_arr_cap) noexcept
            {
                this->internal_drain(ticket_arr, ticket_arr_sz, ticket_arr_cap);
            }

            void void_ticket(model::ticket_id_t * ticket_arr, size_t sz) noexcept
            {
                auto feed_resolutor                 = InternalVoidTicketResolutor{};
                feed_resolutor.manager_arr          = this->base_arr.get();

                size_t trimmed_keyvalue_feed_cap    = std::min(this->keyvalue_feed_cap, sz);
                size_t feeder_allocation_cost       = dg_sock::network_producer_consumer::delvrsrv_kv_allocation_cost(&feed_resolutor, trimmed_keyvalue_feed_cap);
                dg_sock::network_stack_allocation::NoExceptRawAllocation<char[]> feeder_mem(feeder_allocation_cost);
                auto feeder                         = dg_sock::network_exception_handler::nothrow_log(dg_sock::network_producer_consumer::delvrsrv_kv_open_preallocated_raiihandle(&feed_resolutor, trimmed_keyvalue_feed_cap, feeder_mem.get()));

                for (size_t i = 0u; i < sz; ++i)
                {
                    size_t hashed_value     = dg_sock::network_hash::hash_reflectible(ticket_arr[i]);
                    size_t partitioned_idx  = hashed_value & (this->pow2_base_arr_sz - 1u);

                    dg_sock::network_producer_consumer::delvrsrv_kv_deliver(feeder.get(), partitioned_idx, ticket_arr[i]);
                }
            }

            auto max_clockin_dur() const noexcept -> std::chrono::nanoseconds
            {
                return this->max_dur;
            }

            auto max_consume_size() noexcept -> size_t
            {
                return this->max_consume_per_load;
            }

        private:

            struct InternalVoidTicketResolutor: dg_sock::network_producer_consumer::KVConsumerInterface<size_t, model::ticket_id_t>
            {
                std::unique_ptr<TicketTimeoutManagerInterface> * manager_arr;

                void push(const size_t& partitioned_idx, std::move_iterator<model::ticket_id_t *> data_arr, size_t sz) noexcept
                {
                    this->manager_arr[partitioned_idx]->void_ticket(data_arr.base(), sz);
                }
            };

            struct InternalClockInFeedArgument
            {
                ticket_id_t ticket_id;
                std::chrono::nanoseconds dur;
                exception_t * exception_ptr;
            };

            struct InternalClockInFeedResolutor: dg_sock::network_producer_consumer::KVConsumerInterface<size_t, InternalClockInFeedArgument>
            {
                std::unique_ptr<TicketTimeoutManagerInterface> * manager_arr;

                void push(const size_t& partitioned_idx, std::move_iterator<InternalClockInFeedArgument *> data_arr, size_t sz) noexcept
                {    
                    InternalClockInFeedArgument * base_data_arr = data_arr.base();

                    dg_sock::network_stack_allocation::NoExceptAllocation<ClockInArgument[]> registering_arr(sz);
                    dg_sock::network_stack_allocation::NoExceptAllocation<exception_t[]> exception_arr(sz);

                    for (size_t i = 0u; i < sz; ++i)
                    {
                        registering_arr[i] = ClockInArgument{.clocked_in_ticket = base_data_arr[i].ticket_id, 
                                                             .expiry_dur        = base_data_arr[i].dur};
                    }

                    this->manager_arr[partitioned_idx]->clock_in(registering_arr.get(), sz, exception_arr.get());

                    for (size_t i = 0u; i < sz; ++i)
                    {
                        *base_data_arr[i].exception_ptr = exception_arr[i];
                    }
                }
            };

            void internal_drain(model::ticket_id_t * ticket_arr, size_t& ticket_arr_sz, size_t ticket_arr_cap) noexcept
            {
                size_t random_clue                      = dg_sock::network_randomizer::randomize_int<size_t>();
                ticket_arr_sz                           = 0u;
                model::ticket_id_t * current_ticket_arr = ticket_arr;
                size_t current_arr_cap                  = ticket_arr_cap;

                for (size_t i = 0u; i < this->pow2_base_arr_sz; ++i)
                {
                    if (current_arr_cap == 0u)
                    {
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
                            dg_sock::network_log_stackdump::error_fast_optional(dg_sock::network_exception::verbose(dg_sock::network_exception::REST_BAD_RESPONSE));
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
                    dg_sock::network_stack_allocation::NoExceptAllocation<MailBoxArgument[]> mailbox_arr(sz);

                    auto base_mailbox_arg = mailbox_arg.base(); 

                    for (size_t i = 0u; i < sz; ++i)
                    {
                        mailbox_arr[i].to           = base_mailbox_arg[i].to;
                        mailbox_arr[i].content      = static_cast<const void *>(base_mailbox_arg[i].content.data());
                        mailbox_arr[i].content_sz   = base_mailbox_arg[i].content.size();
                    }

                    dg_sock::network_kernel_mailbox::send(this->send_channel, mailbox_arr.get(), sz, exception_arr.get());

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

            std::vector<dg_sock::network_concurrency::daemon_raii_handle_t> daemon_vec;
            std::shared_ptr<RequestContainerInterface> request_container;
            std::shared_ptr<TicketControllerInterface> ticket_controller;
            std::shared_ptr<TicketTimeoutManagerInterface> ticket_timeout_manager;
            std::unique_ptr<RequestIDGeneratorInterface> request_id_generator;
            stdxx::hdi_container<size_t> max_consume_per_load;

        public:

            using self = RestController;

            RestController(std::vector<dg_sock::network_concurrency::daemon_raii_handle_t> daemon_vec,
                           std::shared_ptr<RequestContainerInterface> request_container,
                           std::shared_ptr<TicketControllerInterface> ticket_controller,
                           std::shared_ptr<TicketTimeoutManagerInterface> ticket_timeout_manager,
                           std::unique_ptr<RequestIDGeneratorInterface> request_id_generator,
                           stdxx::hdi_container<size_t> max_consume_per_load) noexcept: daemon_vec(std::move(daemon_vec)),
                                                                                       request_container(std::move(request_container)),
                                                                                       ticket_controller(std::move(ticket_controller)),
                                                                                       ticket_timeout_manager(std::move(ticket_timeout_manager)),
                                                                                       request_id_generator(std::move(request_id_generator)),
                                                                                       max_consume_per_load(std::move(max_consume_per_load)){}

            auto request(model::ClientRequest&& client_request) noexcept -> std::expected<std::unique_ptr<ResponseInterface>, exception_t>
            {
                std::expected<std::unique_ptr<BatchResponseInterface>, exception_t> resp = this->batch_request(std::make_move_iterator(std::addressof(client_request)), 1u);

                if (!resp.has_value())
                {
                    return std::unexpected(resp.error());
                }

                return self::internal_make_single_response(static_cast<std::unique_ptr<BatchResponseInterface>&&>(resp.value()));
            }

            auto batch_request(std::move_iterator<model::ClientRequest *> client_request_arr, size_t sz) noexcept -> std::expected<std::unique_ptr<BatchResponseInterface>, exception_t>
            {
                if (sz > this->max_consume_size())
                {
                    return std::unexpected(dg_sock::network_exception::MAX_CONSUME_SIZE_EXCEEDED);
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

                std::expected<std::unique_ptr<InternalBatchResponse>, exception_t> response = self::internal_make_batch_request_response(sz, ticket_id_arr.get(), this->ticket_controller); //open batch_response associated with the tickets, take ticket responsibility

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

                return std::unique_ptr<BatchResponseInterface>(std::move(response.value()));
            }

            auto get_designated_request_id(size_t request_id_sz, RequestID * out_request_id_arr) noexcept -> exception_t
            {
                if (request_id_sz > this->max_consume_size())
                {
                    return dg_sock::network_exception::MAX_CONSUME_SIZE_EXCEEDED;
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

                    std::unique_ptr<BatchRequestResponse> base;
                    std::unique_ptr<model::ticket_id_t[]> ticket_id_arr;
                    size_t ticket_id_arr_sz;
                    std::shared_ptr<TicketControllerInterface> ticket_controller;
                    bool was_released;

                public:

                    //defer construction -> factory, all deferred constructions are marked as noexcept to avoid leaks, such is malloc() -> inplace -> return, fails can only happen at malloc 

                    InternalBatchResponse(std::unique_ptr<BatchRequestResponse> base,
                                          std::unique_ptr<model::ticket_id_t[]> ticket_id_arr,
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

                    std::unique_ptr<BatchResponseInterface> base;
                
                public:

                    InternalSingleResponse(std::unique_ptr<BatchResponseInterface> base) noexcept: base(std::move(base)){}

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
                                                             std::shared_ptr<TicketControllerInterface> ticket_controller) noexcept -> std::expected<std::unique_ptr<InternalBatchResponse>, exception_t>
            {
                if (request_sz == 0u)
                {
                    return std::unexpected(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                if (ticket_controller == nullptr)
                {
                    return std::unexpected(dg_sock::network_exception::INVALID_ARGUMENT);
                }

                std::expected<std::unique_ptr<BatchRequestResponse>, exception_t> base = dg_sock::network_rest_frame::client_impl1::make_batch_request_response(request_sz);

                if (!base.has_value())
                {
                    return std::unexpected(base.error());
                }

                std::expected<std::unique_ptr<model::ticket_id_t[]>, exception_t> cpy_ticket_id_arr = dg_sock::network_allocation::cstyle_make_unique<ticket_id_t[]>(request_sz);

                if (!cpy_ticket_id_arr.has_value())
                {
                    return std::unexpected(cpy_ticket_id_arr.error());
                }

                std::copy(stdxx::safe_ptr_access(ticket_id_arr), std::next(ticket_id_arr, request_sz), cpy_ticket_id_arr.value().get());
                std::expected<std::unique_ptr<InternalBatchResponse>, exception_t> rs = dg_sock::network_allocation::cstyle_make_unique<InternalBatchResponse>(std::move(base.value()), 
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

            static auto internal_make_single_response(std::unique_ptr<BatchResponseInterface>&& base) noexcept -> std::expected<std::unique_ptr<ResponseInterface>, exception_t>
            {
                return dg_sock::network_allocation::cstyle_make_unique<InternalSingleResponse>(static_cast<std::unique_ptr<BatchResponseInterface>&&>(base));
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

            auto request(model::ClientRequest&& request) noexcept -> std::expected<std::unique_ptr<ResponseInterface>, exception_t>
            {
                size_t random_clue  = dg_sock::network_randomizer::randomize_int<size_t>();
                size_t idx          = random_clue & (this->pow2_rest_controller_arr_sz - 1u);

                return this->rest_controller_arr[idx]->request(static_cast<model::ClientRequest&&>(request));
            }

            auto batch_request(std::move_iterator<model::ClientRequest *> request_arr, size_t request_arr_sz) noexcept -> std::expected<std::unique_ptr<BatchResponseInterface>, exception_t>
            {
                if (request_arr_sz > this->max_consume_size())
                {
                    return std::unexpected(dg_sock::network_exception::MAX_CONSUME_SIZE_EXCEEDED);
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
                    return dg_sock::network_exception::MAX_CONSUME_SIZE_EXCEEDED;
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

    };
}

namespace dg_sock::network_rest_frame::client_instance
{
    using namespace dg_sock::network_rest_frame::model;

    void init(const BuilderConfig& config)
    {

    }

    void deinit() noexcept
    {

    }
}

namespace dg_sock::network_rest_frame::client
{
    using Url               = dg_sock::network_rest_frame::model::ResourceAddress;
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

    class RequestRetryMachineInterface
    {
        public:

            virtual ~RequestRetryMachineInterface() noexcept = default;

            virtual auto get_retryable_promise(dg_sock::unique_ptr<PromiseFactoryInterface<ClientResponse>>&& factory) -> dg_sock::unique_ptr<Promise<ClientRepsonse>> = 0;
    };

    class RequestRetryMachineFactory
    {
        public:

            using retry_policy_t = uint8_t;

            static inline constexpr retry_policy_t EXPONENTIAL_EASY     = 0u;
            static inline constexpr retry_policy_t EXPONENTIAL_MEDIUM   = 1u;
            static inline constexpr retry_policy_t EXPONENTIAL_HARD     = 2u;

            static inline constexpr retry_policy_t UNIFORM_EASY         = 3u;
            static inline constexpr retry_policy_t UNIFORM_MEDIUM       = 4u;
            static inline constexpr retry_policy_t UNIFORM_HARD         = 5u;

            auto get(retry_policy_t retry_policy) -> dg_sock::unique_ptr<RequestRetryMachineInterface>
            {
                switch (retry_policy)
                {
                    case EXPONENTIAL_EASY:
                    {
                        break;
                    }
                    case EXPONENTIAL_MEDIUM:
                    {
                        break;
                    }
                    case EXPONENTIAL_HARD:
                    {
                        break;
                    }
                    case UNIFORM_EASY:
                    {
                        break;
                    }
                    case UNIFORM_MEDIUM:
                    {
                        break;
                    }
                    case UNIFORM_HARD:
                    {
                        break;
                    }
                    default:
                    {
                        throw std::invalid_argument("bad retry policy, enumeration out of range");
                    }
                }                
            }
    };

    using retry_policy_t = RequestRetryMachineFactory::retry_policy_t;

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
                    .payload_serialization_method   = this->_serialization_method.value(),
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
            retry_policy_t retry_policy;
            std::shared_ptr<ClientRequest> client_request;
            bool is_distinct_request;

            RequestDispatcher(Resolutor resolutor,
                              retry_policy_t retry_policy,
                              std::shared_ptr<ClientRequest> client_request,
                              bool is_distinct_request): resolutor(std::move(resolutor)),
                                                         retry_policy(std::move(retry_policy)),
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
                                                   this->get_default_retry_policy(),
                                                   nullptr,
                                                   false){}

            RequestDispatcher(Resolutor resolutor): RequestDispatcher(std::move(resolutor),
                                                                      this->get_default_retry_policy(),
                                                                      nullptr,
                                                                      false){}

            auto set_retry_policy(retry_policy_t retry_policy) -> self&
            {
                this->check_and_throw_retry_policy(retry_policy)
                this->retry_policy = retry_policy;

                return *this;
            }

            auto set_request(ClientRequest client_request) -> self&
            {
                this->client_request = dg_sock::network_allocation::make_shared(std::move(client_request));

                return *this;
            }

            template <class NewResolutor>
            auto set_resolutor(NewResolutor&& resolutor) -> RequestDispatcher<std::decay_t<NewResolutor>>
            {
                RequestDispatcher rs(std::forward<NewResolutor>(resolutor),
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
                return RequestRetryMachineFactory{}.get(this->retry_policy)->get_retryable_promise(dg_sock::make_unique<InternalPromiseFactory>(this->get_full_request()));
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

                return client_instance::get_request_id();
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

            consteval auto get_default_retry_policy() const -> retry_policy_t
            {
                return RequestRetryMachineFactory::EXPONENTIAL_EASY;
            }

            void check_and_throw_retry_policy(retry_policy_t retry_policy)
            {
                RequestRetryMachineFactory{}.get(retry_policy);
            }

            class PromiseWrapper: public virtual Promise<ClientRepsonse>
            {
                private:

                    dg_sock::unique_ptr<dg_sock::network_rest_frame::client::ResponseInterface> base;

                public:

                    PromiseWrapper(dg_sock::unique_ptr<dg_sock::network_rest_frame::client::ResponseInterface> base): base(std::move(base)){}

                    auto is_completed() noexcept -> bool
                    {
                        return this->base->is_completed();
                    }

                    auto wait() -> ClientResponse
                    {
                        std::expected<ClientResponse, exception_t> response = this->base->response();

                        if (!response.has_value())
                        {
                            dg_sock::network_exception::throw_exception(response.error());
                        }

                        return std::move(response.value());
                    }
            };

            class InternalPromiseFactory: public virtual PromiseFactoryInterface<ClientResponse>
            {
                private:

                    ClientRequest request;

                public:

                    InternalPromiseFactory(ClientRequest request): request(std::move(request)){}

                    auto get() -> dg_sock::unique_ptr<Promise<ClientResponse>>
                    {
                        return dg_sock::make_unique<PromiseWrapper>(client_instance::request(this->request));
                    }
            };
    };

    class RequestClient
    {
        private:

            retry_policy_t retry_policy;
            bool is_unique_request;

        public:

            RequestClient(): retry_policy(this->get_default_retry_policy()),
                             is_unique_request(false){}

            auto set_retry_policy(retry_policy_t retry_policy) -> self&
            {
                this->check_and_nothrow_retry_policy(retry_policy);
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

                rs.set_retry_policy(this->retry_policy);
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

            consteval auto get_default_retry_policy() const -> retry_policy_t
            {
                return RequestRetryMachineFactory::EXPONENTIAL_EASY;
            }

            void check_and_throw_retry_policy(retry_policy_t retry_policy)
            {
                RequestRetryMachineFactory{}.get(retry_policy);
            }
    };
}

#endif