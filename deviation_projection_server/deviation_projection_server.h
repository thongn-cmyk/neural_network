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
#include <request_extension/type_based_dgstd_resolutor.h>
#include <request_extension/type_based_resolutor_interface.h>

namespace deviation_projection_server
{
    using namespace float_def;

    using local_exception_t = uint8_t;

    static inline constexpr local_exception_t SUCCESS                                       = 0u;
    static inline constexpr local_exception_t INVALID_ARGUMENT_ERROR_CODE                   = 1u;
    static inline constexpr local_exception_t RUNTIME_ERROR_CODE                            = 2u;
    static inline constexpr local_exception_t CLIENT_NOT_FOUND_ERROR_CODE                   = 3u;

    static inline constexpr std::string_view DEVIATION_PROJECTION_SERVER_VERSION_CONTROL    = "";

    struct local_invalid_argument: std::invalid_argument
    {
        local_invalid_argument(const char * msg = "invalid argument"): std::invalid_argument(msg){}
    };

    struct local_runtime_error: std::runtime_error
    {
        local_runtime_error(const char * msg = "runtime error"): std::runtime_error(msg){}
    };

    struct client_not_found_error: std::invalid_argument
    {
        client_not_found_error(): std::invalid_argument("bad client box, client_box id not found"){}
    };

    class ClientBox
    {
        private:

            std::vector<std::shared_ptr<std::string>> training_data;
            std::vector<std::unique_ptr<deviation_projector::GenericMatrixDeviationCalculatorInterface>> deviation_calculator_vec;

        public:

            ClientBox(): training_data(),
                         deviation_calculator_vec(){}

            void add_training_data(const std::string& token)
            {
                this->training_data.push_back(std::make_shared<std::string>(token));
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

            void add_training_data(const std::string& token)
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                if (this->was_explicitly_destroyed->load(std::memory_order_relaxed))
                {
                    throw std::runtime_error("invalid operation, closed client box");
                }

                this->base->add_training_data(token);
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
        std::string token;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(client_id, token);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(client_id, token);
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

    struct SetAndGetDeviationRequest
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

    struct SetAndGetDeviationResponse
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

    template <class T_In, class T_Out>
    using TypeBasedResolutorInterface = request_extension::resolutor::TypeBasedResolutorInterface<T_In, T_Out>;

    class GetVersionResolver: public virtual TypeBasedResolutorInterface<GetVersionRequest, GetVersionResponse>
    {
        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "deviation_projection/get_version";

            auto handle(const GetVersionRequest& request) -> GetVersionResponse
            {
                return GetVersionResponse
                {
                    .response = std::string(DEVIATION_PROJECTION_SERVER_VERSION_CONTROL),
                    .err_verbal_description = ""
                };
            }
    };

    class OpenClientResolver: public virtual TypeBasedResolutorInterface<OpenClientRequest, OpenClientResponse>
    {
        private:

            std::shared_ptr<ClientBoxManager> client_manager;

        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "deviation_projection/open_client";

            OpenClientResolver(std::shared_ptr<ClientBoxManager> client_manager) noexcept: client_manager(std::move(client_manager)){}

            auto handle(OpenClientRequest& request) -> OpenClientResponse
            {
                try
                {
                    uint64_t client_box_id = this->client_manager->open_client_box(request.connection_config);
                    
                    return OpenClientResponse
                    {
                        .result = client_box_id,
                        .err_verbal_description = ""
                    };
                }
                catch (std::invalid_argument& e)
                {
                    return OpenClientResponse
                    {
                        .result = std::unexpected(INVALID_ARGUMENT_ERROR_CODE),
                        .err_verbal_description = std::string(e.what())
                    };
                }
                catch (std::exception& e)
                {
                    return OpenClientResponse
                    {
                        .result = std::unexpected(RUNTIME_ERROR_CODE),
                        .err_verbal_description = std::string(e.what())
                    };
                }
            }
    };

    class CloseClientResolver: public virtual TypeBasedResolutorInterface<CloseClientRequest, CloseClientResponse>
    {
        private:

            std::shared_ptr<ClientBoxManager> client_manager;

        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "deviation_projection/close_client";

            CloseClientResolver(std::shared_ptr<ClientBoxManager> client_manager) noexcept: client_manager(std::move(client_manager)){}

            auto handle(const CloseClientRequest& request) -> CloseClientResponse
            {
                this->client_manager->close_client_box(request.client_id);

                return CloseClientResponse
                {
                    .result = SUCCESS,
                    .err_verbal_description = ""
                };
            }
    };

    class AddTrainingDataResolver: public virtual TypeBasedResolutorInterface<AddTrainingDataRequest, AddTrainingDataResponse>
    {
        private:

            std::shared_ptr<ClientBoxManager> client_manager;

        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "deviation_projection/add_training_data";

            AddTrainingDataResolver(std::shared_ptr<ClientBoxManager> client_manager) noexcept: client_manager(std::move(client_manager)){}

            auto handle(const AddTrainingDataRequest& request) -> AddTrainingDataResponse
            {
                std::shared_ptr<ConnectionBoundClientBox> client_box = this->client_manager->get_client_box(request.client_id);

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
                        client_box->add_training_data(request.training_token);

                        return AddTrainingDataResponse
                        {
                            .result = SUCCESS,
                            .err_verbal_description = ""
                        };
                    }
                    catch (std::invalid_argument& e)
                    {
                        return AddTrainingDataResponse
                        {
                            .result = INVALID_ARGUMENT_ERROR_CODE,
                            .err_verbal_description = std::string(e.what())
                        };
                    }
                    catch (std::exception& e)
                    {
                        return AddTrainingDataResponse
                        {
                            .result = RUNTIME_ERROR_CODE,
                            .err_verbal_description = std::string(e.what())
                        };
                    }
                }
            }
    };

    class ClearTrainingDataResolver: public virtual TypeBasedResolutorInterface<ClearTrainingDataRequest, ClearTrainingDataResponse>
    {
        private:

            std::shared_ptr<ClientBoxManager> client_manager;

        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "deviation_projection/clear_training_data";

            ClearTrainingDataResolver(std::shared_ptr<ClientBoxManager> client_manager) noexcept: client_manager(std::move(client_manager)){}

            auto handle(const ClearTrainingDataRequest& request) -> ClearTrainingDataResponse
            {
                std::shared_ptr<ConnectionBoundClientBox> client_box = this->client_manager->get_client_box(request.client_id);

                if (client_box == nullptr)
                {
                    return ClearTrainingDataResponse
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

                        return ClearTrainingDataResponse
                        {
                            .result = SUCCESS,
                            .err_verbal_description = ""
                        };
                    }
                    catch (std::exception& e)
                    {
                        return ClearTrainingDataResponse
                        {
                            .result = RUNTIME_ERROR_CODE,
                            .err_verbal_description = std::string(e.what())
                        };
                    }
                }
            }
    };

    class SetMatrixResourceResolver: public virtual TypeBasedResolutorInterface<SetMatrixResourceRequest, SetMatrixResourceResponse>
    {
        private:

            std::shared_ptr<ClientBoxManager> client_manager;

        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "deviation_projection/set_matrix_resource";

            SetMatrixResourceResolver(std::shared_ptr<ClientBoxManager> client_manager) noexcept: client_manager(std::move(client_manager)){}

            auto handle(const SetMatrixResourceRequest& request) -> SetMatrixResourceResponse
            {
                std::shared_ptr<ConnectionBoundClientBox> client_box = this->client_manager->get_client_box(request.client_id);

                if (client_box == nullptr)
                {
                    return SetMatrixResourceResponse
                    {
                        .result = CLIENT_NOT_FOUND_ERROR_CODE,
                        .err_verbal_description = "client not found"
                    };
                }
                else
                {
                    try
                    {
                        client_box->set_matrix_resource(request.matrix_resource_vec);

                        return SetMatrixResourceResponse
                        {
                            .result = SUCCESS,
                            .err_verbal_description = ""
                        };
                    }
                    catch (std::invalid_argument& e)
                    {
                        return SetMatrixResourceResponse
                        {
                            .result = INVALID_ARGUMENT_ERROR_CODE,
                            .err_verbal_description = std::string(e.what())
                        };
                    }
                    catch (std::exception& e)
                    {
                        return SetMatrixResourceResponse
                        {
                            .result = RUNTIME_ERROR_CODE,
                            .err_verbal_description = std::string(e.what())  
                        };
                    }
                }
            }
    };

    class GetDeviationResolver: public virtual TypeBasedResolutorInterface<GetDeviationRequest, GetDeviationResponse>
    {
        private:

            std::shared_ptr<ClientBoxManager> client_manager;

        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "deviation_projection/get_deviation";

            GetDeviationResolver(std::shared_ptr<ClientBoxManager> client_manager) noexcept: client_manager(std::move(client_manager)){}

            auto handle(const GetDeviationRequest& request) -> GetDeviationResponse
            {
                std::shared_ptr<ConnectionBoundClientBox> client_box = this->client_manager->get_client_box(request.client_id);

                if (client_box == nullptr)
                {
                    return GetDeviationResponse
                    {
                        .result = std::unexpected(CLIENT_NOT_FOUND_ERROR_CODE),
                        .err_verbal_description = "client not found"
                    };
                }
                else
                {
                    try
                    {
                        return GetDeviationResponse
                        {
                            .result = client_box->get(),
                            .err_verbal_description = ""
                        };
                    }
                    catch (std::invalid_argument& e)
                    {
                        return GetDeviationResponse
                        {
                            .result = std::unexpected(INVALID_ARGUMENT_ERROR_CODE),
                            .err_verbal_description = std::string(e.what())
                        };
                    }
                    catch (std::exception& e)
                    {
                        return GetDeviationResponse
                        {
                            .result = std::unexpected(RUNTIME_ERROR_CODE),
                            .err_verbal_description = std::string(e.what())
                        };
                    }
                }
            }
    };

    class SetAndGetDeviationResolutor: public virtual TypeBasedResolutorInterface<SetAndGetDeviationRequest, SetAndGetDeviationResponse>
    {
        private:

            std::shared_ptr<ClientBoxManager> client_manager;

        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "deviation_projection/set_and_get_deviation";

            SetAndGetDeviationResolutor(std::shared_ptr<ClientBoxManager> client_manager) noexcept: client_manager(std::move(client_manager)){}

            auto handle(const SetAndGetDeviationRequest& request) -> SetAndGetDeviationResponse
            {
                std::shared_ptr<ConnectionBoundClientBox> client_box = this->client_manager->get_client_box(request.client_id);

                if (client_box == nullptr)
                {
                    return SetAndGetDeviationResponse
                    {
                        .result = std::unexpected(CLIENT_NOT_FOUND_ERROR_CODE),
                        .err_verbal_description = "client not found"
                    };
                }

                try
                {
                    client_box->set_matrix_resource(request.matrix_resource_vec);

                    return SetAndGetDeviationResponse
                    {
                        .result = client_box->get(),
                        .err_verbal_description = ""
                    };
                }
                catch (std::invalid_argument& e)
                {
                    return SetAndGetDeviationResponse
                    {
                        .result = std::unexpected(INVALID_ARGUMENT_ERROR_CODE),
                        .err_verbal_description = std::string(e.what())
                    };
                }
                catch (std::exception& e)
                {
                    return SetAndGetDeviationResponse
                    {
                        .result = std::unexpected(RUNTIME_ERROR_CODE),
                        .err_verbal_description = std::string(e.what())
                    };
                }
            }
    };

    void init()
    {
        using namespace request_extension::resolutor;

        std::shared_ptr<ClientBoxManager> client_box_manager = std::make_shared<ClientBoxManager>();

        dg_sock::network_rest_frame::server_instance::hook(GetVersionResolver::RESOLVABLE_PATH, wrap(std::make_unique<GetVersionResolver>()));
        dg_sock::network_rest_frame::server_instance::hook(OpenClientResolver::RESOLVABLE_PATH, wrap(std::make_unique<OpenClientResolver>(client_box_manager)));
        dg_sock::network_rest_frame::server_instance::hook(CloseClientResolver::RESOLVABLE_PATH, wrap(std::make_unique<CloseClientResolver>(client_box_manager)));
        dg_sock::network_rest_frame::server_instance::hook(AddTrainingDataResolver::RESOLVABLE_PATH, wrap(std::make_unique<AddTrainingDataResolver>(client_box_manager)));
        dg_sock::network_rest_frame::server_instance::hook(ClearTrainingDataResolver::RESOLVABLE_PATH, wrap(std::make_unique<ClearTrainingDataResolver>(client_box_manager)));
        dg_sock::network_rest_frame::server_instance::hook(SetMatrixResourceResolver::RESOLVABLE_PATH, wrap(std::make_unique<SetMatrixResourceResolver>(client_box_manager)));
        dg_sock::network_rest_frame::server_instance::hook(GetDeviationResolver::RESOLVABLE_PATH, wrap(std::make_unique<GetDeviationResolver>(client_box_manager)));
        dg_sock::network_rest_frame::server_instance::hook(SetAndGetDeviationResolutor::RESOLVABLE_PATH, wrap(std::make_unique<SetAndGetDeviationResolutor>(client_box_manager)));
    }

    void deinit() noexcept
    {
        dg_sock::network_rest_frame::server_instance::unhook(SetAndGetDeviationResolutor::RESOLVABLE_PATH);
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