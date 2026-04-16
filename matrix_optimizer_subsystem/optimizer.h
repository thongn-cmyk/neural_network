#ifndef __MATRIX_OPTIMIZER_SUBSYSTEM_DISTRIBUTED_OPTIMIZER_H__
#define __MATRIX_OPTIMIZER_SUBSYSTEM_DISTRIBUTED_OPTIMIZER_H__

#include <stdint.h>
#include <stdlib.h>
#include <matrix/generic_matrix_factory.h>
#include <matrix_optimizer_subsystem/generic_optimizer_engine.h>
#include <matrix_evaluator/matrix_evaluator_interface.h>

namespace  matrix_optimizer_subsystem
{
    class Optimizer
    {
        private:

            using self = Optimizer;

            std::optional<generic_matrix_factory::ExternalGenericMatrixResource> matrix_resource;
            std::shared_ptr<matrix_evaluator::MatrixEvaluatorInterface> matrix_evaluator;
            std::optional<ExternalGenericOptimizerEngineConfig> optimizer_engine_config;

        public:

            Optimizer(): matrix_resource(std::nullopt),
                         matrix_evaluator(nullptr),
                         optimizer_engine_config(std::nullopt){}

            auto set_matrix(const generic_matrix_factory::ExternalGenericMatrixResource& matrix_resource) -> self&
            {
                this->matrix_resource = matrix_resource;

                return *this;
            }

            auto set_matrix_evaluator(const std::shared_ptr<matrix_evaluator::MatrixEvaluatorInterface>& matrix_evaluator) -> self&
            {
                this->matrix_evaluator = matrix_evaluator;

                return *this;
            }

            auto set_optimization_config(const ExternalGenericOptimizerEngineConfig& optimizer_engine_config) -> self&
            {
                this->optimizer_engine_config = optimizer_engine_config;

                return *this;
            }

            auto optimize(common_exception::CancellationTokenInterface& cancellation_token) -> generic_matrix_factory::ExternalGenericMatrixResource
            {
                if (!this->matrix_resource.has_value())
                {
                    throw std::invalid_argument("bad matrix resource, null");
                }

                if (this->matrix_evaluator == nullptr)
                {
                    throw std::invalid_argument("bad matrix evaluator, null");
                }

                if (!this->optimizer_engine_config.has_value())
                {
                    throw std::invalid_argument("bad optimizer engine config, null");
                }

                generic_matrix_factory::GenericMatrixResource internal_resource = generic_matrix_factory::ExternalGenericMatrixFactory{}.to_internal(this->matrix_resource.value());
                std::unique_ptr<the_matrix::MatrixInterface> matrix             = generic_matrix_factory::GenericMatrixLoader{}.load_resource(internal_resource);
                std::shared_ptr<the_matrix::MatrixInterface> optimized_matrix   = GenericOptimizerEngine(this->optimizer_engine_config.value()).optimize(*matrix,
                                                                                                                                                         *this->matrix_evaluator,
                                                                                                                                                         cancellation_token);

                matrix->set_coefficient_vector(optimized_matrix->get_coefficient_vector());

                return generic_matrix_factory::ExternalGenericMatrixFactory{}.to_external(generic_matrix_factory::GenericMatrixLoader{}.unload(*matrix));

            }
    };
}

#endif