#ifndef __DEVIATION_PROJECTION_INGESTION_AID_SERVER_H__
#define __DEVIATION_PROJECTION_INGESTION_AID_SERVER_H__

#include <stdint.h>
#include <stdlib.h>
#include <main_broker/main_service.h>
#include <concurrency_base/concurrency_base.h>

namespace deviation_projection_ingestion_aid_server
{
    struct ServerSink
    {
        dg_sock::network_rest_frame::model::Url url;
        uint64_t client_id;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(url, client_id);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(url, client_id);
        }
    };

    class ClientBox
    {
        private:

            struct Resource
            {
                std::optional<data_loader::Configuration> data_loader_config;
                std::optional<std::vector<ServerSink>> server_sink_vec;
            };

            struct Deliverable
            {
                std::exception_ptr exception;
            };

            Resource resource;
            std::shared_ptr<std::thread> task_thr;
            bool was_run_broke;
            bool was_wait_broke;
            bool was_explicitly_destroyed;
            std::shared_ptr<std::atomic<bool>> is_completed_var;
            std::shared_ptr<std::atomic<bool>> interruption_pill;
            std::shared_ptr<Deliverable> deliverable;

        public:

            void set_data_source()
            {

            }
            
            void set_server_sink()
            {

            }

            void run()
            {

            }

            auto is_completed()
            {

            }

            void interrupt()
            {

            }

            auto wait()
            {

            }

            void close() noexcept
            {

            }
    };

    class ConnectionBoundClientBox
    {
        private:

            std::unique_ptr<connectivity_subsystem::ConnectionInterface> connection;
            std::unique_ptr<ClientBox> base;
            std::unique_ptr<std::atomic<bool>> was_explicitly_destroyed;
            std::unique_ptr<fair_mutex::fair_atomic_flag> mtx;

        public:

            ConnectionBoundClientBox(const connectivity_subsystem::SlaveConfiguration& connection_config): connection(std::make_unique<connectivity_subsystem::ThreadSafeSlaveConnection>(connection_config)),
                                                                                                           base(std::make_unique<ClientBox>()),
                                                                                                           was_explicitly_destroyed(std::make_unique<std::atomic<bool>>(false)),
                                                                                                           mtx(fair_mutex::make_unique_fair_atomic_flag()){}

            void set_data_source(const data_loader::Configuration& config)
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed->load(std::memory_order_relaxed))
                {
                    throw destroyed_client_box_error{};
                }

                this->base->add_data_source(config);
            }

            void set_server_sink(const std::vector<ServerSink>& server_sink_vec)
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed->load(std::memory_order_relaxed))
                {
                    throw destroyed_client_box_error{};
                }

                this->base->set_server_sink(server_sink_vec);
            }

            void run()
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed->load(std::memory_order_relaxed))
                {
                    throw destroyed_client_box_error{};
                }

                this->base->run();
            }

            auto is_completed()
            {
                return this->base->is_completed();
            }

            void interrupt()
            {
                this->base->interrupt();
            }

            void wait()
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                this->base->wait();
            }

            void close() noexcept
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed.exchange(true, std::memory_order_relaxed))
                {
                    return;
                }

                this->connection->close();
                this->base->close();
            }

            auto is_alive() -> bool
            {
                return !this->was_explicitly_destroyed->load(std::memory_order_relaxed) && this->connection->is_alive();
            }
    };

    class ClientBoxManager
    {
        private:

            std::unique_ptr<connection_based_manager::ManagerInterface> base;

        public:

            ClientBoxManager(): base(std::make_unique<connection_based_manager::ClientManager>()){}

            auto open_client_box(const connectivity_subsystem::SlaveConfiguration& connection_config) -> uint64_t
            {
                return this->base->add(std::make_shared<ConnectionBoundClientBox>(connection_config));
            }

            auto get_client_box(uint64_t client_box_id) -> std::shared_ptr<ConnectionBoundClientBox>
            {
                return std::dynamic_pointer_cast<ConnectionBoundClientBox>(this->base->get(client_box_id));
            }

            void close_client_box(uint64_t client_box_id)
            {
                this->base->close(client_box_id);
            }
    };

    struct GetVersionRequest
    {
        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            (void) reflector;
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            (void) reflector;
        }
    };

    struct GetVersionResponse
    {
        std::expected<std::string, deviation_projection_ingestion_aid_server::local_exception_t> response;
        std::string err_verbal_description;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(response, err_verbal_description);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(response, err_verbal_description);
        }
    };

    struct OpenClientRequest
    {
        connectivity_subsystem::SlaveConfiguration connection_config;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(connection_config);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(connection_config);
        };
    };

    struct OpenClientResponse
    {
        std::expected<uint64_t, deviation_projection_ingestion_aid_server::local_exception_t> result;
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

    struct CloseClientRequest
    {
        uint64_t client_box_id;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(client_box_id);
        }
    };

    struct CloseClientResponse
    {
        deviation_projection_ingestion_aid_server::local_exception_t result;
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

    struct RunRequest
    {
        uint64_t client_box_id;
        std::vector<std::pair<uint64_t, dg_sock::network_rest_frame::model::Url>> server_box_vec;
        data_loader::Configuration config;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(client_box_id, server_box_vec, config);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(client_box_id, server_box_vec, config);
        }
    };

    struct RunResponse
    {
        deviation_projection_ingestion_aid_server::local_exception_t result;
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

    struct InterruptRequest
    {
        uint64_t client_box_id;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(client_box_id);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(client_box_id);
        }
    };

    struct InterruptResponse
    {
        deviation_projection_ingestion_aid_server::local_exception_t result;
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

    struct IsCompletedRequest
    {
        uint64_t client_box_id;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(client_box_id);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(client_box_id);
        }
    };

    struct IsCompletedResponse
    {
        std::expected<bool, deviation_projection_ingestion_aid_server::local_exception_t> result;
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

    struct GetResultRequest
    {
        uint64_t client_box_id;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(client_box_id);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(client_box_id);
        }
    };

    struct GetResultResponse
    {
        deviation_projection_ingestion_aid_server::local_exception_t result;
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

    class GetVersionResolver: public virtual dg_sock::network_rest_frame::server::OneRequestHandlerInterface
    {
        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "deviation_projection_ingestion_aid_server/get_version";

            auto handle(const dg_sock::network_rest_frame::model::Request& request) -> dg_sock::network_rest_frame::model::Response
            {
                if (std::string_view(request.payload_serialization_format) != dg::network_compact_serializer::get_dgstd_serialization_identifier())
                {
                    throw std::invalid_argument("unexpected request, bad serialization method");
                }

                GetVersionRequest semantic_request      = dg::network_compact_serializer::dgstd_deserialize<GetVersionRequest>(request.payload);
                GetVersionResponse semantic_response    =
                {
                    .response = std::string(DEVIATION_PROJECTION_INGESTION_AID_SERVER_VERSION_CONTROL),
                    .err_verbal_description = {}
                };

                return dg_sock::network_rest_frame::model::Response
                {
                    .response = dg::network_compact_serializer::dgstd_serialize<dg_sock::string>(semantic_response),
                    .response_serialization_format = dg_sock::string(dg::network_compact_serializer::get_dgstd_serialization_identifier()),
                    .err_code = dg_sock::network_exception::SUCCESS
                };
            }
    };

    class OpenClientResolver: public virtual dg_sock::network_rest_frame::server::OneRequestHandlerInterface
    {
        private:

            std::shared_ptr<ClientBoxManager> client_box_manager;

        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "deviation_projection_ingestion_aid_server/open_client";

            OpenClientResolver(std::shared_ptr<ClientBoxManager> client_box_manager) noexcept: client_box_manager(std::move(client_box_manager)){}

            auto handle(const dg_sock::network_rest_frame::model::Request& request) -> dg_sock::network_rest_frame::model::Response
            {
                if (std::string_view(request.payload_serialization_format) != dg::network_compact_serializer::get_dgstd_serialization_identifier())
                {
                    throw std::invalid_argument("unexpected request, bad serialization method");
                }

                OpenClientRequest semantic_request      = dg::network_compact_serializer::dgstd_deserialize<OpenClientRequest>(request.payload);
                OpenClientResponse semantic_response;

                try
                {
                    uint64_t client_box_id = this->client_box_manager->open_client_box(semantic_request.connection_config);

                    semantic_response = OpenClientResponse
                    {
                        .result = client_box_id,
                        .err_verbal_description = ""
                    };
                }
                catch (...)
                {
                    semantic_response = OpenClientResponse
                    {
                        .result = deviation_projection_ingestion_aid_server::to_local_exception_error_code(std::current_exception()),
                        .err_verbal_description = deviation_projection_ingestion_aid_server::verbose_error_code(deviation_projection_ingestion_aid_server::to_local_exception_error_code(std::current_exception()))
                    };
                }

                return dg_sock::network_rest_frame::model::Response
                {
                    .response = dg::network_compact_serializer::dgstd_serialize<dg_sock::string>(semantic_response),
                    .response_serialization_format = dg_sock::string(dg::network_compact_serializer::get_dgstd_serialization_identifier()),
                    .err_code = dg_sock::network_exception::SUCCESS
                };
            }
    };
    
}

#endif