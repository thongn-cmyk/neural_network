#ifndef __MONEY_SOLUTION_SOLUTION_SERVER_CONTROLLER_H__
#define __MONEY_SOLUTION_SOLUTION_SERVER_CONTROLLER_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>

#include <request_extension/type_based_resolutor_interface.h>
#include <request_extension/type_based_dgstd_resolutor.h>

#include "model.h"
#include <mutex_extension/fair_mutex.h>
#include "local_exception.h"
#include "client_box.h"

#include <atomic>

namespace stock_solution_server
{
    //I've been fighting entropy and the theories for most of my time researching, I know my odds better if trained on milliseconds-level
    //essentially we'd need to have more data than the market can provide to not memorize the charts but the compression instead

    //so if we actually train this neural network on a massive 2020-2026 milliseconds - seconds 128KB - 1MB territory, we'd be profitable
    //of course if we glue all the tickers inside one matrix

    //I've been thinking about level-blocked of neural matrix training, so essentially we'd begin with 1 + 1 -> 1 and moving 2 4 8 coefficient bases
    //essentially 3 different coefficient vectors with "2 first 4, 8 blocked" ->  "2 4 first, 8 blocked" -> "2 4 8 first"

    //I understand that has to be part of the equations, the blocks the ReLU and the "inventional randomness of the wild"
    //because the otherwise is a complete disaster of implementation and interfaces

    //imagine like we have the unlock variables 0.1 -> 0.01 -> 0.001 -> 0.0001 according to the powers (or entropy) of the coefficients
    //if we hit the jackpot, we'd unlock the level there forward, otherwise we'd keep playing with statistics and uniform distribution
    //so it's not ReLU, but you can say that it's rectified linear unit

    //I've done extensive research about almost every decisions written here
    //and it just seems that this is the way

    static inline constexpr std::string_view STOCK_SOLUTION_SERVER_VERSION_CONTROL  = "";

    template <class T_In, class T_Out>
    using TypeBasedResolutorInterface   = request_extension::resolutor::TypeBasedResolutorInterface<T_In, T_Out>;

    class GetVersionResolver: public virtual TypeBasedResolutorInterface<GetVersionRequest, GetVersionResponse>
    {
        public:

            static inline constexpr std::string_view RESOLVABLE_PATH    = "stock_solution_server/get_version";

            auto handle(const GetVersionRequest& request) -> GetVersionResponse
            {
                (void) request;

                return GetVersionResponse
                {
                    .response   = std::string(STOCK_SOLUTION_SERVER_VERSION_CONTROL),
                    .err_verbal_description = {}
                };
            }
    };

    class OpenClientResolver: public virtual TypeBasedResolutorInterface<OpenClientRequest, OpenClientResponse>
    {
        private:

            std::shared_ptr<ClientBoxManager> client_box_manager;

        public:

            static inline constexpr std::string_view RESOLVABLE_PATH    = "stock_solution_server/open_client";

            OpenClientResolver(std::shared_ptr<ClientBoxManager> client_box_manager) noexcept: client_box_manager(std::move(client_box_manager)){}

            auto handle(const OpenClientRequest& request) -> OpenClientResponse
            {
                try
                {
                    uint64_t client_box_id  = this->client_box_manager->open_client_box(request.connection_config);

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
                        .result = std::unexpected(stock_solution_server::to_local_exception_error_code(std::current_exception())),
                        .err_verbal_description = stock_solution_server::verbose_exception(std::current_exception())
                    };
                }
            }
    };

    class CloseClientResolver: public virtual TypeBasedResolutorInterface<CloseClientRequest, CloseClientResponse>
    {
        private:

            std::shared_ptr<ClientBoxManager> client_box_manager;

        public:

            static inline constexpr std::string_view RESOLVABLE_PATH    = "stock_solution_server/close_client";

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
                        .result = stock_solution_server::to_local_exception_error_code(std::current_exception()),
                        .err_verbal_description = stock_solution_server::verbose_exception(std::current_exception())
                    };
                }
            }
    };

    class SetSolutionResolver: public virtual TypeBasedResolutorInterface<SetSolutionRequest, SetSolutionResponse>
    {
        private:

            std::shared_ptr<ClientBoxManager> client_box_manager;

        public:

            static inline constexpr std::string_view RESOLVABLE_PATH    = "stock_solution_server/set_solution";

            SetSolutionResolver(std::shared_ptr<ClientBoxManager> client_box_manager) noexcept: client_box_manager(std::move(client_box_manager)){}

            auto handle(const SetSolutionRequest& request) -> SetSolutionResponse
            {
                try
                {
                    std::shared_ptr<ConnectionBoundClientBox> client_box = this->client_box_manager->get_client_box(request.client_box_id);

                    if (client_box == nullptr)
                    {
                        throw client_box_not_found_error{};
                    }

                    client_box->set_solution(request.solution_data);

                    return SetSolutionResponse
                    {
                        .result = SUCCESS,
                        .err_verbal_description = ""
                    };
                }
                catch (...)
                {
                    return SetSolutionResponse
                    {
                        .result = stock_solution_server::to_local_exception_error_code(std::current_exception()),
                        .err_verbal_description = stock_solution_server::verbose_exception(std::current_exception())
                    };
                }
            }
    };

    class GetRecommendationResolver: public virtual TypeBasedResolutorInterface<GetRecommendationRequest, GetRecommendationResponse>
    {
        private:

            std::shared_ptr<ClientBoxManager> client_box_manager;

        public:

            static inline constexpr std::string_view RESOLVABLE_PATH    = "stock_solution_server/get_recommendation";

            GetRecommendationResolver(std::shared_ptr<ClientBoxManager> client_box_manager) noexcept: client_box_manager(std::move(client_box_manager)){}

            auto handle(const GetRecommendationRequest& request) -> GetRecommendationResponse
            {
                try
                {
                    std::shared_ptr<ConnectionBoundClientBox> client_box = this->client_box_manager->get_client_box(request.client_box_id);

                    if (client_box == nullptr)
                    {
                        throw client_box_not_found_error{};
                    }

                    Actionables actionables = client_box->get_recommendation(request.market_data,
                                                                             request.forecast_timepoint,
                                                                             request.top_k);

                    return GetRecommendationResponse
                    {
                        .result = std::move(actionables),
                        .err_verbal_description = ""
                    };
                }
                catch (...)
                {
                    return GetRecommendationResponse
                    {
                        .result = std::unexpected(stock_solution_server::to_local_exception_error_code(std::current_exception())),
                        .err_verbal_description = stock_solution_server::verbose_exception(std::current_exception())
                    };
                }
            }
    };
}

#endif