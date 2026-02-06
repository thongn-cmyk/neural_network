#ifndef __CONNECTIVITY_SUBSYSTEM_H__
#define __CONNECTIVITY_SUBSYSTEM_H__

#include <stdint.h>
#include <stdlib.h>
#include <unordered_map>
#include <vector>
#include <variant>
#include "stdx.h"

namespace connectivity_subsystem
{
    //connectivity subsystem is an ownership model to hint of a resource cleanup of a remote address

    //in the normal case, is_alive should not affect the entire the process, and follows the sequence of master requests slave_open, master requests slave_solve, and master requests slave_close
    //in the not normal case, the hint should close the remote resource in the worst time of master_alive_time + connectivity_timeout_dur
    //so it's a guarantee and a hint of lifetime

    struct connection_not_found_error: std::invalid_argument
    {
        conneciton_not_found_error(): std::invalid_argument("specified connection_id does not link to a registered entity"){}
    };

    struct ping_room_overflow_error: std::runtime_error
    {
        ping_room_overflow_error(): std::runtime_error("max ping population reached for the specified connection entity"){}
    };

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
        internal_rest_controller::Url url;
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
        connectivity_subsystem::exception_t result;
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
        std::unordered_map<std::string, std::chrono::time_point<std::chrono::system_clock>> who_when_map;

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
            virtual void ping(const MasterPayload& payload) = 0;
    };

    struct RestConfiguration
    {
        static auto get_ping_resolver_url() -> internal_rest_controller::Url
        {
            return {};
        }
    };

    class ConnectionController: public virtual ConnectionControllerInterface
    {
        private:

            std::unordered_map<uint64_t, ConnectionTopic> id_topic_map;
            uint64_t id_counter;
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

                    auto [_, new_map_ptr]   = map_ptr->second.who_when_map.insert({who, {}});
                    map_ptr_2               = new_map_ptr;
                }

                map_ptr_2->second = std::system_clock::now();
            }

            void close_connection(uint64_t connection_id) noexcept
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                this->id_topic_map.erase(connection_id);
            }
    };

    class PingResolver: public virtual internal_rest_controller::ResolvableInterface
    {
        private:

            std::shared_ptr<ConnectionControllerInterface> connection_controller;

        public:

            PingResolver(std::shared_ptr<ConnectionControllerInterface> connection_controller) noexcept: connection_controller(std::move(connection_controller)){}

            auto resolve(const internal_rest_controller::Request& request) -> internal_rest_controller::Response
            {
                if (request.serialization_kind != dg::network_compact_serializer::get_dgstd_serialization_identifier())
                {
                    throw std::invalid_argument("unexpected request type, bad serialization method");
                }

                PingRequest semantic_request = dg::network_compact_seerializer::dgstd_deserialize<PingRequest>(request.content);
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
                        .err_verbal_description = exception_to_verbal_string(std::current_exception())
                    };
                }

                return internal_rest_controller::Response
                {
                    .content = dg::network_compact_serializer::dgstd_serialize<std::string>(semantic_response),
                    .serialization_kind = dg::network_compact_serializer::get_dgstd_serialization_identifier()
                };
            }
    };

    class MasterPinger: public virtual ConnectionPingerInterface
    {
        private:

            internal_rest_controller::RequestClient client;
            std::string identifier;

        public:

            MasterPinger(): client(),
                            identifier(stdx::get_random_identifier()){}

            void ping(const MasterPayload& payload)
            {
                PingRequest ping_request
                {
                    .topic_id   = payload.topic_id,
                    .pinger     = this->identifier
                };

                std::string ping_payload = dg::network_compact_serializer::dgstd_serialize<std::string>(ping_request);

                internal_rest_controller::ClientRequest request   = internal_rest_controller::RequestFactory{}.url(payload.url)
                                                                                                              .payload(ping_payload)
                                                                                                              .serialization_method(dg::network_compact_serializer::get_dgstd_serialization_identifier())
                                                                                                              .get();

                internal_rest_controller::ClientResponse response = this->client.set_retry_policy(internal_rest_controller::UniformRetryPolicy{}.set_break_duration(payload.ping_retry_break_dur)
                                                                                                                                                .set_retry_count(payload.ping_retry_count)
                                                                                                                                                .get())
                                                                                .set_request(request)
                                                                                .get();

                if (response.serialization_kind != dg::network_compact_serializer::get_dgstd_serialization_identifier())
                {
                    throw std::runtime_error("unexpected serialization format");
                }

                PingResponse semantic_response = dg::network_compact_serializer::dgstd_deserialize<PingResponse>(response.content);

                if (semantic_response.result != SUCCESS)
                {
                    if (semantic_response.result == CONNECTION_NOT_FOUND_ERROR_CODE)
                    {
                        throw connection_not_found_error();
                    }
                    else
                    {
                        std::runtime_error(semantic_response.err_verbal_description);
                    }
                }
            }
    };

    struct Signature{};

    using ConnectionControllerSingleton = stdx::shared_ptr_singleton_container<ConnectionController, Signature>;

    void init()
    {
        ConnectionControllerSingleton::initialize();
        internal_rest_controller::hook(RestConfiguration::get_ping_resolver_url(), std::make_unique<PingResolver>(ConnectionControllerSingeton::get()));
    }

    void deinit() noexcept
    {
        internal_rest_controller::unhook(RestConfiguration::get_ping_resolver_url());
        ConnectionControllerSingleton::deinitialize();
    }

    class MasterConnection: public virtual ConnectionInterface
    {
        private:

            MasterConfiguration master_config;
            std::shared_ptr<ConnectionControllerInterface> controller;
            uint64_t connection_id;
            std::shared_ptr<std::atomic<bool>> is_alive_flag;
            std::shared_ptr<void> cron_obj;
            bool is_closed;

        public:

            static inline const std::chrono::nanoseconds MIN_CONNECTION_TIMEOUT_DUR = {};
            static inline const std::chrono::nanoseconds MAX_CONNECTION_TIMEOUT_DUR = {};
            static inline const std::chrono::nanoseconds MIN_CONNECTION_BROKE_DUR   = {};
            static inline const std::chrono::nanoseconds MAX_CONENCTION_BROKE_DUR   = {};
            static inline const std::chrono::nanoseconds MIN_ABS_TIMEOUT_DUR        = {};
            static inline const std::chrono::nanoseconds MAX_ABS_TIMEOUT_DUR        = {};
            static inline const std::chrono::nanoseconds CRON_JOB_DURATION          = {};
            static inline const uint64_t MIN_PING_RETRY_COUNT                       = {};
            static inline const uint64_t MAX_PING_RETRY_COUNT                       = {};
            static inline const std::chrono::nanoseconds MIN_PING_RETRY_BREAK_DUR   = {};
            static inline const std::chrono::nanoseconds MAX_PING_RETRY_MAX_DUR     = {};
            static inline const uint64_t MIN_SLAVE_COUNT                            = {};
            static inline const uint64_t MAX_SLAVE_COUNT                            = {}; 

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
                    this->is_alive_flag = std::make_shared<std::atomic<bool>>(true);
                    auto resolutor_obj  = std::make_shared<InternalResolutor>(this->controller,
                                                                              this->connection_id,
                                                                              this->is_alive_flag,
                                                                              std::nullopt,
                                                                              std::nullopt);

                    this->cron_obj      = cron_subsystem::register_periodic_cronjob(resolutor_obj, CRON_JOB_DURATION);
                    this->is_closed     = false;

                }
                catch (...)
                {
                    controller->close_connection(this->connection_id);
                    throw;
                }
            }

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

                if (std::clamp(config.ping_retry_break_dur, MIN_PING_RETRY_BREAK_DUR, MAX_PING_RETRY_MAX_DUR) != config.ping_retry_break_dur)
                {
                    throw std::invalid_argument("bad ping retry break duration, duration out of range");
                }

                if (std::clamp(config.slave_count, MIN_SLAVE_COUNT, MAX_SLAVE_COUNT) != config.slave_count)
                {
                    throw std::invalid_argument("bad slave count, value out of range");
                }
            }

            auto get_master_payload_url() -> internal_rest_controller::Url
            {
                return RestConfiguration::get_ping_resolver_url();
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
                    std::optional<std::chrono::time_point<std::chrono::system_clock>> first_timepoint;
                    std::optional<std::chrono::time_point<std::chrono::system_clock>> broke_timepoint;

                public:

                    InternalResolutor(std::shared_ptr<ConnectionControllerInterface> controller,
                                      uint64_t connection_id,
                                      std::shared_ptr<std::atomic<bool>> is_alive_flag,
                                      std::optional<std::chrono::time_point<std::chrono::system_clock>> first_timepoint,
                                      std::optional<std::chrono::time_point<std::chrono::system_clock>> broke_timepoint) noexcept: controller(std::move(controller)),
                                                                                                                                   connection_id(connection_id),
                                                                                                                                   is_alive_flag(std::move(is_alive_flag)),
                                                                                                                                   first_timepoint(first_timepoint),
                                                                                                                                   broke_timepoint(broke_timepoint){}

                    void update()
                    {
                        if (!this->is_alive_flag->load(std::memory_order_relaxed))
                        {
                            return;
                        }

                        if (!this->first_timepoint.has_value())
                        {
                            this->first_timepoint = std::chrono::system_clock::now();
                            return;
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
                                this->broke_timepoint = std::chrono::system_clock::now();
                            }
                        }

                        if (!this->broke_timepoint.has_value())
                        {
                            std::chrono::nanoseconds lapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now() - this->first_timepoint.value());

                            if (lapsed >= topic->master_config.connection_broke_dur)
                            {
                                this->is_alive_flag->exchange(false, std::memory_order_relaxed);
                                return;
                            }
                        }

                        if (this->broke_timepoint.has_value())
                        {
                            std::chrono::nanoseconds lapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now() - this->broke_timepoint.value());

                            if (lapsed >= topic->master_config.abs_timeout_dur)
                            {
                                this->is_alive_flag->exchange(false, std::memory_order_relaxed);
                                return;
                            }
                        }

                        for (const auto& [who, when]: topic->who_when_map)
                        {
                            std::chrono::nanoseconds lapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now() - when);

                            if (lapsed >= topic->master_config.connection_timeout_dur)
                            {
                                this->is_alive_flag->exchange(false, std::memory_order_relaxed);
                                return;
                            }
                        }
                    }
            };
    };

    class SlaveConnection: public virtual ConnectionInterface
    {
        private:

            SlaveConfiguration slave_config;
            std::shared_ptr<ConnectionPingerInterface> connection_pinger;
            std::shared_ptr<std::atomic<bool>> is_alive_flag;
            std::shared_ptr<void> cron_obj;
            bool is_closed;

        public:

            static inline const uint64_t MIN_PING_RETRY_COUNT                       = {};
            static inline const uint64_t MAX_PING_RETRY_COUNT                       = {};
            static inline const std::chrono::nanoseconds MIN_PING_RETRY_BREAK_DUR   = {};
            static inline const std::chrono::nanoseconds MAX_PING_RETRY_BREAK_DUR   = {};
            static inline const std::chrono::nanoseconds MIN_CONNECTION_TIMEOUT_DUR = {};
            static inline const std::chrono::nanoseconds MAX_CONNECTION_TIMEOUT_DUR = {};
            static inline const std::chrono::nanoseconds MIN_ABS_TIMEOUT_DUR        = {};
            static inline const std::chrono::nanoseconds MAX_ABS_TIMEOUT_DUR        = {};

            SlaveConnection(const SlaveConfiguration& config)
            {
                this->check_and_throw_configuration(config);

                this->slave_config      = config;
                this->connection_pinger = std::make_unique<MasterPinger>();
                this->is_alive_flag     = std::make_shared<std::atomic<bool>>(true);
                auto resolutor_obj      = std::make_shared<InternalResolutor>(slave_config,
                                                                              this->connection_pinger,
                                                                              this->is_alive_flag,
                                                                              std::nullopt,
                                                                              std::nullopt);

                this->cron_obj          = cron_subsystem::register_periodic_cronjob(resolutor_obj, CRON_JOB_DURATION);
                this->is_closed         = false;
            }

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

                if (std::clamp(config.connection_timeout_dur, MIN_CONNECTION_TIMEOUT_DUR, MAX_CONNECTION_TIMEOUT_DUR != config.connection_timeout_dur))
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

                    SlaveConfiguration slave_config;
                    std::shared_ptr<ConnectionPingerInterface> connection_pinger;
                    std::shared_ptr<std::atomic<bool>> is_alive_flag;
                    std::optional<std::chrono::time_point<std::chrono::system_clock>> last_updated;
                    std::optional<std::chrono::time_point<std::chrono::system_clock>> since;

                public:

                    InternalResolutor(SlaveConfiguration slave_config,
                                      std::shared_ptr<ConnectionPingerInterface> connection_pinger,
                                      std::shared_ptr<std::atomic<bool>> is_alive_flag,
                                      std::optional<std::chrono::time_point<std::chrono::system_clock>> last_updated,
                                      std::optional<std::chrono::time_point<std::chrono::system_clock>> since) noexcept: slave_config(std::move(slave_config)),
                                                                                                                         connection_pinger(std::move(connection_pinger)),
                                                                                                                         is_alive_flag(std::move(is_alive_flag)),
                                                                                                                         last_updated(last_updated),
                                                                                                                         since(since){}

                    void update()
                    {
                        if (!this->is_alive_flag->load(std::memory_order_relaxed))
                        {
                            return;
                        }

                        if (!this->last_updated.has_value())
                        {
                            this->last_updated  = std::chrono::system_clock::now();
                            this->since         = this->last_updated;

                            return;
                        }

                        {
                            std::chrono::nanoseconds lapsed = std::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now() - this->last_updated.value());

                            if (lapsed >= this->slave_config.connection_timeout_dur)
                            {
                                this->is_alive_flag->exchange(false, std::memory_order_relaxed);
                                return;
                            }
                        }

                        {
                            std::chrono::nanoseconds lapsed = std::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now() - this->since.value());

                            if (lapsed >= this->slave_config.abs_timeout_dur)
                            {
                                this->is_alive_flag->exchange(false, std::memory_order_relaxed);
                                return;
                            }
                        }

                        try
                        {
                            this->connection_pinger->ping(this->slave_config.master_payload);
                        }
                        catch (const connection_not_found_error& e)
                        {
                            this->is_alive_flag->exchange(false, std::memory_order_relaxed);
                            return;
                        }
                        catch (const std::exception& e)
                        {
                            this->is_alive_flag->exchange(false, std::memory_order_relaxed);

                            logging_subsystem::log(logging_subsystem::LogFactory{}.topic("connectivity_subsystem")
                                                                                  .topic("SlaveConnection")
                                                                                  .topic("Daemon pinger encountered an error")
                                                                                  .message(std::current_exception())
                                                                                  .error()
                                                                                  .get());

                            return;
                        }

                        this->last_updated = std::chrono::system_clock::now();
                    }
            };
    };
}

#endif