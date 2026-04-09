#ifndef __DEVIATION_PROJECTION_INGESTION_AID_SERVER_CONTROLLER_H__
#define __DEVIATION_PROJECTION_INGESTION_AID_SERVER_CONTROLLER_H__

#include <stdint.h>
#include <stdlib.h>
#include <request_extension/type_based_resolutor_interface.h>
#include <request_extension/type_based_dgstd_resolutor.h>
#include <internal_rest/network_rest_frame.h>
#include "client_box.h"
#include "model.h"
#include "local_exception.h"

namespace deviation_projection_ingestion_aid_server
{
    static inline constexpr std::string_view DEVIATION_PROJECTION_INGESTION_AID_SERVER_VERSION_CONTROL = "";

    template <class T_In, class T_Out>
    using TypeBasedResolutorInterface = request_extension::resolutor::TypeBasedResolutorInterface<T_In, T_Out>;

    class GetVersionResolver: public virtual TypeBasedResolutorInterface<GetVersionRequest, GetVersionResponse>
    {
        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "deviation_projection_ingestion_aid_server/get_version";

            auto handle(const GetVersionRequest& request) -> GetVersionResponse
            {
                (void) request;

                return GetVersionResponse
                {
                    .response = std::string(DEVIATION_PROJECTION_INGESTION_AID_SERVER_VERSION_CONTROL),
                    .err_verbal_description = ""
                };
            }
    };

    class OpenClientResolver: public virtual TypeBasedResolutorInterface<OpenClientRequest, OpenClientResponse>
    {
        private:

            std::shared_ptr<ClientBoxManager> client_box_manager;

        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "deviation_projection_ingestion_aid_server/open_client";

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
                        .result = deviation_projection_ingestion_aid_server::to_local_exception_error_code(std::current_exception()),
                        .err_verbal_description = deviation_projection_ingestion_aid_server::verbose_exception(std::current_exception())
                    };
                }
            }
    };

    class CloseClientResolver: public virtual TypeBasedResolutorInterface<CloseClientRequest, CloseClientResponse>
    {
        private:

            std::shared_ptr<ClientBoxManager> client_box_manager;
        
        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "deviation_projection_ingestion_aid_server/close_client";

            CloseClientResolver(std::shared_ptr<ClientBoxManager> client_box_manager) noexcept: client_box_manager(std::move(client_box_manager)){}

            auto handle(const CloseClientRequest& request) -> CloseClientResponse
            {   
                this->client_box_manager->close_client_box(request.client_box_id);

                return CloseClientResponse
                {
                    .result = SUCCESS,
                    .err_verbal_description = ""
                };
            }
    };

    class RunResolver: public virtual TypeBasedResolutorInterface<RunRequest, RunResponse>
    {
        private:

            std::shared_ptr<ClientBoxManager> client_box_manager;

        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "deviation_projection_ingestion_aid_server/run";

            RunResolver(std::shared_ptr<ClientBoxManager> client_box_manager) noexcept: client_box_manager(std::move(client_box_manager)){}

            auto handle(const RunRequest& request) -> RunResponse
            {
                try
                {
                    std::shared_ptr<ConnectionBoundClientBox> client = this->client_box_manager->get_client_box(request.client_box_id);

                    if (client == nullptr)
                    {
                        throw client_box_not_found_error{};
                    }

                    client->set_data_source(request.data_loader_config);
                    client->set_server_sink(request.server_sink_vec);
                    client->set_firer_config(request.token_firer_config);
                    client->set_concurrent_request_size(request.concurrent_request_sz);
                    client->set_client_retry_policy(request.client_retry_policy);

                    client->run();

                    return RunResponse
                    {
                        .result = deviation_projection_ingestion_aid_server::SUCCESS,
                        .err_verbal_description = ""
                    };
                }
                catch (...)
                {
                    return RunResponse
                    {
                        .result = deviation_projection_ingestion_aid_server::to_local_exception_error_code(std::current_exception()),
                        .err_verbal_description = deviation_projection_ingestion_aid_server::verbose_exception(std::current_exception())
                    };
                }
            }
    };

    class InterruptResolver: public virtual TypeBasedResolutorInterface<InterruptRequest, InterruptResponse>
    {
        private:

            std::shared_ptr<ClientBoxManager> client_box_manager;

        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "deviation_projection_ingestion_aid_server/interrupt";

            InterruptResolver(std::shared_ptr<ClientBoxManager> client_box_manager) noexcept: client_box_manager(std::move(client_box_manager)){}

            auto handle(const InterruptRequest& request) -> InterruptResponse
            {
                try
                {
                    std::shared_ptr<ConnectionBoundClientBox> client = this->client_box_manager->get_client_box(request.client_box_id);

                    if (client == nullptr)
                    {
                        throw client_box_not_found_error{};
                    }

                    client->interrupt();

                    return InterruptResponse
                    {
                        .result = deviation_projection_ingestion_aid_server::SUCCESS,
                        .err_verbal_description = ""
                    };
                }
                catch (...)
                {
                    return InterruptResponse
                    {
                        .result = deviation_projection_ingestion_aid_server::to_local_exception_error_code(std::current_exception()),
                        .err_verbal_description = deviation_projection_ingestion_aid_server::verbose_exception(std::current_exception())
                    };
                }
            }
    };

    class IsCompletedResolver: public virtual TypeBasedResolutorInterface<IsCompletedRequest, IsCompletedResponse>
    {
        private:

            std::shared_ptr<ClientBoxManager> client_box_manager;

        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "deviation_projection_ingestion_aid_server/is_completed";

            IsCompletedResolver(std::shared_ptr<ClientBoxManager> client_box_manager) noexcept: client_box_manager(std::move(client_box_manager)){}

            auto handle(const IsCompletedRequest& request) -> IsCompletedResponse
            {
                try
                {
                    std::shared_ptr<ConnectionBoundClientBox> client = this->client_box_manager->get_client_box(request.client_box_id);

                    if (client == nullptr)
                    {
                        throw client_box_not_found_error{};
                    }

                    return IsCompletedResponse
                    {
                        .result = client->is_completed(),
                        .err_verbal_description = ""
                    };
                }
                catch (...)
                {
                    return IsCompletedResponse
                    {
                        .result = std::unexpected(deviation_projection_ingestion_aid_server::to_local_exception_error_code(std::current_exception())),
                        .err_verbal_description = deviation_projection_ingestion_aid_server::verbose_exception(std::current_exception())
                    };
                }
            }
    };

    class GetResultResolver: public virtual TypeBasedResolutorInterface<GetResultRequest, GetResultResponse>
    {
        private:

            std::shared_ptr<ClientBoxManager> client_box_manager;

        public:

            static inline constexpr std::string_view RESOLVABLE_PATH = "deviation_projection_ingestion_aid_server/get_result";

            GetResultResolver(std::shared_ptr<ClientBoxManager> client_box_manager) noexcept: client_box_manager(std::move(client_box_manager)){}

            auto handle(const GetResultRequest& request) -> GetResultResponse
            {
                try
                {
                    std::shared_ptr<ConnectionBoundClientBox> client = this->client_box_manager->get_client_box(request.client_box_id);

                    if (client == nullptr)
                    {
                        throw client_box_not_found_error{};
                    }

                    client->wait();

                    return GetResultResponse
                    {
                        .result = deviation_projection_ingestion_aid_server::SUCCESS,
                        .err_verbal_description = ""
                    };
                }
                catch (...)
                {
                    return GetResultResponse
                    {
                        .result = deviation_projection_ingestion_aid_server::to_local_exception_error_code(std::current_exception()),
                        .err_verbal_description = deviation_projection_ingestion_aid_server::verbose_exception(std::current_exception())
                    };
                }
            }
    };
}

#endif