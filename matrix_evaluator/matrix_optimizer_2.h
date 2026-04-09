#ifndef __MATRIX_OPTIMIZER_2_H__
#define __MATRIX_OPTIMIZER_2_H__

#include <stdint.h>
#include <stdlib.h>
#include "matrix_deviation_calculator.h"
#include "projection_aid_subsystem.h"
#include "float_def.h"

namespace matrix_optimizer_2
{
    using namespace float_def;

    class GenericMatrixResourceDeviationCalculatorInterface
    {
        public:

            virtual ~GenericMatrixResourceDeviationCalculatorInterface() = default;

            virtual auto get_deviation(const generic_matrix_factory::GenericMatrixResource& resource) -> mdc_float_t
    };

    template <class FloatType = std_float_t>
    class ParallelDeviationCalculator: public virtual cron_subsystem::UpdatableInterface,
                                       public virtual GenericMatrixResourceDeviationCalculatorInterface
    {
        private:

            struct WaitingResource
            {
                generic_matrix_factory::GenericMatrixResource waiting_resource;
                FloatType * result;
                std::exception_ptr * exception;
                std::binary_semaphore * smp;
            };

            std::unique_ptr<fair_mutex::fair_atomic_flag> mtx;
            std::unique_ptr<projection_aid_subsystem::TrainingSession> session;
            std::vector<WaitingResource> resource_vec;

        public:

            static_assert(std::is_floating_point_v<FloatType>);

            ParallelDeviationCalculator(std::unique_ptr<projection_aid_subsystem::TrainingSession>&& session)
            {
                if (session == nullptr)
                {
                    throw std::invalid_argument("bad training session, null session");
                }

                this->mtx           = fair_mutex::make_unique_fair_atomic_flag();
                this->resource_vec  = {};
                this->session       = std::move(session);
            }

            auto get(const generic_matrix_factory::GenericMatrixResource& resource) -> mdc_float_t
            {
                std::binary_semaphore smp(0);
                FloatType result{};
                std::exception_ptr exception{};

                {
                    fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                    this->resource_vec.push_back
                    (
                        WaitingResource
                        {
                            .waiting_resource   = resource,
                            .result             = &result,
                            .exception          = &exception,
                            .smp                = &smp
                        }
                    );
                }

                smp.acquire();

                if (exception != nullptr)
                {
                    std::rethrow_exception(exception);
                }

                return result;
            }

            void update()
            {
                fair_mutex::xlock_guard<fair_mutex::fair_atomic_flag> lck_grd(*this->mtx);

                try
                {
                    std::vector<generic_matrix_factory::GenericMatrixResource> flat_resource_vec{};

                    for (const auto& e: this->resource_vec)
                    {
                        flat_resource_vec.push_back(e.waiting_resource);
                    }

                    std::vector<FloatType> result = this->session->set_matrix_resource(flat_resource_vec).template get<FloatType>();

                    for (size_t i = 0u; i < result.size(); ++i)
                    {
                        *this->reosurce_vec[i].result       = result[i];
                        *this->resource_vec[i].exception    = nullptr;

                        this->resource_vec[i].smp->release();
                    }

                    this->resource_vec.clear();
                }
                catch (...)
                {
                    for (const auto& e: this->resource_vec)
                    {
                        *e.exception = std::current_exception();
                        e.smp->release();
                    }

                    this->resource_vec.clear();
                }
            }
    };

    template <class FloatType = std_float_t>
    class SelfObservedParallelDeviationCalculator: public virtual GenericMatrixResourceDeviationCalculatorInterface
    {
        private:

            std::shared_ptr<ParallelDeviationCalculator<FloatType>> base;
            std::shared_ptr<void> cron_resource;

        public:

            SelfObservedParallelDeviationCalculator(std::unique_ptr<projection_aid_subsystem::TrainingSession> session,
                                                    std::chrono::nanoseconds cron_duration)
            {
                this->base          = std::make_shared<ParallelDeviationCalculator>(std::move(session));
                this->cron_reosurce = cron_subsystem::register_periodic_cronjob(this->base, cron_duration);
            }

            auto get(const generic_matrix_factory::GenericMatrixResource& resource) -> mdc_float_t
            {
                return this->base->get(resource);
            }
    };

    class LocalTimeMachine: public virtual time_machine::TimeMachineInterface
    {
        private:

            std::shared_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface> projector;
            std::shared_ptr<GenericMatrixResourceDeviationCalculatorInterface> deviation_calculator;
            generic_matrix_factory::GenericMatrixResource matrix_resource;

        public:

            LocalTimeMachine(std::shared_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface> projector,
                             std::shared_ptr<GenericMatrixResourceDeviationCalculatorInterface> deviation_calculator,
                             generic_matrix_factory::GenericMatrixResource matrix_resource)
            {
                if (projector == nullptr)
                {
                    throw std::invalid_argument("bad projector, null");
                }

                if (deviation_calculator == nullptr)
                {
                    throw std::invalid_argument("bad deviation calculator, null");
                }

                this->projector             = std::move(projector);
                this->deviation_calculator  = std::move(deviation_calculator);
                this->matrix_resource       = std::move(matrix_resource);
            }

            auto f(std_float_t t) -> tm_float_t
            {
                std::vector<std_float_t> coefficient_vec            = this->projector->project(t);
                std::unique_ptr<the_matrix::MatrixInterface> matrix = generic_matrix_factory::GenericMatrixLoader{}.load_resource(this->matrix_resource);

                matrix->set_coefficient_vector(stdx::to_castable_vector_initializer(coefficient_vec));

                return this->deviation_calculator->get(generic_matrix_factory::GenericMatrixLoader{}.unload(matrix));
            }
    };

    class ParallelTemporalCoefficientOptimizer
    {
        public:

            ParallelTemporalCoefficientOptimizer(){}

            auto optimize() -> std::vector<std::vector<>>
            {

            }
    };
}

#endif