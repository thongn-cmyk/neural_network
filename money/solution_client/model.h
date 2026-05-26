#ifndef __MONEY_SOLUTION_SOLUTION_CLIENT_MODEL_H__
#define __MONEY_SOLUTION_SOLUTION_CLIENT_MODEL_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <connectivity_subsystem/connectivity_subsystem.h>
#include "local_exception.h"
#include <string>
#include <internal_rest/network_rest_frame.h>
#include <vector>
#include <optional>
#include <chrono>

namespace stock_solution_client
{
    template <class T>
    using Promise       = dg_sock::network_rest_frame::client::Promise<T>;

    using Remote        = dg_sock::network_rest_frame::model::Remote;
    using Url           = dg_sock::network_rest_frame::model::Url;
    using ClientRequest = dg_sock::network_rest_frame::model::ClientRequest;

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
        std::expected<std::string, local_exception_t> response;
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
        std::expected<uint64_t, local_exception_t> result;
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

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(client_box_id);
        }
    };

    struct CloseClientResponse
    {
        local_exception_t result;
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

    //migrations
    struct ExternalSolutionData
    {
        std::string solution_bytestream;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(solution_bytestream);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(solution_bytestream);
        }
    };

    struct SetSolutionRequest
    {
        uint64_t client_box_id;
        ExternalSolutionData solution_data;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(client_box_id, solution_data);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(client_box_id, solution_data);
        }
    };

    struct SetSolutionResponse
    {
        local_exception_t result;
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

    //migrations
    struct TickerData
    {
        std::string ticker_name;
        std::string feature_name;
        double feature_value;
        std::chrono::time_point<std::chrono::utc_clock> timestamp;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(ticker_name,
                      feature_name,
                      feature_value,
                      timestamp);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(ticker_name,
                      feature_name,
                      feature_value,
                      timestamp);
        }
    };

    //migrations
    struct MarketData
    {
        std::vector<TickerData> ticker_data_vec;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(ticker_data_vec);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(ticker_data_vec);
        }
    };

    struct GetRecommendationRequest
    {
        uint64_t client_box_id;
        MarketData market_data;
        std::chrono::time_point<std::chrono::utc_clock> forecast_timepoint;
        std::optional<uint32_t> top_k;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(client_box_id,
                      market_data,
                      forecast_timepoint,
                      top_k);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(client_box_id,
                      market_data,
                      forecast_timepoint,
                      top_k);
        }
    };

    struct Actionable
    {
        std::string ticker_name;
        double norm_confident_score;
        bool bull_flag;
        bool bear_flag;
        std::optional<std::chrono::time_point<std::chrono::utc_clock>> guaranteed_timepoint;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(ticker_name,
                      norm_confident_score,
                      bull_flag,
                      bear_flag,
                      guaranteed_timepoint);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(ticker_name,
                      norm_confident_score,
                      bull_flag,
                      bear_flag,
                      guaranteed_timepoint);
        }
    };

    struct Actionables
    {
        std::vector<Actionable> actionable_vec;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(actionable_vec);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(actionable_vec);
        }
    };

    struct GetRecommendationResponse
    {
        std::expected<Actionables, local_exception_t> result;
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

    struct GetRecommendationPayload
    {
        MarketData market_data;
        std::chrono::time_point<std::chrono::utc_clock> forecast_timepoint;
        std::optional<uint32_t> top_k;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(market_data,
                      forecast_timepoint,
                      top_k);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(market_data,
                      forecast_timepoint,
                      top_k);
        }
    };
}

#endif