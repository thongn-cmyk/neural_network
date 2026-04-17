#ifndef __MATRIX_PROJECTION_SERVER_H__
#define __MATRIX_PROJECTION_SERVER_H__

#include <stdint.h>
#include <stdlib.h>
#include <matrix/generic_matrix_factory.h>
#include <request_extension/type_based_resolutor_interface.h>
#include <request_extension/type_based_dgstd_resolutor.h>
#include <matrix/matrix_serializer.h>
#include "local_exception.h"
#include "model.h"
#include "client_box.h"

namespace matrix_projection_server
{
    static inline constexpr std::string_view MATRIX_PROJECTION_SERVER_VERSION_CONTROL = "";

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
                        .result = std::unexpected(matrix_projection_server::to_local_exception_error_code(std::current_exception())),
                        .err_verbal_description = matrix_projection_server::verbose_exception(std::current_exception())
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
                        .err_verbal_description = matrix_projection_server::verbose_exception(std::current_exception())
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

                    client_box->set_matrix(request.matrix_resource);

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
                        .err_verbal_description = matrix_projection_server::verbose_exception(std::current_exception())
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
                        .err_verbal_description = ""
                    };
                }
                catch (...)
                {
                    return ProjectMatrixResponse
                    {
                        .result = std::unexpected(matrix_projection_server::to_local_exception_error_code(std::current_exception())),
                        .err_verbal_description = matrix_projection_server::verbose_exception(std::current_exception())
                    };
                }
            }
    };
}

#endif