#ifndef __MATRIX_PROJECTION_SERVER_H__
#define __MATRIX_PROJECTION_SERVER_H__

#include <stdint.h>
#include <stdlib.h>
#include <matrix/generic_matrix_factory.h>
#include <request_extension/type_based_resolutor_interface.h>
#include <request_extension/type_based_dgstd_resolutor.h>

namespace matrix_projection_server
{
    static inline constexpr std::string_view MATRIX_PROJECTION_SERVER_VERSION_CONTROL = "";

    //during my thesis of structure and server, it's seemed that the two semantic spaces are coupled as if they are unique references to each other
    //it's seemed that the structures cannot be shared thus encapsulated by the server handler solely, and publish the structure via document or some sort of self-explanatory API process

    class ClientBox
    {
        private:

            std::unique_ptr<the_matrix::MatrixInterface> matrix;

        public:

            ClientBox(): matrix(nullptr){}

            void set_matrix(const generic_matrix_factory::ExternalGenericMatrixResource& matrix_resource)
            {
                this->matrix = generic_matrix_factory::ExternalGenericMatrixLoader{}.load_resource(matrix_resource);
            }

            auto project(const matrix_serializer::GenericMatrix& generic_in_matrix) -> matrix_serializer::GenericMatrix
            {
                std::shared_ptr<tensor_model::Matrix> in_matrix = matrix_serializer::deserialize(generic_in_matrix);

                return matrix_serializer::serialize(this->matrix->project({in_matrix})[0]);
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


            void set_matrix(const generic_matrix_factory::ExternalGenericMatrixResource& matrix_resource)
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                this->base->set_matrix(matrix_resource);
            }

            auto project(const matrix_serializer::GenericMatrix& generic_in_matrix) -> matrix_serializer::GenericMatrix
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                return this->base->project(generic_in_matrix);
            }

            void close() noexcept
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                this->connection->close();
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
                return this->base->get_client_box(client_box_id);
            }

            void close_client_box(uint64_t client_box_id)
            {
                this->base->close_client_box(client_box_id);
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
        std::expected<std::string, matrix_projection_server::local_exception_t> response;
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
        std::expected<uint64_t, matrix_projection_server::local_exception_t> client_box_id;
        std::string err_verbal_description;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(client_box_id, err_verbal_description);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(client_box_id, err_verbal_description);
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

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(client_box_id);
        }
    };

    struct CloseClientResponse
    {
        matrix_projection_server::local_exception_t result;
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

    struct SetMatrixRequest
    {
        uint64_t client_box_id;
        generic_matrix_factory::ExternalGenericMatrixResource matrix_resource;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(client_box_id, matrix_resource);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(client_box_id, matrix_resource);
        }
    };

    struct SetMatrixResponse
    {
        matrix_projection_server::local_exception_t result;
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

    struct ProjectMatrixRequest
    {
        matrix_serializer::GenericMatrix generic_matrix;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(generic_matrix);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(generic_matrix);
        }
    };

    struct ProjectMatrixResponse
    {
        std::expected<matrix_serializer::GenericMatrix, matrix_projection_server::local_exception_t> result;
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

            static inline constexpr std::string_view RESOLVABLE_PATH = "matrix_projection_server/get_version";

            auto handle(const GetVersionRequest& request) -> GetVersionResponse
            {
                (void) request;

                return GetVersionResponse
                {
                    .response = std::string(MATRIX_PROJECTION_SERVER_VERSION_CONTROL),
                    .err_verbal_description = {}
                };
            }
    };

    class OpenClientResolver: public virtual TypeBasedResolutorInterface<OpenClientRequest, OpenClientResponse>
    {
        private:

            std::shared_ptr<ClientBoxManager> client_box_manager;

        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "matrix_projection_server/open_client";

            OpenClientResolver(std::shared_ptr<ClientBoxManager> client_box_manager) noexcept: client_box_manager(std::move(client_box_manager)){}

            auto handle(const OpenClientRequest& request) -> OpenClientResponse
            {
                try
                {
                    uint64_t client_box_id = this->client_box_manager->open_client_box(request.connection_config);

                    return OpenClientResponse
                    {
                        .result = client_box_id,
                        .err_verbal_description = ""
                    };
                }
                catch (...)
                {
                    return OpenClientResponse
                    {
                        .result = matrix_projection_server::to_local_exception_error_code(std::current_exception()),
                        .err_verbal_description = matrix_projection_server::verbose_local_exception(matrix_projection_server::to_local_exception_error_code(std::current_exception()))
                    };
                }
            }
    };

    class CloseClientResolver: public virtual TypeBasedResolutorInterface<CloseClientRequest, CloseClientResponse>
    {
        private:

            std::shared_ptr<ClientBoxManager> client_box_manager;

        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "matrix_projection_server/close_client";

            CloseClientResolver(std::shared_ptr<ClientBoxManager> client_box_manager) noexcept: client_box_manager(std::move(client_box_manager)){}

            auto handle(const CloseClientRequest& request) -> CloseClientResponse
            {
                try
                {
                    this->client_box_manager->close_client_box(request.client_box_id);

                    return CloseClientResponse
                    {
                        .result = SUCCESS,
                        .err_verbal_description = ""
                    };
                }
                catch (...)
                {
                    return CloseClientResponse
                    {
                        .result = matrix_projection_server::to_local_exception_error_code(std::current_exception()),
                        .err_verbal_description = matrix_projection_server::verbose_error_code(matrix_projection_server::to_local_exception_error_code(std::current_exception()))
                    };
                }
            }
    };

    class SetMatrixResolver: public virtual TypeBasedResolutorInterface<SetMatrixRequest, SetMatrixResponse>
    {
        private:

            std::shared_ptr<ClientBoxManager> client_box_manager;

        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "matrix_projection_server/set_matrix";

            SetMatrixResolver(std::shared_ptr<ClientBoxManager> client_box_manager) noexcept: client_box_manager(std::move(client_box_manager)){}

            auto handle(const SetMatrixRequest& request) -> SetMatrixResponse
            {
                try
                {
                    std::shared_ptr<ConnectionBoundClientBox> client_box = this->client_box_manager->get_client_box(request.client_box_id);

                    if (client_box == nullptr)
                    {
                        throw client_box_not_found_error{};
                    }

                    client_box->set_matrix(semantic_request.matrix_resource);

                    return SetMatrixResponse
                    {
                        .result = SUCCESS,
                        .err_verbal_description = ""
                    };
                }
                catch (...)
                {
                    return SetMatrixResponse
                    {
                        .result = matrix_projection_server::to_local_exception_error_code(std::current_exception()),
                        .err_verbal_description = matrix_projection_server::verbose_error_code(matrix_projection_server::to_local_exception_error_code(std::current_exception()))
                    };
                }
            }
    };

    class ProjectMatrixResolver: public virtual TypeBasedResolutorInterface<ProjectMatrixRequest, ProjectMatrixResponse>
    {
        private:

            std::shared_ptr<ClientBoxManager> client_box_manager;

        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "matrix_projection_server/project_matrix";

            ProjectMatrixResolver(std::shared_ptr<ClientBoxManager> client_box_manager) noexcept: client_box_manager(std::move(client_box_manager)){}

            auto handle(const ProjectMatrixRequest& request) -> ProjectMatrixResponse
            {
                try
                {
                    std::shared_ptr<ConnectionBoundClientBox> client_box = this->client_box_manager->get_client_box(request.client_box_id);

                    if (client_box == nullptr)
                    {
                        throw client_box_not_found_error{};
                    }

                    matrix_serializer::GenericMatrix result = client_box->project(request.generic_matrix);
                    return ProjectMatrixResponse
                    {
                        .result = std::move(result),
                        .err_verbal_description = matrix_projection_server::verbose_error_code(matrix_projection_server::to_local_exception_error_code(std::current_exception()))
                    };
                }
                catch (...)
                {
                    return ProjectMatrixResponse
                    {
                        .result = std::unexpected(matrix_projection_server::to_local_exception_error_code(std::current_exception())),
                        .err_verbal_description = matrix_projection_server::verbose_error_code(matrix_projection_server::to_local_exception_error_code(std::current_exception()))
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
        dg_sock::network_rest_frame::server_instance::hook(SetMatrixResolver::RESOLVABLE_PATH, wrap(std::make_unique<SetMatrixResolver>(client_box_manager)));
        dg_sock::network_rest_frame::server_instance::hook(ProjectMatrixResolver::RESOLVABLE_PATH, wrap(std::make_unique<ProjectMatrixResolver>(client_box_manager)));
    }

    void deinit() noexcept
    {
        dg_sock::network_rest_frame::server_instance::unhook(ProjectMatrixResolver::RESOLVABLE_PATH);
        dg_sock::network_rest_frame::server_instance::unhook(SetMatrixResolver::RESOLVABLE_PATH);
        dg_sock::network_rest_frame::server_instance::unhook(CloseClientResolver::RESOLVABLE_PATH);
        dg_sock::network_rest_frame::server_instance::unhook(OpenClientResolver::RESOLVABLE_PATH);
        dg_sock::network_rest_frame::server_instance::unhook(GetVersionResolver::RESOLVABLE_PATH);
    }
}

#endif