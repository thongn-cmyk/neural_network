#ifndef __DG_CONNECTIVITY_SUBSYSTEM_H__
#define __DG_CONNECTIVITY_SUBSYSTEM_H__

#include <stdint.h>
#include <stdlib.h>
#include <unordered_map>
#include <vector>
#include <variant>
#include <stl_extension/stdx.h>
#include <mutex_extension/fair_mutex.h>
#include <internal_rest/network_rest_frame.h>
#include <serializer/compact_serializer.h>
#include <cron_subsystem/cron_subsystem.h>
#include <global_config/rest_config.h>

namespace connectivity_subsystem
{
    //connectivity subsystem is an ownership model to hint of a resource cleanup of a remote address

    //in the normal case, is_alive should not affect the entire the process, and follows the sequence of master requests slave_open, master requests slave_solve, and master requests slave_close
    //in the not normal case, the hint should close the remote resource in the worst time of master_alive_time + connectivity_timeout_dur
    //so it's a guarantee and a hint of lifetime

    using local_err_code_t = uint8_t;

    static inline constexpr local_err_code_t SUCCESS                            = 0u;
    static inline constexpr local_err_code_t CONNECTION_NOT_FOUND_ERROR_CODE    = 1u;
    static inline constexpr local_err_code_t CLOSED_CONNECTION_ERROR_CODE       = 2u;
    static inline constexpr local_err_code_t PING_ROOM_OVERFLOW_ERROR_CODE      = 3u;
    static inline constexpr local_err_code_t OTHER_ERROR_CODE                   = 4u;

    struct connection_not_found_error: std::invalid_argument
    {
        connection_not_found_error(): std::invalid_argument("specified connection_id does not link to a registered entity"){}
    };

    struct closed_connection_error: std::runtime_error
    {
        closed_connection_error(): std::runtime_error("connection not found for the specified entity"){}
    };

    struct ping_room_overflow_error: std::runtime_error
    {
        ping_room_overflow_error(): std::runtime_error("max ping population reached for the specified connection entity"){}
    };

    auto exception_to_verbal_string(std::exception_ptr exception) -> std::string_view
    {
        try
        {
            std::rethrow_exception(exception);
        }
        catch (connection_not_found_error& e)
        {
            return "conenction not found error";
        }
        catch (closed_connection_error& e)
        {
            return "closed connection error";
        }
        catch (ping_room_overflow_error& e)
        {
            return "ping room overflow error";
        }
        catch (std::exception& e)
        {
            return "unidentifier error";
        }

        return "success";
    }

    auto exception_to_error_code(std::exception_ptr exception) -> local_err_code_t
    {
        try
        {
            std::rethrow_exception(exception);
        }
        catch (connection_not_found_error& e)
        {
            return CONNECTION_NOT_FOUND_ERROR_CODE;
        }
        catch (closed_connection_error& e)
        {
            return CLOSED_CONNECTION_ERROR_CODE;
        }
        catch (ping_room_overflow_error& e)
        {
            return PING_ROOM_OVERFLOW_ERROR_CODE;
        }
        catch (std::exception& e)
        {
            return OTHER_ERROR_CODE;
        }

        return SUCCESS;
    }

    void throw_error_code(local_err_code_t err_code)
    {
        switch (err_code)
        {
            case SUCCESS:
            {
                break;
            }
            case CONNECTION_NOT_FOUND_ERROR_CODE:
            {
                throw connection_not_found_error{};
            }
            case CLOSED_CONNECTION_ERROR_CODE:
            {
                throw closed_connection_error{};
            }
            case PING_ROOM_OVERFLOW_ERROR_CODE:
            {
                throw ping_room_overflow_error{};
            }
            case OTHER_ERROR_CODE:
            {
                throw std::runtime_error("something went wrong");
            }
            default:
            {
                throw std::runtime_error("invalid error code");
            }
        }
    }

    struct MasterConfiguration
    {
        std::chrono::nanoseconds connection_timeout_dur;
        std::chrono::nanoseconds connection_broke_dur;
        std::chrono::nanoseconds abs_timeout_dur;
        uint64_t ping_retry_count;
        std::chrono::nanoseconds ping_retry_break_dur;
        uint64_t slave_count;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(connection_timeout_dur,
                      connection_broke_dur,
                      abs_timeout_dur,
                      ping_retry_count,
                      ping_retry_break_dur,
                      slave_count);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(connection_timeout_dur,
                      connection_broke_dur,
                      abs_timeout_dur,
                      ping_retry_count,
                      ping_retry_break_dur,
                      slave_count);
        }
    };

    struct MasterPayload
    {
        dg_sock::network_rest_frame::model::Url url;
        uint64_t topic_id;
        uint64_t ping_retry_count;
        std::chrono::nanoseconds ping_retry_break_dur;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(url,
                      topic_id,
                      ping_retry_count,
                      ping_retry_break_dur);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(url,
                      topic_id,
                      ping_retry_count,
                      ping_retry_break_dur);
        }
    };

    struct SlaveConfiguration
    {
        MasterPayload master_payload;

        std::chrono::nanoseconds connection_timeout_dur;
        std::chrono::nanoseconds abs_timeout_dur;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(master_payload,
                      connection_timeout_dur,
                      abs_timeout_dur);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(master_payload,
                      connection_timeout_dur,
                      abs_timeout_dur);
        }
    };

    struct PingRequest
    {
        uint64_t topic_id;
        std::string pinger;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(topic_id, pinger);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(topic_id, pinger);
        }
    };

    struct PingResponse
    {
        local_err_code_t result;
        std::string err_verbal_description;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(result, err_verbal_description);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(result, err_verbal_description);
        }
    };

    struct ConnectionTopic
    {
        MasterConfiguration master_config;
        std::unordered_map<std::string, std::chrono::time_point<std::chrono::steady_clock>> who_when_map;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(master_config, who_when_map);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(master_config, who_when_map);
        }
    };

    class ConnectionInterface
    {
        public:

            virtual ~ConnectionInterface() noexcept = default;
            virtual void close() noexcept = 0;
            virtual auto is_alive() -> bool = 0;
    };

    class ConnectionControllerInterface
    {
        public:

            virtual ~ConnectionControllerInterface() noexcept = default;
            virtual auto open_connection(const MasterConfiguration& config) -> uint64_t = 0;
            virtual auto get_connection(uint64_t connection_id) -> std::optional<ConnectionTopic> = 0;
            virtual void ping_connection(uint64_t connection_id, const std::string& who) = 0;
            virtual void close_connection(uint64_t connection_id) noexcept = 0;
    };

    class ConnectionPingerInterface
    {
        public:

            virtual ~ConnectionPingerInterface() noexcept = default;
            virtual auto ping(const MasterPayload& payload) -> std::shared_ptr<dg_sock::network_rest_frame::client::Promise<stdx::fancy_void>> = 0;
    };

    class ConnectionController: public virtual ConnectionControllerInterface
    {
        private:

            std::unordered_map<uint64_t, ConnectionTopic> id_topic_map;
            uint32_t id_counter;
            std::unique_ptr<fair_mutex::fair_atomic_flag> mtx;

        public:

            ConnectionController(): id_topic_map(),
                                    id_counter(0u),
                                    mtx(fair_mutex::make_unique_fair_atomic_flag()){}

            auto open_connection(const MasterConfiguration& config) -> uint64_t
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                uint64_t nxt_id             = id_counter;
                this->id_topic_map[nxt_id]  = ConnectionTopic
                {
                    .master_config  = config,
                    .who_when_map   = {}
                };
                this->id_counter            += 1;

                return nxt_id;
            }

            auto get_connection(uint64_t connection_id) -> std::optional<ConnectionTopic>
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (auto map_ptr = this->id_topic_map.find(connection_id); map_ptr != this->id_topic_map.end())
                {
                    return map_ptr->second;
                }

                return std::nullopt;
            }

            void ping_connection(uint64_t connection_id, const std::string& who)
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                auto map_ptr = this->id_topic_map.find(connection_id);

                if (map_ptr == this->id_topic_map.end())
                {
                    throw connection_not_found_error();
                }

                auto map_ptr_2 = map_ptr->second.who_when_map.find(who);

                if (map_ptr_2 == map_ptr->second.who_when_map.end())
                {
                    if (map_ptr->second.who_when_map.size() == map_ptr->second.master_config.slave_count)
                    {
                        throw ping_room_overflow_error();
                    }

                    auto [new_map_ptr, _]   = map_ptr->second.who_when_map.insert({who, {}});
                    map_ptr_2               = new_map_ptr;
                }

                map_ptr_2->second = std::chrono::steady_clock::now();
            }

            void close_connection(uint64_t connection_id) noexcept
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                this->id_topic_map.erase(connection_id);
            }
    };

    class DistributedConnectionController: public virtual ConnectionControllerInterface
    {
        private:

            std::vector<std::unique_ptr<ConnectionController>> base_vec;
            std::unique_ptr<std::atomic<size_t>> seed;

        public:

            DistributedConnectionController(uint32_t concurrency_sz): base_vec()
            {
                if (concurrency_sz == 0u)
                {
                    throw std::invalid_argument("bad concurrency size, 0");
                }

                this->seed      = std::make_unique<std::atomic<size_t>>(0u);

                for (size_t i = 0u; i < concurrency_sz; ++i)
                {
                    this->base_vec.push_back(std::make_unique<ConnectionController>());
                }
            }

            auto open_connection(const MasterConfiguration& config) -> uint64_t
            {
                if (this->base_vec.size() == 0u)
                {
                    std::abort();
                }

                uint32_t slot_idx           = this->seed->fetch_add(1u, std::memory_order_relaxed) % this->base_vec.size();
                uint32_t base_connection_id = stdx::nothrow_integer_cast<uint32_t>(this->base_vec[slot_idx]->open_connection(config));

                return this->encode(base_connection_id, slot_idx);
            }

            auto get_connection(uint64_t connection_id) -> std::optional<ConnectionTopic>
            {
                auto [base_connection_id, slot_idx] = this->decode(connection_id);

                if (slot_idx >= this->base_vec.size())
                {
                    return std::nullopt;
                }

                return this->base_vec[slot_idx]->get_connection(base_connection_id);
            }

            void ping_connection(uint64_t connection_id, const std::string& who)
            {
                auto [base_connection_id, slot_idx] = this->decode(connection_id);

                if (slot_idx >= this->base_vec.size())
                {
                    throw std::invalid_argument("bad connection id, invalid connection id");
                }

                this->base_vec[slot_idx]->ping_connection(base_connection_id, who);
            }

            void close_connection(uint64_t connection_id) noexcept
            {
                auto [base_connection_id, slot_idx] = this->decode(connection_id);

                if (slot_idx >= this->base_vec.size())
                {
                    return;
                }

                this->base_vec[slot_idx]->close_connection(base_connection_id);
            }

        private:

            static constexpr auto encode(uint32_t hi, uint32_t lo) noexcept -> uint64_t
            {
                return (static_cast<uint64_t>(hi) << 32) | static_cast<uint64_t>(lo);
            }

            static constexpr auto decode(uint64_t encoded) noexcept -> std::pair<uint32_t, uint32_t>
            {
                uint32_t lo = encoded & static_cast<uint64_t>(std::numeric_limits<uint32_t>::max());
                uint32_t hi = encoded >> 32;

                return {hi, lo};
            }
    };

    class PingResolver: public virtual dg_sock::network_rest_frame::server::OneRequestHandlerInterface
    {
        private:

            std::shared_ptr<ConnectionControllerInterface> connection_controller;

        public:
            
            static inline constexpr std::string_view RESOLVABLE_PATH = "connectivity_subsystem/ping_resolver";

            using Request   = dg_sock::network_rest_frame::model::Request;
            using Response  = dg_sock::network_rest_frame::model::Response;

            PingResolver(std::shared_ptr<ConnectionControllerInterface> connection_controller): connection_controller(std::move(connection_controller))
            {
                if (this->connection_controller == nullptr)
                {
                    throw std::invalid_argument("bad connection controller, null");
                }
            }

            auto handle(const Request& request) -> Response
            {
                if (request.payload_serialization_format != dg::network_compact_serializer::get_dgstd_serialization_identifier())
                {
                    throw std::invalid_argument("unexpected request type, bad serialization method");
                }

                PingRequest semantic_request = dg::network_compact_serializer::dgstd_deserialize<PingRequest>(request.payload);
                PingResponse semantic_response;

                try
                {
                    this->connection_controller->ping_connection(semantic_request.topic_id, semantic_request.pinger);
                    semantic_response = PingResponse
                    {
                        .result = SUCCESS,
                        .err_verbal_description = ""
                    };
                }
                catch (...)
                {
                    semantic_response = PingResponse
                    {
                        .result = exception_to_error_code(std::current_exception()),
                        .err_verbal_description = std::string(exception_to_verbal_string(std::current_exception()))
                    };
                }

                return Response
                {
                    .response = dg::network_compact_serializer::dgstd_serialize<dg_sock::string>(semantic_response),
                    .response_serialization_format = dg_sock::string(dg::network_compact_serializer::get_dgstd_serialization_identifier()),
                    .err_code = dg_sock::network_exception::SUCCESS
                };
            }
    };

    class MasterPinger: public virtual ConnectionPingerInterface
    {
        private:

            dg_sock::network_rest_frame::client::RequestClient client;
            std::string identifier;

        public:

            MasterPinger(): client(),
                            identifier(stdx::get_random_identifier()){}

            auto ping(const MasterPayload& payload) -> std::shared_ptr<dg_sock::network_rest_frame::client::Promise<stdx::fancy_void>>
            {
                using namespace dg_sock::network_rest_frame::client;

                PingRequest ping_request
                {
                    .topic_id   = payload.topic_id,
                    .pinger     = this->identifier
                };

                std::string ping_payload    = dg::network_compact_serializer::dgstd_serialize<std::string>(ping_request);
                ClientRequest request       = RequestFactory{}.url(payload.url)
                                                              .payload(ping_payload)
                                                              .serialization_method(dg::network_compact_serializer::get_dgstd_serialization_identifier())
                                                              .get();

                auto resolutor = [](const ClientResponse& response)
                {
                    if (dg_sock::network_exception::is_failed(response.err_code))
                    {
                        dg_sock::network_excepiton::throw_exception(response.err_code);
                    }

                    if (std::string_view(response.response_serialization_format) != dg::network_compact_serializer::get_dgstd_serialization_identifier())
                    {
                        throw std::runtime_error("unexpected serialization format");
                    }

                    PingResponse semantic_response = dg::network_compact_serializer::dgstd_deserialize<PingResponse>(response.response);
                    throw_error_code(semantic_response.result);

                    return stdx::fancy_void{};
                };

                return this->client.request(request)
                                   .set_retry_policy(RequestRetryMachineFactory<>::EXPONENTIAL_MEDIUM)
                                   .set_resolutor(resolutor)
                                   .get_promise();
            }
    };

    struct Signature{};

    using ConnectionControllerSingleton = stdx::shared_ptr_singleton_container<DistributedConnectionController, Signature>;

    static inline constexpr size_t CONCURRENCY_SZ = 128u;

    void init()
    {
        ConnectionControllerSingleton::initialize(CONCURRENCY_SZ);
        dg_sock::network_rest_frame::server_instance::hook(PingResolver::RESOLVABLE_PATH, std::make_unique<PingResolver>(ConnectionControllerSingleton::get()));
    }

    void deinit() noexcept
    {
        dg_sock::network_rest_frame::server_instance::unhook(PingResolver::RESOLVABLE_PATH);
        ConnectionControllerSingleton::deinitialize();
    }

    class MasterConnection: public virtual ConnectionInterface
    {
        private:

            MasterConfiguration master_config;
            std::shared_ptr<ConnectionControllerInterface> controller;
            uint64_t connection_id;
            std::shared_ptr<std::atomic<bool>> is_alive_flag;
            std::shared_ptr<std::atomic<bool>> is_connection_disposed;
            std::shared_ptr<void> cron_obj;
            bool is_closed;

        public:

            static inline const std::chrono::nanoseconds MIN_CONNECTION_TIMEOUT_DUR = std::chrono::nanoseconds(0);
            static inline const std::chrono::nanoseconds MAX_CONNECTION_TIMEOUT_DUR = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::hours(1));
            static inline const std::chrono::nanoseconds MIN_CONNECTION_BROKE_DUR   = std::chrono::nanoseconds(0);
            static inline const std::chrono::nanoseconds MAX_CONNECTION_BROKE_DUR   = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::hours(1));
            static inline const std::chrono::nanoseconds MIN_ABS_TIMEOUT_DUR        = std::chrono::nanoseconds(0);
            static inline const std::chrono::nanoseconds MAX_ABS_TIMEOUT_DUR        = std::chrono::nanoseconds::max();
            static inline const std::chrono::nanoseconds CRON_JOB_DURATION          = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::milliseconds(100));
            static inline const uint64_t MIN_PING_RETRY_COUNT                       = 0u;
            static inline const uint64_t MAX_PING_RETRY_COUNT                       = std::numeric_limits<uint64_t>::max();
            static inline const std::chrono::nanoseconds MIN_PING_RETRY_BREAK_DUR   = std::chrono::nanoseconds(0);
            static inline const std::chrono::nanoseconds MAX_PING_RETRY_BREAK_DUR   = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::hours(1));
            static inline const uint64_t MIN_SLAVE_COUNT                            = 1u;
            static inline const uint64_t MAX_SLAVE_COUNT                            = std::numeric_limits<uint64_t>::max(); 

            MasterConnection(const MasterConfiguration& config,
                             std::shared_ptr<ConnectionControllerInterface> controller = ConnectionControllerSingleton::get())
            {
                if (controller == nullptr)
                {
                    throw std::invalid_argument("bad controller, null");
                }

                this->check_and_throw_configuration(config);

                this->master_config = config;
                this->controller    = controller;
                this->connection_id = controller->open_connection(config);

                try
                {
                    this->is_alive_flag             = std::make_shared<std::atomic<bool>>(true);
                    this->is_connection_disposed    = std::make_shared<std::atomic<bool>>(false);
                    auto resolutor_obj              = std::make_shared<InternalResolutor>(this->controller,
                                                                                          this->connection_id,
                                                                                          this->is_alive_flag,
                                                                                          this->is_connection_disposed);

                    this->cron_obj      = cron_subsystem::register_periodic_cronjob(resolutor_obj, CRON_JOB_DURATION);
                    this->is_closed     = false;

                }
                catch (...)
                {
                    controller->close_connection(this->connection_id);
                    throw;
                }
            }

            MasterConnection(const MasterConnection&) = delete;
            MasterConnection& operator =(const MasterConnection&) = delete;

            ~MasterConnection() noexcept
            {
                this->close();
            }

            auto get_slave_configuration() -> SlaveConfiguration
            {
                if (this->is_closed)
                {
                    throw closed_connection_error{};
                }

                return this->internal_get_slave_configuration();
            }

            void close() noexcept
            {
                if (std::exchange(this->is_closed, true))
                {
                    return;
                }

                this->cron_obj = nullptr;

                if (this->is_connection_disposed->exchange(true, std::memory_order_relaxed))
                {
                    return;
                }

                this->controller->close_connection(this->connection_id);
            }

            auto is_alive() -> bool
            {
                return !this->is_closed && this->is_alive_flag->load(std::memory_order_relaxed);
            }
        
        private:

            void check_and_throw_configuration(const MasterConfiguration& config)
            {
                if (std::clamp(config.connection_timeout_dur, MIN_CONNECTION_TIMEOUT_DUR, MAX_CONNECTION_TIMEOUT_DUR) != config.connection_timeout_dur)
                {
                    throw std::invalid_argument("bad connection timeout duration, duration out of range");
                }

                if (std::clamp(config.connection_broke_dur, MIN_CONNECTION_BROKE_DUR, MAX_CONNECTION_BROKE_DUR) != config.connection_broke_dur)
                {
                    throw std::invalid_argument("bad connection broke duration, duration out of range");
                }

                if (std::clamp(config.abs_timeout_dur, MIN_ABS_TIMEOUT_DUR, MAX_ABS_TIMEOUT_DUR) != config.abs_timeout_dur)
                {
                    throw std::invalid_argument("bad absolute timeout duration, duration out of range");
                }

                if (std::clamp(config.ping_retry_count, MIN_PING_RETRY_COUNT, MAX_PING_RETRY_COUNT) != config.ping_retry_count)
                {
                    throw std::invalid_argument("bad ping retry count, value out of range");
                }

                if (std::clamp(config.ping_retry_break_dur, MIN_PING_RETRY_BREAK_DUR, MAX_PING_RETRY_BREAK_DUR) != config.ping_retry_break_dur)
                {
                    throw std::invalid_argument("bad ping retry break duration, duration out of range");
                }

                if (std::clamp(config.slave_count, MIN_SLAVE_COUNT, MAX_SLAVE_COUNT) != config.slave_count)
                {
                    throw std::invalid_argument("bad slave count, value out of range");
                }
            }

            auto get_master_payload_url() -> dg_sock::network_rest_frame::model::Url
            {
                return 
                {
                    .remote_addr    = dg_sock::network_rest_frame::client_instance::address(),
                    .resource_addr  = dg_sock::string(PingResolver::RESOLVABLE_PATH),
                    .channel        = global_config::rest_config::HIGH_AVAILABILITY_CHANNEL
                };
            }

            auto internal_get_slave_configuration() -> SlaveConfiguration
            {
                return SlaveConfiguration
                {
                    .master_payload = MasterPayload
                    {
                        .url                    = this->get_master_payload_url(),
                        .topic_id               = this->connection_id,
                        .ping_retry_count       = this->master_config.ping_retry_count,
                        .ping_retry_break_dur   = this->master_config.ping_retry_break_dur
                    },
                    .connection_timeout_dur = this->master_config.connection_timeout_dur,
                    .abs_timeout_dur        = this->master_config.abs_timeout_dur,
                };
            }

            class InternalResolutor: public virtual cron_subsystem::UpdatableInterface
            {
                private:

                    std::shared_ptr<ConnectionControllerInterface> controller;
                    uint64_t connection_id;
                    std::shared_ptr<std::atomic<bool>> is_alive_flag;
                    std::shared_ptr<std::atomic<bool>> is_connection_disposed;
                    std::optional<std::chrono::time_point<std::chrono::steady_clock>> first_timepoint;
                    std::optional<std::chrono::time_point<std::chrono::steady_clock>> broke_timepoint;
                    bool is_connection_dispose_inquired;

                public:

                    InternalResolutor(std::shared_ptr<ConnectionControllerInterface> controller,
                                      uint64_t connection_id,
                                      std::shared_ptr<std::atomic<bool>> is_alive_flag,
                                      std::shared_ptr<std::atomic<bool>> is_connection_disposed): controller(std::move(controller)),
                                                                                                  connection_id(connection_id),
                                                                                                  is_alive_flag(std::move(is_alive_flag)),
                                                                                                  is_connection_disposed(std::move(is_connection_disposed)),
                                                                                                  first_timepoint(std::nullopt),
                                                                                                  broke_timepoint(std::nullopt),
                                                                                                  is_connection_dispose_inquired(false){}

                    void update()
                    {
                        if (!this->is_alive_flag->load(std::memory_order_relaxed))
                        {
                            if (std::exchange(this->is_connection_dispose_inquired, true))
                            {
                                return;
                            }

                            if (this->is_connection_disposed->exchange(true, std::memory_order_relaxed))
                            {
                                return;
                            }

                            this->controller->close_connection(this->connection_id);
                            return;
                        }

                        if (!this->first_timepoint.has_value())
                        {
                            this->first_timepoint = std::chrono::steady_clock::now();
                        }

                        std::optional<ConnectionTopic> topic = this->controller->get_connection(this->connection_id);

                        if (!topic.has_value())
                        {
                            this->is_alive_flag->exchange(false, std::memory_order_relaxed);
                            return;
                        }

                        if (topic->master_config.slave_count == 0u)
                        {
                            std::abort();
                        }

                        if (topic->who_when_map.size() == topic->master_config.slave_count)
                        {
                            if (!this->broke_timepoint.has_value())
                            {
                                this->broke_timepoint = std::chrono::steady_clock::now();
                            }
                        }

                        if (!this->broke_timepoint.has_value())
                        {
                            std::chrono::nanoseconds lapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - this->first_timepoint.value());

                            if (lapsed >= topic->master_config.connection_broke_dur)
                            {
                                this->is_alive_flag->exchange(false, std::memory_order_relaxed);
                                return;
                            }
                        }

                        if (this->broke_timepoint.has_value())
                        {
                            std::chrono::nanoseconds lapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - this->broke_timepoint.value());

                            if (lapsed >= topic->master_config.abs_timeout_dur)
                            {
                                this->is_alive_flag->exchange(false, std::memory_order_relaxed);
                                return;
                            }
                        }

                        for (const auto& [who, when]: topic->who_when_map)
                        {
                            std::chrono::nanoseconds lapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - when);

                            if (lapsed >= topic->master_config.connection_timeout_dur)
                            {
                                this->is_alive_flag->exchange(false, std::memory_order_relaxed);
                                return;
                            }
                        }
                    }
            };
    };

    class ThreadSafeMasterConnection: private MasterConnection
    {
        private:

            std::unique_ptr<fair_mutex::fair_atomic_flag> mtx;

        public:

            ThreadSafeMasterConnection(const MasterConfiguration& config,
                                       std::shared_ptr<ConnectionControllerInterface> controller = ConnectionControllerSingleton::get()): MasterConnection(config, controller),
                                                                                                                                          mtx(fair_mutex::make_unique_fair_atomic_flag()){}

            auto get_slave_configuration() -> SlaveConfiguration
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                return MasterConnection::get_slave_configuration();
            }

            void close() noexcept
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                MasterConnection::close();
            }

            auto is_alive() -> bool
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                return MasterConnection::is_alive();
            }
    };

    class SlaveConnection: public virtual ConnectionInterface
    {
        private:

            SlaveConfiguration slave_config;
            std::shared_ptr<std::atomic<bool>> is_alive_flag;
            std::shared_ptr<void> cron_obj;
            bool is_closed;

        public:

            static inline const uint64_t MIN_PING_RETRY_COUNT                       = 0u;
            static inline const uint64_t MAX_PING_RETRY_COUNT                       = std::numeric_limits<uint64_t>::max();
            static inline const std::chrono::nanoseconds MIN_PING_RETRY_BREAK_DUR   = std::chrono::nanoseconds(0);
            static inline const std::chrono::nanoseconds MAX_PING_RETRY_BREAK_DUR   = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::hours(1));
            static inline const std::chrono::nanoseconds MIN_CONNECTION_TIMEOUT_DUR = std::chrono::nanoseconds(0);
            static inline const std::chrono::nanoseconds MAX_CONNECTION_TIMEOUT_DUR = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::hours(1));
            static inline const std::chrono::nanoseconds MIN_ABS_TIMEOUT_DUR        = std::chrono::nanoseconds(0);
            static inline const std::chrono::nanoseconds MAX_ABS_TIMEOUT_DUR        = std::chrono::nanoseconds::max();
            static inline const std::chrono::nanoseconds CRON_JOB_DURATION          = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::milliseconds(100));

            SlaveConnection(const SlaveConfiguration& config)
            {
                this->check_and_throw_configuration(config);

                this->slave_config      = config;
                this->is_alive_flag     = std::make_shared<std::atomic<bool>>(true);
                auto resolutor_obj      = std::make_shared<InternalResolutor>(slave_config,
                                                                              std::make_unique<MasterPinger>(),
                                                                              this->is_alive_flag);

                this->cron_obj          = cron_subsystem::register_periodic_cronjob(resolutor_obj, CRON_JOB_DURATION);
                this->is_closed         = false;
            }

            SlaveConnection(const SlaveConnection&) = delete;
            SlaveConnection& operator =(const SlaveConnection&) = delete;

            ~SlaveConnection() noexcept
            {
                this->close();
            }

            void close() noexcept
            {
                if (std::exchange(this->is_closed, true))
                {
                    return;
                }

                this->cron_obj = nullptr;
            }

            auto is_alive() -> bool
            {
                return !this->is_closed && this->is_alive_flag->load(std::memory_order_relaxed);
            }

        private:

            void check_and_throw_configuration(const SlaveConfiguration& config)
            {
                if (std::clamp(config.master_payload.ping_retry_count, MIN_PING_RETRY_COUNT, MAX_PING_RETRY_COUNT) != config.master_payload.ping_retry_count)
                {
                    throw std::invalid_argument("bad ping retry count, value out of range");
                }

                if (std::clamp(config.master_payload.ping_retry_break_dur, MIN_PING_RETRY_BREAK_DUR, MAX_PING_RETRY_BREAK_DUR) != config.master_payload.ping_retry_break_dur)
                {
                    throw std::invalid_argument("bad ping retry break duration, duration out of range");
                }

                if (std::clamp(config.connection_timeout_dur, MIN_CONNECTION_TIMEOUT_DUR, MAX_CONNECTION_TIMEOUT_DUR) != config.connection_timeout_dur)
                {
                    throw std::invalid_argument("bad connection timeout duration, duration out of range");
                }

                if (std::clamp(config.abs_timeout_dur, MIN_ABS_TIMEOUT_DUR, MAX_ABS_TIMEOUT_DUR) != config.abs_timeout_dur)
                {
                    throw std::invalid_argument("bad absolute timeout duration, duration out of range");
                }
            }

            class InternalResolutor: public virtual cron_subsystem::UpdatableInterface
            {
                private:

                    struct PingPromise
                    {
                        std::shared_ptr<dg_sock::network_rest_frame::client::Promise<stdx::fancy_void>> base_promise;
                        std::chrono::time_point<std::chrono::steady_clock> since;
                    };

                    SlaveConfiguration slave_config;
                    std::unique_ptr<ConnectionPingerInterface> connection_pinger;
                    std::shared_ptr<std::atomic<bool>> is_alive_flag;
                    std::optional<std::chrono::time_point<std::chrono::steady_clock>> last_updated;
                    std::optional<std::chrono::time_point<std::chrono::steady_clock>> since;
                    std::optional<PingPromise> ping_promise;

                public:

                    InternalResolutor(SlaveConfiguration slave_config,
                                      std::unique_ptr<ConnectionPingerInterface> connection_pinger,
                                      std::shared_ptr<std::atomic<bool>> is_alive_flag) noexcept: slave_config(std::move(slave_config)),
                                                                                                  connection_pinger(std::move(connection_pinger)),
                                                                                                  is_alive_flag(std::move(is_alive_flag)),
                                                                                                  last_updated(std::nullopt),
                                                                                                  since(std::nullopt),
                                                                                                  ping_promise(std::nullopt){}

                    void update()
                    {
                        if (!this->is_alive_flag->load(std::memory_order_relaxed))
                        {
                            return;
                        }

                        if (!this->last_updated.has_value())
                        {
                            this->last_updated  = std::chrono::steady_clock::now();
                            this->since         = this->last_updated;
                        }

                        if (this->ping_promise.has_value())
                        {
                            if (this->ping_promise->base_promise->is_completed())
                            {
                                try
                                {
                                    this->ping_promise->base_promise->wait();
                                    this->ping_promise = std::nullopt;
                                    this->last_updated = std::chrono::steady_clock::now();

                                    return;
                                }
                                catch (connection_not_found_error& e)
                                {
                                    this->is_alive_flag->exchange(false, std::memory_order_relaxed);
                                    this->ping_promise = std::nullopt;

                                    return;
                                }
                                catch (std::exception& e)
                                {
                                    this->is_alive_flag->exchange(false, std::memory_order_relaxed);
                                    this->ping_promise = std::nullopt;

                                    logging_subsystem::noexcept_log(logging_subsystem::LogFactory{}.topic("connectivity_subsystem")
                                                                                                   .topic("SlaveConnection")
                                                                                                   .topic("Ping response resolution encountered an error")
                                                                                                   .message(std::current_exception())
                                                                                                   .error()
                                                                                                   .get());

                                    return;
                                }
                            }

                            std::chrono::nanoseconds lapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - this->ping_promise->since);

                            if (lapsed < this->slave_config.connection_timeout_dur)
                            {
                                this->last_updated = std::chrono::steady_clock::now();
                                return;
                            }

                            this->is_alive_flag->exchange(false, std::memory_order_relaxed);
                            this->ping_promise = std::nullopt;

                            return;
                        }

                        {
                            std::chrono::nanoseconds lapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - this->last_updated.value());

                            if (lapsed >= this->slave_config.connection_timeout_dur)
                            {
                                this->is_alive_flag->exchange(false, std::memory_order_relaxed);
                                return;
                            }
                        }

                        {
                            std::chrono::nanoseconds lapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - this->since.value());

                            if (lapsed >= this->slave_config.abs_timeout_dur)
                            {
                                this->is_alive_flag->exchange(false, std::memory_order_relaxed);
                                return;
                            }
                        }

                        try
                        {
                            this->ping_promise = PingPromise
                            {
                                .base_promise   = this->connection_pinger->ping(this->slave_config.master_payload),
                                .since          = std::chrono::steady_clock::now()
                            };
                        }
                        catch (std::exception& e)
                        {
                            this->is_alive_flag->exchange(false, std::memory_order_relaxed);

                            logging_subsystem::noexcept_log(logging_subsystem::LogFactory{}.topic("connectivity_subsystem")
                                                                                           .topic("SlaveConnection")
                                                                                           .topic("Daemon pinger encountered an error")
                                                                                           .message(std::current_exception())
                                                                                           .error()
                                                                                           .get());

                            return;
                        }
                    }
            };
    };

    class ThreadSafeSlaveConnection: private SlaveConnection
    {
        private:

            std::unique_ptr<fair_mutex::fair_atomic_flag> mtx;

        public:

            ThreadSafeSlaveConnection(const SlaveConfiguration& config): SlaveConnection(config),
                                                                         mtx(fair_mutex::make_unique_fair_atomic_flag()){}

            void close() noexcept
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                SlaveConnection::close();
            }

            auto is_alive() -> bool
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                return SlaveConnection::is_alive();
            }
    };
}

#endif