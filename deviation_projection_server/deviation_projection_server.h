#ifndef __DG_DEVIATION_PROJECTION_SERVER_H__
#define __DG_DEVIATION_PROJECTION_SERVER_H__

#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <unordered_map>
#include <general_definition/float_def.h>
#include <memory>
#include <internal_rest/network_rest_frame.h>
#include <deviation_projector/generic_matrix_deviation_calculator_interface.h>
#include <deviation_projector/generic_resource.h>
#include <connectivity_subsystem/connectivity_subsystem.h>
#include <connection_based_manager/connection_based_manager.h>
#include <string>
#include <serializer/compact_serializer.h>
#include <chrono>

namespace deviation_projection_server
{
    using namespace float_def;

    using local_exception_t = uint8_t;

    static inline constexpr local_exception_t SUCCESS                                       = 0u;
    static inline constexpr local_exception_t INVALID_ARGUMENT_ERROR_CODE                   = 1u;
    static inline constexpr local_exception_t RUNTIME_ERROR_CODE                            = 2u;
    static inline constexpr local_exception_t CLIENT_NOT_FOUND_ERROR_CODE                   = 3u;

    static inline constexpr std::string_view DEVIATION_PROJECTION_SERVER_VERSION_CONTROL    = "";

    class ClientBox
    {
        private:

            std::vector<std::pair<std::shared_ptr<std::string>, std::shared_ptr<std::string>>> training_data;
            std::vector<std::unique_ptr<deviation_projector::GenericMatrixDeviationCalculatorInterface>> deviation_calculator_vec;

        public:

            ClientBox(): training_data(),
                         deviation_calculator_vec(){}

            void add_training_data(const std::shared_ptr<std::string>& inp, const std::shared_ptr<std::string>& out)
            {
                if (inp == nullptr)
                {
                    throw std::invalid_argument("bad input matrix, null");
                }

                if (out == nullptr)
                {
                    throw std::invalid_argument("bad output matrix, null");
                }

                this->training_data.push_back({inp, out});
            }

            void clear_training_data() noexcept
            {
                this->training_data.clear();
            }

            void set_matrix_resource(const std::vector<deviation_projector::GenericMatrixDeviationCalculatorResource>& matrix_resource_vec)
            {
                this->deviation_calculator_vec.clear();

                for (const auto& e: matrix_resource_vec)
                {
                    this->deviation_calculator_vec.push_back(deviation_projector::GenericMatrixDeviationCalculatorResourceLoader{}.load(e));
                }
            }

            auto get() -> std::vector<mdc_float_t>
            {
                std::vector<mdc_float_t> rs_vec{};

                for (const auto& deviation_calculator: this->deviation_calculator_vec)
                {
                    rs_vec.push_back(deviation_calculator->get_deviation(this->training_data));
                }

                return rs_vec;
            }
    };

    class ConnectionBoundClientBox: public virtual connection_based_manager::HealthcheckableInterface
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

            void add_training_data(const std::shared_ptr<std::string>& inp, const std::shared_ptr<std::string>& out)
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed->load(std::memory_order_relaxed))
                {
                    throw std::runtime_error("invalid operation, closed client box");
                }

                this->base->add_training_data(inp, out);
            }

            void clear_training_data()
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed->load(std::memory_order_relaxed))
                {
                    throw std::runtime_error("invalid operation, closed client box");
                }

                this->base->clear_training_data();
            }

            void set_matrix_resource(const std::vector<deviation_projector::GenericMatrixDeviationCalculatorResource>& matrix_resource_vec)
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed->load(std::memory_order_relaxed))
                {
                    throw std::runtime_error("invalid operation, closed client box");
                }

                this->base->get_matrix_resource(matrix_resource_vec);
            }

            auto get() -> std::vector<mdc_float_t>
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed->load(std::memory_order_relaxed))
                {
                    throw std::runtime_error("invalid operation, closed client box");
                }

                return this->base->get();
            }

            void close() noexcept
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed->exchange(true, std::memory_order_relaxed))
                {
                    return;
                }

                this->connection->close();
                this->base = nullptr;
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
                std::shared_ptr<ConnectionBoundClientBox> obj = std::make_shared<ConnectionBoundClientBox>(connection_config);

                return this->base->add(obj);
            }

            auto get_client_box(uint64_t client_box_id) -> std::shared_ptr<ConnectionBoundClientBox>
            {
                return std::dynamic_pointer_cast<ConnectionBoundClientBox>(this->base->get(client_box_id));
            }

            void close_client_box(uint64_t client_box_id) noexcept
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
        std::expected<std::string, deviation_projection_server::local_exception_t> response;
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
        }
    };

    struct OpenClientResponse
    {
        std::expected<uint64_t, deviation_projection_server::local_exception_t> result;
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
        uint64_t client_id;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(client_id);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(client_id);
        }
    };

    struct CloseClientResponse
    {
        deviation_projection_server::local_exception_t result;
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

    struct AddTrainingDataRequest
    {
        uint64_t client_id;
        std::string input_matrix;
        std::string output_matrix;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(client_id, input_matrix, output_matrix);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(client_id, input_matrix, output_matrix);
        }
    };

    struct AddTrainingDataResponse
    {
        deviation_projection_server::local_exception_t result;
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

    struct ClearTrainingDataRequest
    {
        uint64_t client_id;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(client_id);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(client_id);
        }
    };

    struct ClearTrainingDataResponse
    {
        deviation_projection_server::local_exception_t result;
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

    struct SetMatrixResourceRequest
    {
        uint64_t client_id;
        std::vector<deviation_projector::GenericMatrixDeviationCalculatorResource> matrix_resource_vec;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(client_id, matrix_resource_vec);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(client_id, matrix_resource_vec);
        }
    };

    struct SetMatrixResourceResponse
    {
        deviation_projection_server::local_exception_t result;
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

    struct GetDeviationRequest
    {
        uint64_t client_id;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(client_id);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(client_id);
        }
    };

    struct GetDeviationResponse
    {
        std::expected<std::vector<mdc_float_t>, deviation_projection_server::local_exception_t> result;
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

            static inline constexpr std::string_view RESOLVABLE_PATH = "deviation_projection/get_version";

            auto handle(const dg_sock::network_rest_frame::model::Request& request) -> dg_sock::network_rest_frame::model::Response
            {
                if (std::string_view(request.payload_serialization_format) != dg::network_compact_serializer::get_dgstd_serialization_identifier())
                {
                    throw std::invalid_argument("unexpected request, bad serialization method");
                }

                GetVersionRequest semantic_request      = dg::network_compact_serializer::dgstd_deserialize<GetVersionRequest>(request.payload);
                GetVersionResponse semantic_response    =
                {
                    .response = std::string(DEVIATION_PROJECTION_SERVER_VERSION_CONTROL),
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

            std::shared_ptr<ClientBoxManager> client_manager;

        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "deviation_projection/open_client";

            OpenClientResolver(std::shared_ptr<ClientBoxManager> client_manager) noexcept: client_manager(std::move(client_manager)){}

            auto handle(const dg_sock::network_rest_frame::model::Request& request) -> dg_sock::network_rest_frame::model::Response
            {
                if (std::string_view(request.payload_serialization_format) != dg::network_compact_serializer::get_dgstd_serialization_identifier())
                {
                    throw std::invalid_argument("unexpected request, bad serialization method");
                }

                OpenClientRequest semantic_request = dg::network_compact_serializer::dgstd_deserialize<OpenClientRequest>(request.payload);
                OpenClientResponse semantic_response;

                try
                {
                    uint64_t client_box_id = this->client_manager->open_client_box(semantic_request.connection_config);
                    
                    semantic_response = OpenClientResponse
                    {
                        .result = client_box_id,
                        .err_verbal_description = ""
                    };
                }
                catch (std::invalid_argument& e)
                {
                    semantic_response = OpenClientResponse
                    {
                        .result = std::unexpected(INVALID_ARGUMENT_ERROR_CODE),
                        .err_verbal_description = std::string(e.what())
                    };
                }
                catch (std::exception& e)
                {
                    semantic_response = OpenClientResponse
                    {
                        .result = std::unexpected(RUNTIME_ERROR_CODE),
                        .err_verbal_description = std::string(e.what())
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

    class CloseClientResolver: public virtual dg_sock::network_rest_frame::server::OneRequestHandlerInterface
    {
        private:

            std::shared_ptr<ClientBoxManager> client_manager;

        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "deviation_projection/close_client";

            CloseClientResolver(std::shared_ptr<ClientBoxManager> client_manager) noexcept: client_manager(std::move(client_manager)){}

            auto handle(const dg_sock::network_rest_frame::model::Request& request) -> dg_sock::network_rest_frame::model::Response
            {
                if (std::string_view(request.payload_serialization_format) != dg::network_compact_serializer::get_dgstd_serialization_identifier())
                {
                    throw std::invalid_argument("unexpected request, bad serialization method");
                }

                CloseClientRequest semantic_request = dg::network_compact_serializer::dgstd_deserialize<CloseClientRequest>(request.payload);
                this->client_manager->close_client_box(semantic_request.client_id);
                CloseClientResponse semantic_response
                {
                    .result = SUCCESS,
                    .err_verbal_description = ""
                };

                return dg_sock::network_rest_frame::model::Response
                {
                    .response = dg::network_compact_serializer::dgstd_serialize<dg_sock::string>(semantic_response),
                    .response_serialization_format = dg_sock::string(dg::network_compact_serializer::get_dgstd_serialization_identifier()),
                    .err_code = dg_sock::network_exception::SUCCESS
                };
            }
    };

    class AddTrainingDataResolver: public virtual dg_sock::network_rest_frame::server::OneRequestHandlerInterface
    {
        private:

            std::shared_ptr<ClientBoxManager> client_manager;

        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "deviation_projection/add_training_data";

            AddTrainingDataResolver(std::shared_ptr<ClientBoxManager> client_manager) noexcept: client_manager(std::move(client_manager)){}

            auto handle(const dg_sock::network_rest_frame::model::Request& request) -> dg_sock::network_rest_frame::model::Response
            {
                if (std::string_view(request.payload_serialization_format) != dg::network_compact_serializer::get_dgstd_serialization_identifier())
                {
                    throw std::invalid_argument("unexpected request, bad serialization method");
                }

                AddTrainingDataRequest semantic_request = dg::network_compact_serializer::dgstd_deserialize<AddTrainingDataRequest>(request.payload);
                AddTrainingDataResponse semantic_response;

                std::shared_ptr<ConnectionBoundClientBox> client_box = this->client_manager->get_client_box(semantic_request.client_id);

                if (client_box == nullptr)
                {
                    semantic_response = AddTrainingDataResponse
                    {
                        .result = CLIENT_NOT_FOUND_ERROR_CODE,
                        .err_verbal_description = "client not found"
                    };
                }
                else
                {
                    try
                    {
                        std::shared_ptr<std::string> inp    = std::make_shared<std::string>(semantic_request.input_matrix);
                        std::shared_ptr<std::string> out    = std::make_shared<std::string>(semantic_request.output_matrix);

                        client_box->add_training_data(inp, out);

                        semantic_response = AddTrainingDataResponse
                        {
                            .result = SUCCESS,
                            .err_verbal_description = ""
                        };
                    }
                    catch (std::invalid_argument& e)
                    {
                        semantic_response = AddTrainingDataResponse
                        {
                            .result = INVALID_ARGUMENT_ERROR_CODE,
                            .err_verbal_description = std::string(e.what())
                        };
                    }
                    catch (std::exception& e)
                    {
                        semantic_response = AddTrainingDataResponse
                        {
                            .result = RUNTIME_ERROR_CODE,
                            .err_verbal_description = std::string(e.what())
                        };
                    }
                }

                return dg_sock::network_rest_frame::model::Response
                {
                    .response = dg::network_compact_serializer::dgstd_serialize<dg_sock::string>(semantic_response),
                    .response_serialization_format = dg_sock::string(dg::network_compact_serializer::get_dgstd_serialization_identifier()),
                    .err_code = dg_sock::network_exception::SUCCESS
                };
            }
    };

    class ClearTrainingDataResolver: public virtual dg_sock::network_rest_frame::server::OneRequestHandlerInterface
    {
        private:

            std::shared_ptr<ClientBoxManager> client_manager;
        
        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "deviation_projection/clear_training_data";

            ClearTrainingDataResolver(std::shared_ptr<ClientBoxManager> client_manager) noexcept: client_manager(std::move(client_manager)){}

            auto handle(const dg_sock::network_rest_frame::model::Request& request) -> dg_sock::network_rest_frame::model::Response
            {
                if (std::string_view(request.payload_serialization_format) != dg::network_compact_serializer::get_dgstd_serialization_identifier())
                {
                    throw std::invalid_argument("unexpected request, bad serialization method");
                }

                ClearTrainingDataRequest semantic_request = dg::network_compact_serializer::dgstd_deserialize<ClearTrainingDataRequest>(request.payload);
                ClearTrainingDataResponse semantic_response;

                std::shared_ptr<ConnectionBoundClientBox> client_box = this->client_manager->get_client_box(semantic_request.client_id);

                if (client_box == nullptr)
                {
                    semantic_response = ClearTrainingDataResponse
                    {
                        .result = CLIENT_NOT_FOUND_ERROR_CODE,
                        .err_verbal_description = "client not found"
                    };
                }
                else
                {
                    try
                    {
                        client_box->clear_training_data();

                        semantic_response = ClearTrainingDataResponse
                        {
                            .result = SUCCESS,
                            .err_verbal_description = ""
                        };
                    }
                    catch (std::invalid_argument& e)
                    {
                        semantic_response = ClearTrainingDataResponse
                        {
                            .result = INVALID_ARGUMENT_ERROR_CODE,
                            .err_verbal_description = std::string(e.what())
                        };
                    }
                    catch (std::exception& e)
                    {
                        semantic_response = ClearTrainingDataResponse
                        {
                            .result = RUNTIME_ERROR_CODE,
                            .err_verbal_description = std::string(e.what())
                        };
                    }
                }

                return dg_sock::network_rest_frame::model::Response
                {
                    .response = dg::network_compact_serializer::dgstd_serialize<dg_sock::string>(semantic_response),
                    .response_serialization_format = dg_sock::string(dg::network_compact_serializer::get_dgstd_serialization_identifier()),
                    .err_code = dg_sock::network_exception::SUCCESS
                };
            }
    };

    class SetMatrixResourceResolver: public virtual dg_sock::network_rest_frame::server::OneRequestHandlerInterface
    {
        private:

            std::shared_ptr<ClientBoxManager> client_manager;

        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "deviation_projection/set_matrix_resource";

            SetMatrixResourceResolver(std::shared_ptr<ClientBoxManager> client_manager) noexcept: client_manager(std::move(client_manager)){}

            auto handle(const dg_sock::network_rest_frame::model::Request& request) -> dg_sock::network_rest_frame::model::Response
            {
                if (std::string_view(request.payload_serialization_format) != dg::network_compact_serializer::get_dgstd_serialization_identifier())
                {
                    throw std::invalid_argument("unexpected request, bad serialization method");
                }

                SetMatrixResourceRequest semantic_request = dg::network_compact_serializer::dgstd_deserialize<SetMatrixResourceRequest>(request.payload);
                SetMatrixResourceResponse semantic_response;

                std::shared_ptr<ConnectionBoundClientBox> client_box = this->client_manager->get_client_box(semantic_request.client_id);

                if (client_box == nullptr)
                {
                    semantic_response = SetMatrixResourceResponse
                    {
                        .result = CLIENT_NOT_FOUND_ERROR_CODE,
                        .err_verbal_description = "client not found"
                    };
                }
                else
                {
                    try
                    {
                        client_box->set_matrix_resource(semantic_request.matrix_resource_vec);

                        semantic_response = SetMatrixResourceResponse
                        {
                            .result = SUCCESS,
                            .err_verbal_description = ""
                        };
                    }
                    catch (std::invalid_argument& e)
                    {
                        semantic_response = SetMatrixResourceResponse
                        {
                            .result = INVALID_ARGUMENT_ERROR_CODE,
                            .err_verbal_description = std::string(e.what())
                        };
                    }
                    catch (std::exception& e)
                    {
                        semantic_response = SetMatrixResourceResponse
                        {
                            .result = RUNTIME_ERROR_CODE,
                            .err_verbal_description = std::string(e.what())  
                        };
                    }
                }

                return dg_sock::network_rest_frame::model::Response
                {
                    .response = dg::network_compact_serializer::dgstd_serialize<dg_sock::string>(semantic_response),
                    .response_serialization_format = dg_sock::string(dg::network_compact_serializer::get_dgstd_serialization_identifier()),
                    .err_code = dg_sock::network_exception::SUCCESS
                };
            }
    };

    class GetDeviationResolver: public virtual dg_sock::network_rest_frame::server::OneRequestHandlerInterface
    {
        private:

            std::shared_ptr<ClientBoxManager> client_manager;
        
        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "deviation_projection/get_deviation";

            GetDeviationResolver(std::shared_ptr<ClientBoxManager> client_manager) noexcept: client_manager(std::move(client_manager)){}

            auto handle(const dg_sock::network_rest_frame::model::Request& request) -> dg_sock::network_rest_frame::model::Response
            {
                if (std::string_view(request.payload_serialization_format) != dg::network_compact_serializer::get_dgstd_serialization_identifier())
                {
                    throw std::invalid_argument("unexpected request, bad serialization method");
                }

                GetDeviationRequest semantic_request = dg::network_compact_serializer::dgstd_deserialize<GetDeviationRequest>(request.payload);
                GetDeviationResponse semantic_response;

                std::shared_ptr<ConnectionBoundClientBox> client_box = this->client_manager->get_client_box(semantic_request.client_id);

                if (client_box == nullptr)
                {
                    semantic_response = GetDeviationResponse
                    {
                        .result = std::unexpected(CLIENT_NOT_FOUND_ERROR_CODE),
                        .err_verbal_description = "client not found"
                    };
                }
                else
                {
                    try
                    {
                        semantic_response = GetDeviationResponse
                        {
                            .result = client_box->get(),
                            .err_verbal_description = ""
                        };
                    }
                    catch (std::invalid_argument& e)
                    {
                        semantic_response = GetDeviationResponse
                        {
                            .result = std::unexpected(INVALID_ARGUMENT_ERROR_CODE),
                            .err_verbal_description = std::string(e.what())
                        };
                    }
                    catch (std::exception& e)
                    {
                        semantic_response = GetDeviationResponse
                        {
                            .result = std::unexpected(RUNTIME_ERROR_CODE),
                            .err_verbal_description = std::string(e.what())
                        };
                    }
                }

                return dg_sock::network_rest_frame::model::Response
                {
                    .response = dg::network_compact_serializer::dgstd_serialize<dg_sock::string>(semantic_response),
                    .response_serialization_format = dg_sock::string(dg::network_compact_serializer::get_dgstd_serialization_identifier()),
                    .err_code = dg_sock::network_exception::SUCCESS
                };
            }
    };

    void init()
    {
        std::shared_ptr<ClientBoxManager> client_box_manager = std::make_shared<ClientBoxManager>();

        dg_sock::network_rest_frame::server_instance::hook(GetVersionResolver::RESOLVABLE_PATH, std::make_unique<GetVersionResolver>());
        dg_sock::network_rest_frame::server_instance::hook(OpenClientResolver::RESOLVABLE_PATH, std::make_unique<OpenClientResolver>(client_box_manager));
        dg_sock::network_rest_frame::server_instance::hook(CloseClientResolver::RESOLVABLE_PATH, std::make_unique<CloseClientResolver>(client_box_manager));
        dg_sock::network_rest_frame::server_instance::hook(AddTrainingDataResolver::RESOLVABLE_PATH, std::make_unique<AddTrainingDataResolver>(client_box_manager));
        dg_sock::network_rest_frame::server_instance::hook(ClearTrainingDataResolver::RESOLVABLE_PATH, std::make_unique<ClearTrainingDataResolver>(client_box_manager));
        dg_sock::network_rest_frame::server_instance::hook(SetMatrixResourceResolver::RESOLVABLE_PATH, std::make_unique<SetMatrixResourceResolver>(client_box_manager));
        dg_sock::network_rest_frame::server_instance::hook(GetDeviationResolver::RESOLVABLE_PATH, std::make_unique<GetDeviationResolver>(client_box_manager));
    }

    void deinit() noexcept
    {
        dg_sock::network_rest_frame::server_instance::unhook(GetDeviationResolver::RESOLVABLE_PATH);
        dg_sock::network_rest_frame::server_instance::unhook(SetMatrixResourceResolver::RESOLVABLE_PATH);
        dg_sock::network_rest_frame::server_instance::unhook(ClearTrainingDataResolver::RESOLVABLE_PATH);
        dg_sock::network_rest_frame::server_instance::unhook(AddTrainingDataResolver::RESOLVABLE_PATH);
        dg_sock::network_rest_frame::server_instance::unhook(CloseClientResolver::RESOLVABLE_PATH);
        dg_sock::network_rest_frame::server_instance::unhook(OpenClientResolver::RESOLVABLE_PATH);
        dg_sock::network_rest_frame::server_instance::unhook(GetVersionResolver::RESOLVABLE_PATH);
    }
}

#endif