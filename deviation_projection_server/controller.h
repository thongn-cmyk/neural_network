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
#include "local_exception.h"
#include "model.h"
#include "client_box.h"

namespace deviation_projection_server
{
    static inline constexpr std::string_view DEVIATION_PROJECTION_SERVER_VERSION_CONTROL    = "";

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

            auto handle(const OpenClientRequest& request) -> OpenClientResponse
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
                catch (...)
                {
                    return OpenClientResponse
                    {
                        .result = std::unexpected(to_local_exception_error_code(std::current_exception())),
                        .err_verbal_description = verbose_exception(std::current_exception())
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
                try
                {
                    std::shared_ptr<ConnectionBoundClientBox> client_box = this->client_manager->get_client_box(request.client_id);

                    if (client_box == nullptr)
                    {
                        throw client_box_not_found_error{};
                    }

                    client_box->add_training_data(request.token);

                    return AddTrainingDataResponse
                    {
                        .result = SUCCESS,
                        .err_verbal_description = ""
                    };
                }
                catch (...)
                {
                    return AddTrainingDataResponse
                    {
                        .result = to_local_exception_error_code(std::current_exception()),
                        .err_verbal_description = verbose_exception(std::current_exception())
                    };
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
                try
                {
                    std::shared_ptr<ConnectionBoundClientBox> client_box = this->client_manager->get_client_box(request.client_id);

                    if (client_box == nullptr)
                    {
                        throw client_box_not_found_error{};
                    }

                    client_box->clear_training_data();

                    return ClearTrainingDataResponse
                    {
                        .result = SUCCESS,
                        .err_verbal_description = ""
                    };
                }
                catch (...)
                {
                    return ClearTrainingDataResponse
                    {
                        .result = to_local_exception_error_code(std::current_exception()),
                        .err_verbal_description = verbose_exception(std::current_exception())
                    };
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
                try
                {
                    std::shared_ptr<ConnectionBoundClientBox> client_box = this->client_manager->get_client_box(request.client_id);

                    if (client_box == nullptr)
                    {
                        throw client_box_not_found_error{};
                    }

                    client_box->set_matrix_resource(request.matrix_resource_vec);

                    return SetMatrixResourceResponse
                    {
                        .result = SUCCESS,
                        .err_verbal_description = ""
                    };
                }
                catch (...)
                {
                    return SetMatrixResourceResponse
                    {
                        .result = to_local_exception_error_code(std::current_exception()),
                        .err_verbal_description = verbose_exception(std::current_exception())
                    };
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
                try
                {
                    std::shared_ptr<ConnectionBoundClientBox> client_box = this->client_manager->get_client_box(request.client_id);

                    if (client_box == nullptr)
                    {
                        throw client_box_not_found_error{};
                    }

                    return GetDeviationResponse
                    {
                        .result = client_box->get(),
                        .err_verbal_description = ""
                    };
                }
                catch (...)
                {
                    return GetDeviationResponse
                    {
                        .result = std::unexpected(to_local_exception_error_code(std::current_exception())),
                        .err_verbal_description = verbose_exception(std::current_exception())
                    };
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
                try
                {
                    std::shared_ptr<ConnectionBoundClientBox> client_box = this->client_manager->get_client_box(request.client_id);

                    if (client_box == nullptr)
                    {
                        throw client_box_not_found_error{};
                    }

                    client_box->set_matrix_resource(request.matrix_resource_vec);

                    return SetAndGetDeviationResponse
                    {
                        .result = client_box->get(),
                        .err_verbal_description = ""
                    };
                }
                catch (...)
                {
                    return SetAndGetDeviationResponse
                    {
                        .result = std::unexpected(to_local_exception_error_code(std::current_exception())),
                        .err_verbal_description = verbose_exception(std::current_exception())
                    };
                }
            }
    };
}

#endif