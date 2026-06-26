#ifndef __MONEY_SOLUTION_SOLUTION_TRAINER_SERVER_SOLUTION_BUILDER_H__
#define __MONEY_SOLUTION_SOLUTION_TRAINER_SERVER_SOLUTION_BUILDER_H__

#include <stdint.h>
#include <stdlib.h>
#include <expected>
#include <optional>
#include <string>
#include <data_loader/source_loader/multisource_loader/multisource_loader.h>
#include <money/stock_solution.h>
#include <common_exception/cancellation_token.h>
#include <concurrency_utility/concurrency_utility.h>
#include <chrono>
#include <memory>
#include <concurrency_detachable_task/detachable_task_handle_interface.h>
#include <vector>
#include <matrix_broker_client/matrix_broker_client.h>
#include <stl_extension/stdx.h>
#include <serializer/compact_serializer.h>
#include <data_loader/hex_encoder/hex_encoder.h>
#include <fire_bandwidth_control/generic_firer.h>
#include <fire_bandwidth_control/config_builder.h>
#include <internal_rest/network_rest_frame.h>
#include <global_config/rest_config.h>
#include <stl_extension/semantic_mapper.h>

namespace stock_solution_builder
{
    using Remote = dg_sock::network_rest_frame::model::Remote;

    struct ResourceBase
    {
        struct ComputeSink
        {
            Remote remote;
            fire_bandwidth_control::generic_firer::ExternalGenericFirerConfig firer_config;
        };

        std::shared_ptr<common_exception::CancellationTokenInterface> cancellation_token;
        std::optional<data_loader::source_loader::multisource_loader::ExternalMultisourceLoaderConfig> data_loader_config;
        std::vector<ComputeSink> compute_sink_vec;

        std::optional<std::chrono::time_point<std::chrono::utc_clock>> from_timepoint;
        std::optional<std::chrono::time_point<std::chrono::utc_clock>> to_timepoint;

        uint8_t optimization_flag;

        static inline constexpr uint8_t OPTIMIZATION_FLAG_O1    = 0u;
        static inline constexpr uint8_t OPTIMIZATION_FLAG_O2    = 1u;
        static inline constexpr uint8_t OPTIMIZATION_FLAG_O3    = 2u;
    };

    template <class T>
    using TaskPromise   = concurrency_detachable_task::DetachableTaskHandleInterface<T>;

    template <class T>
    using RestPromise   = dg_sock::network_rest_frame::client::Promise<T>;

    class ImmutableSolutionBuilder
    {
        private:

            ResourceBase resource_base;

            std::optional<std::vector<stock_solution::TickerData>> ticker_data;
            std::optional<std::vector<std::string>> ticker_vec;
            std::optional<std::vector<std::string>> feature_vec;

            std::optional<std::chrono::time_point<std::chrono::utc_clock>> from_timepoint;
            std::optional<std::chrono::time_point<std::chrono::utc_clock>> to_timepoint;

            static inline constexpr std::chrono::nanoseconds DEFAULT_ITERATION_STEP = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::days(1));

        public:

            ImmutableSolutionBuilder(ResourceBase resource_base): resource_base(std::move(resource_base)){}

            auto build() -> stock_solution::ExternalSolutionData
            {
                auto rs = stock_solution::SolutionBuilder{}.set_focal_option(this->get_focal_option())
                                                           .set_matrix_broker(this->get_matrix_broker())
                                                           .set_matrix_optimizer(this->get_matrix_optimizer())
                                                           .set_cancellation_token(this->get_cancellation_token())
                                                           .set_data(this->get_ticker_data())
                                                           .set_training_first_timepoint(this->get_training_first_timepoint())
                                                           .set_training_last_timepoint(this->get_training_last_timepoint())
                                                           .set_feature_name_list(this->get_feature_name_list())
                                                           .set_tickers(this->get_ticker_vec())
                                                           .build()->wait();

                return stock_solution::to_external_solution_data(rs);
            }

        private:

            class InternalMatrixBroker: public virtual stock_solution::MatrixBrokerInterface
            {
                private:

                    matrix_broker_client::APIClient client;
                    ResourceBase resource_base;

                    static auto get_self_remote() -> Remote
                    {
                        return Remote
                        {
                            .addr       = dg_sock::network_rest_frame::client_instance::address(),
                            .channel    = global_config::rest_config::GENERAL_COMPUTE_CHANNEL
                        };
                    }

                public:

                    InternalMatrixBroker(ResourceBase resource_base): client(get_self_remote()),
                                                                      resource_base(std::move(resource_base)){}

                    auto broke_matrix(size_t flat_matrix_sz,
                                      const std::shared_ptr<common_exception::CancellationTokenInterface>& cancellation_token) -> std::shared_ptr<TaskPromise<stock_solution::MatrixResult>>
                    {
                        this->client.set_cancellation_token(cancellation_token);

                        std::shared_ptr<RestPromise<matrix_broker_client::ClientMatrixResult>> matrix_result = this->client.broke_matrix(this->get_generator_id(),
                                                                                                                                         this->get_matrix_entropy(),
                                                                                                                                         flat_matrix_sz);

                        auto resolutor  = [](const matrix_broker_client::ClientMatrixResult& rs)
                        {
                            return stock_solution::MatrixResult
                            {
                                .matrix         = stdx::to_automap_object(rs.matrix_resource),
                                .matrix_shape   = stdx::to_castable_vector_initializer(std::get<matrix_broker_client::FixedProjectionArgument>(rs.projection_argument.projection_argument).out_matrix_shape)
                            };
                        };

                        return concurrency_utility::task_promise_cast(concurrency_utility::to_shared_promise(concurrency_utility::rest_to_task_promise(matrix_result)),
                                                                      resolutor);
                    }

                private:

                    auto get_generator_id() -> std::string
                    {
                        return "taylor_cuda_matrix";
                    }

                    auto get_matrix_entropy() -> uint8_t
                    {
                        switch (this->resource_base.optimization_flag)
                        {
                            case ResourceBase::OPTIMIZATION_FLAG_O1:
                            {
                                return matrix_broker_client::MATRIX_ENTROPY_LOW;
                            }
                            case ResourceBase::OPTIMIZATION_FLAG_O2:
                            {
                                return matrix_broker_client::MATRIX_ENTROPY_MID;
                            }
                            case ResourceBase::OPTIMIZATION_FLAG_O3:
                            {
                                return matrix_broker_client::MATRIX_ENTROPY_HIGH;
                            }
                            default:
                            {
                                throw std::invalid_argument("bad optimization flag, enumeration out of range");
                            }
                        }
                    }
            };

            class InternalMatrixOptimizationSession: public virtual stock_solution::MatrixOptimizationSessionInterface
            {
                private:

                    ResourceBase resource_base;

                public:

                    InternalMatrixOptimizationSession(ResourceBase resource_base): resource_base(std::move(resource_base)){}

                    auto add_training_data(const std::shared_ptr<tensor_model::Matrix>& inp,
                                           const std::shared_ptr<tensor_model::Matrix>& out,
                                           const std::shared_ptr<common_exception::CancellationTokenInterface>& cancellation_token) -> std::shared_ptr<TaskPromise<stdx::fancy_void>>
                    {
                        return {};
                    }

                    auto optimize(const generic_matrix_factory::ExternalGenericMatrixResource& resource,
                                  const std::shared_ptr<common_exception::CancellationTokenInterface>& cancellation_token) -> std::shared_ptr<TaskPromise<generic_matrix_factory::ExternalGenericMatrixResource>>
                    {
                        return {};
                    }
            };

            class InternalMatrixOptimizationSessionGenerator: public virtual stock_solution::MatrixOptimizationSessionGeneratorInterface
            {
                private:

                    ResourceBase resource_base;

                public:

                    InternalMatrixOptimizationSessionGenerator(ResourceBase resource_base): resource_base(std::move(resource_base)){}

                    auto get_session() -> std::unique_ptr<stock_solution::MatrixOptimizationSessionInterface>
                    {
                        return std::make_unique<InternalMatrixOptimizationSession>(this->resource_base);
                    }
            };

            auto get_focal_option() -> uint8_t
            {
                switch (this->resource_base.optimization_flag)
                {
                    case ResourceBase::OPTIMIZATION_FLAG_O1:
                    {
                        return stock_solution::SolutionBuilder::FOCAL_OPTION_MINUTE;
                    }
                    case ResourceBase::OPTIMIZATION_FLAG_O2:
                    {
                        return stock_solution::SolutionBuilder::FOCAL_OPTION_SECOND_1;
                    }
                    case ResourceBase::OPTIMIZATION_FLAG_O3:
                    {
                        return stock_solution::SolutionBuilder::FOCAL_OPTION_SECOND_0;
                    }
                    default:
                    {
                        throw std::invalid_argument("bad optimization flag, enumeration out of range");
                    }
                }
            }

            auto get_matrix_broker() -> std::unique_ptr<stock_solution::MatrixBrokerInterface>
            {
                return std::make_unique<InternalMatrixBroker>(this->resource_base);
            }

            auto get_matrix_optimizer() -> std::unique_ptr<stock_solution::MatrixOptimizationSessionGeneratorInterface>
            {
                return std::make_unique<InternalMatrixOptimizationSessionGenerator>(this->resource_base);
            }

            auto get_cancellation_token() -> const std::shared_ptr<common_exception::CancellationTokenInterface>&
            {
                return this->resource_base.cancellation_token;
            }

            auto deserialize_token_data(const std::string& token) -> stock_solution::TickerData
            {
                return dg::network_compact_serializer::dgstd_deserialize<stock_solution::TickerData>(data_loader::hex_encoder::hex_decode<std::string>(token));
            }

            auto download_ticker_data() -> std::vector<stock_solution::TickerData>
            {
                using namespace data_loader::source_loader;

                if (!this->resource_base.data_loader_config.has_value())
                {
                    throw std::invalid_argument("bad data loader config, null");
                }

                std::unique_ptr<UserSpaceSourceLoaderInterface> source_loader                       = std::make_unique<multisource_loader::MultisourceLoader>(this->resource_base.data_loader_config.value());
                std::shared_ptr<common_exception::CancellationTokenInterface> cancellation_token    = this->get_cancellation_token();

                if (cancellation_token == nullptr)
                {
                    cancellation_token = std::make_shared<common_exception::CancellationToken>();
                }

                std::vector<stock_solution::TickerData> ticker_data{};

                while (true)
                {
                    std::optional<std::string> token   = source_loader->get(*cancellation_token);

                    if (!token.has_value())
                    {
                        return ticker_data;
                    }

                    ticker_data.push_back(this->deserialize_token_data(token.value()));
                }
            }

            auto get_ticker_data() -> const std::vector<stock_solution::TickerData>&
            {
                if (!this->ticker_data.has_value())
                {
                    this->ticker_data   = this->download_ticker_data();
                }

                return this->ticker_data.value();
            }

            auto get_training_first_timepoint() -> std::chrono::time_point<std::chrono::utc_clock>
            {
                if (!this->from_timepoint.has_value())
                {
                    if (this->resource_base.from_timepoint.has_value())
                    {
                        this->from_timepoint = this->resource_base.from_timepoint.value();
                    }
                    else
                    {
                        std::optional<std::chrono::time_point<std::chrono::utc_clock>> cand = std::nullopt;

                        for (const auto& ticker: this->get_ticker_data())
                        {
                            if (!cand.has_value())
                            {
                                cand = ticker.timestamp;
                            }

                            cand = std::min(cand.value(), ticker.timestamp);
                        }

                        if (!cand.has_value())
                        {
                            throw std::invalid_argument("bad first timepoint, no timepoint avaialble");
                        }

                        this->from_timepoint = cand.value();
                    }
                }

                return this->from_timepoint.value();
            }

            auto get_training_last_timepoint() -> std::chrono::time_point<std::chrono::utc_clock>
            {
                if (!this->to_timepoint.has_value())
                {
                    if (this->resource_base.to_timepoint.has_value())
                    {
                        this->to_timepoint = this->resource_base.to_timepoint.value();
                    }
                    else
                    {
                        std::optional<std::chrono::time_point<std::chrono::utc_clock>> cand = std::nullopt;

                        for (const auto& ticker: this->get_ticker_data())
                        {
                            if (!cand.has_value())
                            {
                                cand = ticker.timestamp;
                            }

                            cand = std::max(cand.value(), ticker.timestamp);
                        }

                        if (!cand.has_value())
                        {
                            throw std::invalid_argument("bad last timepoint, no timepoint available");
                        }

                        this->to_timepoint = cand.value(); //[) bug
                    }
                }
                
                return this->to_timepoint.value();
            }

            auto get_feature_name_list() -> const std::vector<std::string>&
            {
                if (!this->feature_vec.has_value())
                {
                    std::unordered_set<std::string> rs_set{};

                    for (const auto& ticker: this->get_ticker_data())
                    {
                        rs_set.insert(ticker.feature_name);
                    }

                    this->feature_vec = std::vector<std::string>(rs_set.begin(), rs_set.end());
                }

                return this->feature_vec.value();
            }

            auto get_ticker_vec() -> const std::vector<std::string>&
            {
                if (!this->ticker_vec.has_value())
                {
                    std::unordered_set<std::string> rs_set{};

                    for (const auto& ticker: this->get_ticker_data())
                    {
                        rs_set.insert(ticker.ticker_name);
                    }

                    this->ticker_vec = std::vector<std::string>(rs_set.begin(), rs_set.end());
                }

                return this->ticker_vec.value();
            }
    };

    class SolutionBuilder
    {
        private:

            ResourceBase resource_base;

        public:
           
            SolutionBuilder(): resource_base()
            {
                this->resource_base.optimization_flag   = ResourceBase::OPTIMIZATION_FLAG_O1;
            }

            auto set_cancellation_token(const std::shared_ptr<common_exception::CancellationTokenInterface>& cancellation_token_arg) -> SolutionBuilder&
            {
                this->resource_base.cancellation_token  = cancellation_token_arg;

                return *this;
            }

            auto set_data_loader_config(const data_loader::source_loader::multisource_loader::ExternalMultisourceLoaderConfig& data_loader_config_arg) -> SolutionBuilder&
            {
                this->resource_base.data_loader_config  = data_loader_config_arg;

                return *this;
            }

            auto get_default_firer_config() -> fire_bandwidth_control::generic_firer::ExternalGenericFirerConfig
            {
                return fire_bandwidth_control::config_builder::FreeFirerBuilder{}.build();
            }

            auto add_compute_sink(const Remote& remote,
                                  std::optional<fire_bandwidth_control::generic_firer::ExternalGenericFirerConfig> firer_config = std::nullopt) -> SolutionBuilder&
            {
                if (!firer_config.has_value())
                {
                    firer_config = this->get_default_firer_config();
                }

                this->resource_base.compute_sink_vec.push_back
                (
                    ResourceBase::ComputeSink
                    {
                        .remote         = remote,
                        .firer_config   = std::move(firer_config.value())
                    }
                );

                return *this;
            }

            auto set_from(std::chrono::time_point<std::chrono::utc_clock> timepoint) -> SolutionBuilder&
            {
                this->resource_base.from_timepoint = timepoint;

                return *this;
            }

            auto set_to(std::chrono::time_point<std::chrono::utc_clock> timepoint) -> SolutionBuilder&
            {
                this->resource_base.to_timepoint = timepoint;

                return *this;
            }

            auto set_optimization_flag(uint8_t optimization_flag_arg) -> SolutionBuilder&
            {
                switch (optimization_flag_arg)
                {
                    case ResourceBase::OPTIMIZATION_FLAG_O1:
                    case ResourceBase::OPTIMIZATION_FLAG_O2:
                    case ResourceBase::OPTIMIZATION_FLAG_O3:
                    {
                        this->resource_base.optimization_flag = optimization_flag_arg;
                        break;
                    }
                    default:
                    {
                        throw std::invalid_argument("bad optimization flag, enumeration out of range");
                    }
                }

                return *this;
            }

            auto build() -> stock_solution::ExternalSolutionData
            {
                return ImmutableSolutionBuilder(this->resource_base).build();
            }
    };
}

#endif