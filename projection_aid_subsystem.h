#ifndef __PROJECTION_AID_SUBSYSTEM_H__
#define __PROJECTION_AID_SUBSYSTEM_H__

#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <unordered_map>

namespace projection_aid_subsystem
{
    class TrainingDataIterableInterface
    {
        public:

            virtual ~TrainingDataIterableInterface() = default;
            virtual auto next() -> std::pair<std::shared_ptr<tensor_model::Matrix>, std::shared_ptr<tensor_model::Matrix>> = 0;
            virtual auto has_next() -> bool = 0;
    };

    class TrainingSession
    {
        public:

            auto set_matrix_resource(const std::vector<generic_matrix_factory::GenericMatrixResource>& matrix_resource_vec) -> TrainingSession&
            {
                return *this;
            }

            template <class FloatType = float_def::std_float_t>
            auto get() -> std::vector<FloatType>
            {
                return {};
            }
    };

    class TrainingSessionFactory
    {
        public:

            static inline constexpr uint8_t RETRY_POLICY_MINIMUM_EFFORT = 0u;
            static inline constexpr uint8_t RETRY_POLICY_MEDIUM_EFFOT   = 1u;
            static inline constexpr uint8_t RETRY_POLICY_MAXIMUM_EFFORT = 2u;

        public:

            auto set_matrix_deviation_calculator(generic_matrix_deviation_calculator::matrix_deviation_calculator_t calculator_kind) -> TrainingSessionFactory&
            {
                return *this;
            }

            auto set_matrix_deviation_reducer(generic_matrix_deviation_calculator::matrix_deviation_reducer_t reducer_kind) -> TrainingSessionFactory&
            {
                return *this;
            }

            auto set_remote(const std::vector<std::string>& ipv6_addr_vec) -> TrainingSessionFactory&
            {
                return *this;
            }

            auto set_training_data(const std::shared_ptr<TrainingDataIterableInterface>& training_data_iterable) -> TrainingSessionFactory&
            {
                return *this;
            }

            auto set_concurrency_size(size_t sz) -> TrainingSessionFactory&
            {
                return *this;
            }

            auto set_retry_policy(uint8_t retry_policy) -> TrainingSessionFactory&
            {
                return *this;
            }

            auto get() -> std::unique_ptr<TrainingSession>
            {
                return {};
            }
    };
}

#endif