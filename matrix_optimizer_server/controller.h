#ifndef __MATRIX_OPTIMIZER_SERVER_CONTROLLER_H__
#define __MATRIX_OPTIMIZER_SERVER_CONTROLLER_H__

#include <stdint.h>
#include <stdlib.h>
#include "model.h"
#include "client_box.h"
#include "local_exception.h"
#include <request_extension/type_based_dgstd_resolutor.h>
#include <request_extension/type_based_resolutor_interface.h>

namespace matrix_optimizer_server
{
    static inline constexpr std::string_view MATRIX_OPTIMIZER_SERVER_VERSION_CONTROL = "";

    template <class T_In, class T_Out>
    using TypeBasedResolutorInterface = request_extension::resolutor::TypeBasedResolutorInterface<T_In, T_Out>;

    class GetVersionResolver: public virtual TypeBasedResolutorInterface<GetVersionRequest, GetVersionResponse>
    {
        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "matrix_optimizer/get_version";

            auto handle(const GetVersionRequest& request) -> GetVersionResponse
            {
                return GetVersionResponse
                {
                    .response = std::string(MATRIX_OPTIMIZER_SERVER_VERSION_CONTROL),
                    .err_verbal_description = {}
                };
            }
    };

    class OpenClientResolver: public virtual TypeBasedResolutorInterface<OpenClientRequest, OpenClientResponse>
    {
        private:

            std::shared_ptr<ClientBoxManager> client_box_manager;

        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "matrix_optimizer/open_client";

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
                        .result = matrix_optimizer_server::to_local_exception_error_code(std::current_exception()),
                        .err_verbal_description = matrix_optimizer_server::verbose_exception(std::current_exception())
                    };
                }
            }
    };

    class CloseClientResolver: public virtual TypeBasedResolutorInterface<CloseClientRequest, CloseClientResponse>
    {
        private:

            std::shared_ptr<ClientBoxManager> client_box_manager;

        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "matrix_optimizer/close_client";

            CloseClientResolver(std::shared_ptr<ClientBoxManager> client_box_manager) noexcept: client_box_manager(std::move(client_box_manager)){}

            auto handle(const CloseClientRequest& request) -> CloseClientResponse
            {
                this->client_box_manager->close_client_box(request.client_box_id);

                return CloseClientResponse
                {
                    .result = matrix_optimizer_server::SUCCESS,
                    .err_verbal_description = ""
                };
            }
    };

    class RunResolver: public virtual TypeBasedResolutorInterface<RunRequest, RunResponse>
    {
        private:

            std::shared_ptr<ClientBoxManager> client_box_manager;

        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "matrix_optimizer/run";

            RunResolver(std::shared_ptr<ClientBoxManager> client_box_manager) noexcept: client_box_manager(std::move(client_box_manager)){}

            auto handle(const RunRequest& request) -> RunResponse
            {
                try
                {
                    std::shared_ptr<ConnectionBoundClientBox> client_box = this->client_box_manager->get_client_box(request.client_box_id);

                    if (client_box == nullptr)
                    {
                        throw client_box_not_found_error{};
                    }

                    client_box->set_data_source(request.data_loader_config_vec);
                    client_box->set_remote_vector(request.remote_vec);
                    client_box->set_matrix_resource(request.matrix_resource);
                    client_box->set_matrix_deviation_wrapper(request.matrix_deviation_wrapper_config);
                    client_box->set_optimizer(request.optimizer_config);

                    client_box->run();

                    return RunResponse
                    {
                        .result = matrix_optimizer_server::SUCCESS,
                        .err_verbal_description = ""
                    };
                }
                catch (...)
                {
                    return RunResponse
                    {
                        .result = matrix_optimizer_server::to_local_exception_error_code(std::current_exception()),
                        .err_verbal_description = matrix_optimizer_server::verbose_exception(std::current_exception())
                    };
                }
            }
    };

    class IsCompletedResolver: public virtual TypeBasedResolutorInterface<IsCompletedRequest, IsCompletedResponse>
    {
        private:

            std::shared_ptr<ClientBoxManager> client_box_manager;
        
        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "matrix_optimizer/is_completed";

            IsCompletedResolver(std::shared_ptr<ClientBoxManager> client_box_manager) noexcept: client_box_manager(std::move(client_box_manager)){}

            auto handle(const IsCompletedRequest& request) -> IsCompletedResponse
            {
                try
                {
                    std::shared_ptr<ConnectionBoundClientBox> client_box = this->client_box_manager->get_client_box(request.client_box_id);

                    if (client_box == nullptr)
                    {
                        throw client_box_not_found_error{};
                    }

                    return IsCompletedResponse
                    {
                        .result = client_box->is_completed(),
                        .err_verbal_description = ""
                    };
                }
                catch (...)
                {
                    return IsCompletedResponse
                    {
                        .result = std::unexpected(matrix_optimizer_server::to_local_exception_error_code(std::current_exception())),
                        .err_verbal_description = matrix_optimizer_server::verbose_exception(std::current_exception())
                    };
                }
            }
    };

    class GetResultResolver: public virtual TypeBasedResolutorInterface<GetResultRequest, GetResultResponse>
    {
        private:

            std::shared_ptr<ClientBoxManager> client_box_manager;

        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "matrix_optimizer/get_result";

            GetResultResolver(std::shared_ptr<ClientBoxManager> client_box_manager) noexcept: client_box_manager(std::move(client_box_manager)){}

            auto handle(const GetResultRequest& request) -> GetResultResponse
            {
                try
                {
                    std::shared_ptr<ConnectionBoundClientBox> client_box = this->client_box_manager->get_client_box(request.client_box_id);

                    if (client_box == nullptr)
                    {
                        throw client_box_not_found_error{};
                    }

                    if (!client_box->is_completed())
                    {
                        throw optimization_in_progress_error{};
                    }

                    return GetResultResponse
                    {
                        .result = client_box->wait(),
                        .err_verbal_description = ""
                    };
                }
                catch (...)
                {
                    return GetResultResponse
                    {
                        .result = std::unexpected(matrix_optimizer_server::to_local_exception_error_code(std::current_exception())),
                        .err_verbal_description = matrix_optimizer_server::verbose_exception(std::current_exception())
                    };
                }
            }
    };
}

#endif