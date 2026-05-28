#ifndef __DEVIATION_PROJECTION_MATRIX_EVALUATOR_H__
#define __DEVIATION_PROJECTION_MATRIX_EVALUATOR_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <matrix_evaluator/matrix_evaluator_interface.h>
#include <deviation_projection_client/deviation_projection_client.h>
#include <deviation_projector/generic_matrix_as_deviation_wrapper.h>
#include <common_exception/cancellation_token.h>
#include <vector>
#include <matrix/generic_matrix_factory.h>
#include <general_definition/float_def.h>
#include <internal_rest/network_rest_frame.h>
#include <limits.h>
#include <serializer/compact_serializer.h>
#include <chrono>
#include <connectivity_subsystem/connectivity_subsystem.h>
#include <common_exception/common_exception.h>
#include <stl_extension/semantic_mapper.h>

namespace deviation_projection_matrix_evaluator
{
    using namespace float_def;

    template <class T>
    using Promise = dg_sock::network_rest_frame::client::Promise<T>;

    using ClientRemote = deviation_projection_client::ClientRemote;

    class MatrixEvaluatorBuilder
    {
        private:

            std::optional<std::vector<ClientRemote>> client_remote_vec;
            std::optional<generic_matrix_factory::ExternalGenericMatrixResource> exportable_matrix;
            std::optional<deviation_projector::ExternalGenericMatrixAsDeviationWrapperConfig> deviation_wrapper_config;
            std::shared_ptr<common_exception::CancellationTokenInterface> cancellation_token;
            dg_sock::network_rest_frame::client::retry_policy_t retry_policy;
            connectivity_subsystem::MasterConfiguration connection_config;

            using self = MatrixEvaluatorBuilder;

            static auto get_default_connection_config() -> connectivity_subsystem::MasterConfiguration
            {
                return connectivity_subsystem::MasterConnection::default_master_configuration();
            }

        public:

            MatrixEvaluatorBuilder(): client_remote_vec(std::nullopt),
                                      exportable_matrix(std::nullopt),
                                      deviation_wrapper_config(std::nullopt),
                                      cancellation_token(nullptr),
                                      retry_policy(dg_sock::network_rest_frame::client::RequestRetryMachineFactory<>::EXPONENTIAL_HARD),
                                      connection_config(get_default_connection_config()){}

            auto set_client_remote(const std::vector<ClientRemote>& client_remote_vec) -> self&
            {
                this->client_remote_vec = client_remote_vec;

                return *this;
            }

            auto set_exportable_matrix(const generic_matrix_factory::ExternalGenericMatrixResource& exportable_matrix)
            {
                this->exportable_matrix = exportable_matrix;

                return *this;
            }

            auto set_matrix_deviation_wrapper(const deviation_projector::ExternalGenericMatrixAsDeviationWrapperConfig& deviation_wrapper_config) -> self&
            {
                this->deviation_wrapper_config = deviation_wrapper_config;

                return *this;
            }

            auto set_cancellation_token(const std::shared_ptr<common_exception::CancellationTokenInterface>& cancellation_token) -> self&
            {
                this->cancellation_token = cancellation_token;

                return *this;
            }

            auto set_retry_policy(const dg_sock::network_rest_frame::client::retry_policy_t retry_policy) -> self&
            {
                this->retry_policy = retry_policy;

                return *this;
            }

            auto set_connection_config(const connectivity_subsystem::MasterConfiguration& connection_config) -> self&
            {
                this->connection_config = connection_config;

                return *this;
            }

            auto build() -> std::unique_ptr<matrix_evaluator::MatrixEvaluatorInterface>
            {
                if (!this->client_remote_vec.has_value())
                {
                    throw std::invalid_argument("bad client remote vec, null");
                }

                if (!this->exportable_matrix.has_value())
                {
                    throw std::invalid_argument("bad exportable matrix, null");
                }

                if (!this->deviation_wrapper_config.has_value())
                {
                    throw std::invalid_argument("bad deviation wrapper config, null");
                }

                if (this->client_remote_vec->empty())
                {
                    throw std::invalid_argument("bad client remote vec, empty");
                }

                std::vector<std::unique_ptr<deviation_projection_client::NoOwned_APIClient>> api_client_vec{};

                for (const auto& client_remote: this->client_remote_vec.value())
                {
                    std::unique_ptr<deviation_projection_client::NoOwned_APIClient> api_client = std::make_unique<deviation_projection_client::NoOwned_APIClient>(client_remote);

                    api_client->set_unique_request(true);
                    api_client->set_retry_policy(this->retry_policy);
                    api_client->set_cancellation_token(this->cancellation_token);

                    api_client_vec.push_back(std::move(api_client));
                }

                return std::make_unique<MatrixEvaluator>(std::move(api_client_vec),
                                                         this->exportable_matrix.value(),
                                                         this->deviation_wrapper_config.value());
            }

        private:

            class MatrixEvaluator: public virtual matrix_evaluator::MatrixEvaluatorInterface
            {
                private:

                    std::vector<std::unique_ptr<deviation_projection_client::NoOwned_APIClient>> api_client_vec;
                    generic_matrix_factory::ExternalGenericMatrixResource exportable_matrix;
                    deviation_projector::ExternalGenericMatrixAsDeviationWrapperConfig matrix_deviation_wrapper_config;

                public:

                    MatrixEvaluator(std::vector<std::unique_ptr<deviation_projection_client::NoOwned_APIClient>> api_client_vec,
                                    generic_matrix_factory::ExternalGenericMatrixResource exportable_matrix,
                                    deviation_projector::ExternalGenericMatrixAsDeviationWrapperConfig matrix_deviation_wrapper_config) noexcept: api_client_vec(std::move(api_client_vec)),
                                                                                                                                                  exportable_matrix(std::move(exportable_matrix)),
                                                                                                                                                  matrix_deviation_wrapper_config(std::move(matrix_deviation_wrapper_config)){}

                    auto get_deviation(the_matrix::MatrixInterface& matrix) -> eval_float_t
                    {
                        deviation_projector::ExternalGenericMatrixDeviationCalculatorResource deviation_resource = this->get_deviation_resource(matrix);
                        std::vector<mdc_float_t> rs = this->get_deviation_from_client_vec(deviation_resource);

                        return this->reduce_deviation(rs);
                    }

                private:

                    auto get_deviation_resource(the_matrix::MatrixInterface& matrix) -> deviation_projector::ExternalGenericMatrixDeviationCalculatorResource
                    {
                        using namespace generic_matrix_factory;

                        std::unique_ptr<the_matrix::MatrixInterface> mutable_matrix = GenericMatrixLoader{}.load_resource(GenericMatrixExternalizer{}.to_internal(this->exportable_matrix));
                        mutable_matrix->set_coefficient_vector(matrix.get_coefficient_vector());
                        generic_matrix_factory::ExternalGenericMatrixResource deviation_matrix  = GenericMatrixExternalizer{}.to_external(GenericMatrixLoader{}.unload(*mutable_matrix));

                        return deviation_projector::GenericMatrixAsDeviationWrapper(this->matrix_deviation_wrapper_config).wrap(deviation_matrix);
                    }

                    auto get_deviation_from_client_vec(const deviation_projector::ExternalGenericMatrixDeviationCalculatorResource& deviation_resource) -> std::vector<mdc_float_t>
                    {
                        std::vector<std::shared_ptr<Promise<std::vector<mdc_float_t>>>> promise_vec{};

                        for (const auto& api_client: this->api_client_vec)
                        {
                            promise_vec.push_back(api_client->set_and_get_deviation(stdx::to_automap_object(std::vector<deviation_projector::ExternalGenericMatrixDeviationCalculatorResource>{deviation_resource})));
                        }

                        std::vector<mdc_float_t> rs{};

                        for (const auto& promise: promise_vec)
                        {
                            std::vector<mdc_float_t> tmp = promise->wait();
                            rs.insert(rs.end(), tmp.begin(), tmp.end());
                        }

                        return rs;
                    }

                    auto reduce_deviation(const std::vector<mdc_float_t>& deviation_vec) -> eval_float_t
                    {
                        if (deviation_vec.size() == 0u)
                        {
                            throw std::invalid_argument("bad deviation reduction operation, array size of 0");
                        }

                        eval_float_t rs = 0;

                        for (const auto& e: deviation_vec)
                        {
                            rs += e;
                        }

                        return rs / static_cast<double>(deviation_vec.size());
                    }
            };
    };
}

#endif