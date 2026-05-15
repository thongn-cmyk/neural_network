#ifndef __COSINE_RECOMMENDER_MACHINE_X_H__
#define __COSINE_RECOMMENDER_MACHINE_X_H__

#include <stdint.h>
#include <stdlib.h>
#include <utility>
#include <algorithm>
#include <vector>
#include <stdexcept>
#include "conventional_randomizer.h"
#include "space_operation.h"
#include "activation.h"
#include "branch_optimizer.h"
#include "coefficient_randomizer.h"
#include "cosine_recommender_machine.h"
#include "coordinate_recommender_machine.h"

namespace cosine_recommender_machine_x
{
    using crm_x_float_t             = double;
    using crm_x_promoted_float_t    = long double;

    //i've been thinking, this is not good enough for the unit randomization, we'd need to invest in the branch and the hierarchy of the branch to punch through the best decisions with less than the compute capacity or statistical capacity that we have
    //this has to involve multistep prediction, such is bad -> bad -> bad -> focal_cosine_left -> focal_cosine_right -> focal_cosine_middle, etc.
    //                                                  good -> good -> good -> focal_cosine_one -> focal_cosine_two, etc.

    //we'd need to specify what we want to branch
    //this is way too harder than we anticipated

    //but in the time of one step forward, where we could not find the next converging number, this proved very useful
    //

    class EnumerableCosineProjectorInterface
    {
        public:

            virtual ~EnumerableCosineProjectorInterface() noexcept = default;

            virtual auto cosine_like(const std::vector<size_t>& enumeration_vec,
                                     const std::vector<crm_x_float_t>& org_vec) -> std::vector<crm_x_float_t> = 0;

            virtual auto get_enumeration_prefix_tree() -> std::vector<size_t> = 0;
    };

    class VectorRandomizerMachineInterface
    {
        public:

            virtual ~VectorRandomizerMachineInterface() noexcept = default;

            virtual auto randomize(size_t sz) -> std::vector<crm_x_float_t> = 0;
    };

    class CosineRecommendationResultInterface
    {
        public:

            virtual ~CosineRecommendationResultInterface() = default;

            virtual auto get() -> std::vector<crm_x_float_t> = 0;
            virtual void feedback(crm_x_float_t score) = 0;
    };

    class CosineRecommenderMachineInterface
    {
        public:

            virtual ~CosineRecommenderMachineInterface() = default;

            virtual auto next() -> std::unique_ptr<CosineRecommendationResultInterface> = 0;
            virtual auto space_size() -> size_t = 0;
    };

    class ChanceMachine
    {
        private:

            size_t dice_sz;
            size_t dice_chance;
            conventional_randomizer::RandomizerObject randomizer;

        public:

            ChanceMachine(size_t dice_sz,
                          size_t dice_chance): dice_sz(stdx::safe_non_zero_access(dice_sz)),
                                               dice_chance(dice_chance),
                                               randomizer(){}

            bool flip_a_coin()
            {
                size_t dice_result = this->randomizer.randomize_uint(0u, this->dice_sz);
                return dice_result < this->dice_chance;
            }
    };

    class DecisiveCosineProjector: public virtual EnumerableCosineProjectorInterface
    {
        private:

            static inline const std::vector<std::pair<std::string, std::string>> DECISION_VEC = 
            {
                {"origin", "uniform_cosine"},
                {"origin", "focal_cosine"},

                {"uniform_cosine", "uniform_cosine_one"},
                {"uniform_cosine", "uniform_cosine_two"},
                {"uniform_cosine", "uniform_cosine_all"},

                {"focal_cosine", "focal_cosine_1"},
                {"focal_cosine", "focal_cosine_2"},

                {"focal_cosine_1", "focal_cosine_1_one"},
                {"focal_cosine_1", "focal_cosine_1_two"},
                {"focal_cosine_1", "focal_cosine_1_all"},

                {"focal_cosine_2", "focal_cosine_2_one"},
                {"focal_cosine_2", "focal_cosine_2_two"},
                {"focal_cosine_2", "focal_cosine_2_all"}
            };

            static inline const std::string ORIGIN = "origin";

            conventional_randomizer::ApplicationRandomizerObject focal_randomizer;
            conventional_randomizer::RandomizerObject randomizer;

        public:

            auto cosine_like(const std::vector<size_t>& enumeration_vec,
                             const std::vector<crm_x_float_t>& org_vec) -> std::vector<crm_x_float_t>
            {
                if (enumeration_vec.empty())
                {
                    throw std::invalid_argument("bad enumeration, short enumeration tree");
                }

                size_t enum_value       = enumeration_vec.front();
                const size_t CHOICE_SZ  = 2u; 

                if (enum_value >= CHOICE_SZ)
                {
                    throw std::invalid_argument("bad enumeration, invalid enumeration");                    
                }

                auto successor          = std::vector<size_t>(std::next(enumeration_vec.begin()), enumeration_vec.end());

                switch (enum_value)
                {
                    case 0:
                    {
                        return this->uniform_cosine(successor, org_vec);
                    }
                    case 1:
                    {
                        return this->focal_cosine(successor, org_vec);
                    }
                    default:
                    {
                        std::unreachable();
                    }
                }
            }

            auto get_enumeration_prefix_tree() -> std::vector<size_t>
            {
                static const std::vector<size_t> prefix_tree = make_prefix_tree_from_decision_vec(DECISION_VEC, ORIGIN);

                return prefix_tree;
            }

        private:

            static void preorder_trace(const std::unordered_map<std::string, std::vector<std::string>>& graph,
                                       const std::string& origin,
                                       std::vector<size_t>& enumeration_vec)
            {
                auto map_ptr = graph.find(origin);

                if (map_ptr == graph.end())
                {
                    enumeration_vec.push_back(0u);
                    return;
                }

                enumeration_vec.push_back(map_ptr->second.size());

                for (const std::string& org: map_ptr->second)
                {
                    preorder_trace(graph, org, enumeration_vec);
                }
            }

            static auto make_prefix_tree_from_decision_vec(const std::vector<std::pair<std::string, std::string>>& edge_vec,
                                                           const std::string& origin) -> std::vector<size_t>
            {
                std::unordered_map<std::string, std::vector<std::string>> graph{};

                for (const auto& [src, dst]: edge_vec)
                {
                    graph[src].push_back(dst);
                }

                std::vector<size_t> enumeration_vec{};
                preorder_trace(graph, origin, enumeration_vec);

                return enumeration_vec;
            }

            //actually we cant take the convenience of the pattern to build a function pointer loops, so it's better to write this way

            auto uniform_cosine_one(const std::vector<size_t>& enumeration_vec,
                                    const std::vector<crm_x_float_t>& vec) -> std::vector<crm_x_float_t>
            {
                if (!enumeration_vec.empty())
                {
                    throw std::invalid_argument("bad enumeration, redundant enumeration tree");
                }

                if (vec.empty())
                {
                    return vec;
                }

                const crm_x_float_t ANGLE_DEVIATION = 0.001; //
                const size_t DISCRETIZATION_SZ      = 1'000'000'000ULL;
                size_t idx                          = this->randomizer.randomize_uint(0, vec.size());

                auto tmp_vec    = vec;
                tmp_vec[idx]    = space_operation::radian_normalize(this->randomizer.randomize_fixed_point_float(-ANGLE_DEVIATION, ANGLE_DEVIATION, DISCRETIZATION_SZ) + tmp_vec[idx]);

                return tmp_vec;
            }

            auto uniform_cosine_two(const std::vector<size_t>& enumeration_vec,
                                    const std::vector<crm_x_float_t>& vec) -> std::vector<crm_x_float_t>
            {
                if (!enumeration_vec.empty())
                {
                    throw std::invalid_argument("bad enumeration, redundant enumeration tree");
                }

                if (vec.empty())
                {
                    return vec;
                }

                const crm_x_float_t ANGLE_DEVIATION = 0.001; //
                const size_t DISCRETIZATION_SZ      = 1'000'000'000ULL;
                size_t idx0                         = this->randomizer.randomize_uint(0, vec.size());
                size_t idx1                         = this->randomizer.randomize_uint(0, vec.size());

                auto tmp_vec    = vec;
                tmp_vec[idx0]   = space_operation::radian_normalize(this->randomizer.randomize_fixed_point_float(-ANGLE_DEVIATION, ANGLE_DEVIATION, DISCRETIZATION_SZ) + tmp_vec[idx0]);
                tmp_vec[idx1]   = space_operation::radian_normalize(this->randomizer.randomize_fixed_point_float(-ANGLE_DEVIATION, ANGLE_DEVIATION, DISCRETIZATION_SZ) + tmp_vec[idx1]);

                return tmp_vec;
            }

            auto uniform_cosine_all(const std::vector<size_t>& enumeration_vec,
                                    const std::vector<crm_x_float_t>& vec) -> std::vector<crm_x_float_t>
            {
                if (!enumeration_vec.empty())
                {
                    throw std::invalid_argument("bad enumeration, redundant enumeration tree");
                }

                if (vec.empty())
                {
                    return vec;
                }

                const crm_x_float_t ANGLE_DEVIATION         = 0.001; //
                const size_t DISCRETIZATION_SZ              = 1'000'000'000ULL;
                const crm_x_float_t individual_deviation    = std::max(static_cast<crm_x_float_t>(ANGLE_DEVIATION / vec.size()), std::numeric_limits<crm_x_float_t>::min());

                std::vector<crm_x_float_t> rs_vec(vec.size());

                for (size_t i = 0u; i < vec.size(); ++i)
                {
                    crm_x_float_t deviation = this->randomizer.randomize_fixed_point_float(-individual_deviation, individual_deviation, DISCRETIZATION_SZ);
                    rs_vec[i]               = space_operation::radian_normalize(vec[i] + deviation);
                }

                return rs_vec;
            }

            auto uniform_cosine(const std::vector<size_t>& enumeration_vec,
                                const std::vector<crm_x_float_t>& org_vec) -> std::vector<crm_x_float_t>
            {
                if (enumeration_vec.empty())
                {
                    throw std::invalid_argument("bad enumeration, short enumeration tree");
                }

                size_t enum_value       = enumeration_vec.front();
                const size_t CHOICE_SZ  = 3u;

                if (enum_value >= CHOICE_SZ)
                {
                    throw std::invalid_argument("bad enumeration, invalid enumeration");
                }

                auto successor          = std::vector<size_t>(std::next(enumeration_vec.begin()), enumeration_vec.end());

                switch (enum_value)
                {
                    case 0:
                    {
                        return this->uniform_cosine_one(successor, org_vec);
                    }
                    case 1:
                    {
                        return this->uniform_cosine_two(successor, org_vec);
                    }
                    case 2:
                    {
                        return this->uniform_cosine_all(successor, org_vec);
                    }
                    default:
                    {
                        std::unreachable();
                    }
                }
            }

            auto focal_cosine_1_one(const std::vector<size_t>& enumeration_vec,
                                    const std::vector<crm_x_float_t>& vec) -> std::vector<crm_x_float_t>
            {
                if (!enumeration_vec.empty())
                {
                    throw std::invalid_argument("bad enumeration, redundant enumeration tree");
                }

                if (vec.empty())
                {
                    return vec;
                }

                const crm_x_float_t ANGLE_DEVIATION = 0.001; //

                size_t idx      = this->randomizer.randomize_uint(0, vec.size());
                auto tmp_vec    = vec;
                tmp_vec[idx]    = space_operation::radian_normalize(stdx::deviation_clamp<crm_x_float_t>(this->focal_randomizer.ld_randomize_focal(true), ANGLE_DEVIATION) + tmp_vec[idx]);

                return tmp_vec;
            }

            auto focal_cosine_1_two(const std::vector<size_t>& enumeration_vec,
                                    const std::vector<crm_x_float_t>& vec) -> std::vector<crm_x_float_t>
            {
                if (!enumeration_vec.empty())
                {
                    throw std::invalid_argument("bad enumeration, redundant enumeration tree");
                }

                if (vec.empty())
                {
                    return vec;
                }

                const crm_x_float_t ANGLE_DEVIATION = 0.001; //

                size_t idx0     = this->randomizer.randomize_uint(0, vec.size());
                size_t idx1     = this->randomizer.randomize_uint(0, vec.size());

                auto tmp_vec    = vec;
                tmp_vec[idx0]   = space_operation::radian_normalize(stdx::deviation_clamp<crm_x_float_t>(this->focal_randomizer.ld_randomize_focal(true), ANGLE_DEVIATION) + tmp_vec[idx0]);
                tmp_vec[idx1]   = space_operation::radian_normalize(stdx::deviation_clamp<crm_x_float_t>(this->focal_randomizer.ld_randomize_focal(true), ANGLE_DEVIATION) + tmp_vec[idx1]);

                return tmp_vec;
            }

            auto focal_cosine_1_all(const std::vector<size_t>& enumeration_vec,
                                    const std::vector<crm_x_float_t>& vec) -> std::vector<crm_x_float_t>
            {
                if (!enumeration_vec.empty())
                {
                    throw std::invalid_argument("bad enumeration, redundant enumeration tree");
                }

                if (vec.empty())
                {
                    return vec;
                }

                const crm_x_float_t ANGLE_DEVIATION         = 0.001; //
                const crm_x_float_t individual_deviation    = ANGLE_DEVIATION / vec.size();

                std::vector<crm_x_float_t> rs_vec(vec.size());

                for (size_t i = 0u; i < vec.size(); ++i)
                {
                    crm_x_float_t tentative_deviation   = this->focal_randomizer.ld_randomize_focal(true);
                    crm_x_float_t deviation             = stdx::deviation_clamp(tentative_deviation, individual_deviation);
                    rs_vec[i]                           = space_operation::radian_normalize(vec[i] + deviation);
                }

                return rs_vec;
            }

            auto focal_cosine_1(const std::vector<size_t>& enumeration_vec,
                                const std::vector<crm_x_float_t>& org_vec) -> std::vector<crm_x_float_t>
            {
                if (enumeration_vec.empty())
                {
                    throw std::invalid_argument("bad enumeration, short enumeration tree");
                }

                size_t enum_value       = enumeration_vec.front();
                const size_t CHOICE_SZ  = 3u;

                if (enum_value >= CHOICE_SZ)
                {
                    throw std::invalid_argument("bad enumeration, invalid enumeration");
                }

                auto successor          = std::vector<size_t>(std::next(enumeration_vec.begin()), enumeration_vec.end());

                switch (enum_value)
                {
                    case 0:
                    {
                        return this->focal_cosine_1_one(successor, org_vec);
                    }
                    case 1:
                    {
                        return this->focal_cosine_1_two(successor, org_vec);
                    }
                    case 2:
                    {
                        return this->focal_cosine_1_all(successor, org_vec);
                    }
                    default:
                    {
                        std::unreachable();
                    }
                }
            }

            auto focal_cosine_2_one(const std::vector<size_t>& enumeration_vec,
                                    const std::vector<crm_x_float_t>& vec) -> std::vector<crm_x_float_t>
            {
                if (!enumeration_vec.empty())
                {
                    throw std::invalid_argument("bad enumeration, redundant enumeration tree");
                }

                if (vec.empty())
                {
                    return vec;
                }

                const crm_x_float_t ANGLE_DEVIATION_RANGE = std::numbers::pi_v<crm_x_float_t> * 2;

                size_t idx      = this->randomizer.randomize_uint(0, vec.size());
                auto tmp_vec    = vec;
                tmp_vec[idx]    = space_operation::radian_normalize(stdx::deviation_clamp<crm_x_float_t>(this->focal_randomizer.ld_randomize_focal(true), ANGLE_DEVIATION_RANGE) + tmp_vec[idx]);

                return tmp_vec;
            }

            auto focal_cosine_2_two(const std::vector<size_t>& enumeration_vec,
                                    const std::vector<crm_x_float_t>& vec) -> std::vector<crm_x_float_t>
            {
                if (!enumeration_vec.empty())
                {
                    throw std::invalid_argument("bad enumeration, redundant enumeration tree");
                }

                if (vec.empty())
                {
                    return vec;
                }

                const crm_x_float_t ANGLE_DEVIATION_RANGE = std::numbers::pi_v<crm_x_float_t> * 2;

                size_t idx0     = this->randomizer.randomize_uint(0, vec.size());
                size_t idx1     = this->randomizer.randomize_uint(0, vec.size());

                auto tmp_vec    = vec;
                tmp_vec[idx0]   = space_operation::radian_normalize(stdx::deviation_clamp<crm_x_float_t>(this->focal_randomizer.ld_randomize_focal(true), ANGLE_DEVIATION_RANGE) + tmp_vec[idx0]);
                tmp_vec[idx1]   = space_operation::radian_normalize(stdx::deviation_clamp<crm_x_float_t>(this->focal_randomizer.ld_randomize_focal(true), ANGLE_DEVIATION_RANGE) + tmp_vec[idx1]);

                return tmp_vec;
            }

            auto focal_cosine_2_all(const std::vector<size_t>& enumeration_vec,
                                    const std::vector<crm_x_float_t>& vec) -> std::vector<crm_x_float_t>
            {
                if (!enumeration_vec.empty())
                {
                    throw std::invalid_argument("bad enumeration, redundant enumeration tree");
                }

                if (vec.empty())
                {
                    return vec;
                }

                const crm_x_float_t ANGLE_DEVIATION_RANGE   = std::numbers::pi_v<crm_x_float_t> * 2;

                std::vector<crm_x_float_t> rs_vec(vec.size());

                for (size_t i = 0u; i < vec.size(); ++i)
                {
                    crm_x_float_t tentative_deviation   = this->focal_randomizer.ld_randomize_focal(true);
                    crm_x_float_t deviation             = stdx::deviation_clamp(tentative_deviation, ANGLE_DEVIATION_RANGE);
                    rs_vec[i]                           = space_operation::radian_normalize(vec[i] + deviation);
                }

                return rs_vec;
            }

            auto focal_cosine_2(const std::vector<size_t>& enumeration_vec,
                                const std::vector<crm_x_float_t>& org_vec) -> std::vector<crm_x_float_t>
            {
                if (enumeration_vec.empty())
                {
                    throw std::invalid_argument("bad enumeration, short enumeration tree");
                }

                size_t enum_value       = enumeration_vec.front();
                const size_t CHOICE_SZ  = 3u;

                if (enum_value >= CHOICE_SZ)
                {
                    throw std::invalid_argument("bad enumeration, invalid enumeration");
                }

                auto successor          = std::vector<size_t>(std::next(enumeration_vec.begin()), enumeration_vec.end());

                switch (enum_value)
                {
                    case 0:
                    {
                        return this->focal_cosine_2_one(successor, org_vec);
                    }
                    case 1:
                    {
                        return this->focal_cosine_2_two(successor, org_vec);
                    }
                    case 2:
                    {
                        return this->focal_cosine_2_all(successor, org_vec);
                    }
                    default:
                    {
                        std::unreachable();
                    }
                }
            }

            auto focal_cosine(const std::vector<size_t>& enumeration_vec,
                              const std::vector<crm_x_float_t>& org_vec) -> std::vector<crm_x_float_t>
            {
                if (enumeration_vec.empty())
                {
                    throw std::invalid_argument("bad enumeration, short enumeration tree");
                }

                size_t enum_value       = enumeration_vec.front();
                const size_t CHOICE_SZ  = 2u;

                if (enum_value >= CHOICE_SZ)
                {
                    throw std::invalid_argument("bad enumeration, invalid enumeration");
                }

                auto successor          = std::vector<size_t>(std::next(enumeration_vec.begin()), enumeration_vec.end());

                switch (enum_value)
                {
                    case 0:
                    {
                        return this->focal_cosine_1(successor, org_vec);
                    }
                    case 1:
                    {
                        return this->focal_cosine_2(successor, org_vec);
                    }
                    default:
                    {
                        std::unreachable();
                    }
                }
            }
    };

    class NormalUnitRandomizerMachine: public virtual VectorRandomizerMachineInterface
    {
        private:

            coefficient_randomizer::CoefficientRandomizer randomizer;

        public:

            auto randomize(size_t sz) -> std::vector<crm_x_float_t>
            {
                return randomizer.template randomize_unit_vector<crm_x_float_t>(sz);
            }
    };

    class NormalRadianRandomizerMachine: public virtual VectorRandomizerMachineInterface
    {
        private:

            coefficient_randomizer::CoefficientRandomizer randomizer;

        public:

            auto randomize(size_t sz) -> std::vector<crm_x_float_t>
            {
                return randomizer.template randomize_radian_vector<crm_x_float_t>(sz);
            }
    };

    class CosineRecommenderMachine: public virtual CosineRecommenderMachineInterface
    {
        private:

            std::unique_ptr<branch_optimizer::MultipleBranchPredictorInterface> branch_predictor;
            std::unique_ptr<EnumerableCosineProjectorInterface> cosine_projector;
            std::unique_ptr<VectorRandomizerMachineInterface> cosine_randomizer;
            std::shared_ptr<coordinate_recommender_machine::CoordinateRecommenderMachineInterface> coordinate_recommender;
            ChanceMachine chance_machine;
            size_t space_sz;

        public:

            CosineRecommenderMachine(std::unique_ptr<branch_optimizer::MultipleBranchPredictorInterface> branch_predictor,
                                     std::unique_ptr<EnumerableCosineProjectorInterface> cosine_projector,
                                     std::unique_ptr<VectorRandomizerMachineInterface> cosine_randomizer,
                                     std::unique_ptr<coordinate_recommender_machine::CoordinateRecommenderMachineInterface> coordinate_recommender,
                                     ChanceMachine chance_machine,
                                     size_t space_sz) noexcept: branch_predictor(std::move(branch_predictor)),
                                                                cosine_projector(std::move(cosine_projector)),
                                                                cosine_randomizer(std::move(cosine_randomizer)),
                                                                coordinate_recommender(std::move(coordinate_recommender)),
                                                                chance_machine(std::move(chance_machine)),
                                                                space_sz(space_sz){}

            auto next() -> std::unique_ptr<CosineRecommendationResultInterface>
            {
                bool coin_flip = this->chance_machine.flip_a_coin();

                if (coin_flip)
                {
                    return this->make_random_recommendation_result(this->cosine_randomizer->randomize(this->space_sz));
                }

                std::optional<std::vector<coordinate_recommender_machine::machine_float_t>> recommended_coor = this->coordinate_recommender->next();

                if (!recommended_coor.has_value())
                {
                    return this->make_random_recommendation_result(this->cosine_randomizer->randomize(this->space_sz));
                }

                std::shared_ptr<branch_optimizer::MultipleBranchPredictionResultInterface> branch = this->branch_predictor->next();

                if (branch == nullptr)
                {
                    std::abort();
                }

                std::vector<size_t> enumeration_vec = branch->get_enumeration();
                std::vector<crm_x_float_t> new_coor = this->cosine_projector->cosine_like(enumeration_vec, stdx::to_castable_vector_initializer(recommended_coor.value()));

                try
                {
                    stdx::xsafe_float_range_access(new_coor.data(), new_coor.size());

                    std::vector<crm_x_float_t> euclid_vec(new_coor.size());
                    space_operation::radian_to_euclidean_coordinate(new_coor.data(), new_coor.size(), euclid_vec.data());

                    stdx::xsafe_float_range_access(euclid_vec.data(), euclid_vec.size());

                }
                catch (...)
                {
                    new_coor = stdx::to_castable_vector_initializer(recommended_coor.value());
                }

                return this->make_predicted_recommendation_result(new_coor, branch);
            }

            auto space_size() -> size_t
            {
                return this->space_sz;
            }

        private:

            class InternalCosineRecommendationResult: public virtual CosineRecommendationResultInterface
            {
                private:

                    std::vector<crm_x_float_t> result;
                    std::shared_ptr<coordinate_recommender_machine::CoordinateRecommenderMachineInterface> coordinate_recommender;
                    std::shared_ptr<branch_optimizer::MultipleBranchPredictionResultInterface> branch;
                    bool was_feedback_received;

                public:

                    InternalCosineRecommendationResult(std::vector<crm_x_float_t> result,
                                                       std::shared_ptr<coordinate_recommender_machine::CoordinateRecommenderMachineInterface> coordinate_recommender,
                                                       std::shared_ptr<branch_optimizer::MultipleBranchPredictionResultInterface> branch,
                                                       bool was_feedback_received) noexcept: result(std::move(result)),
                                                                                             coordinate_recommender(std::move(coordinate_recommender)),
                                                                                             branch(std::move(branch)),
                                                                                             was_feedback_received(was_feedback_received){}

                    auto get() -> std::vector<crm_x_float_t>
                    {
                        std::vector<crm_x_float_t> euclid_vec(this->result.size());
                        space_operation::radian_to_euclidean_coordinate(this->result.data(), this->result.size(), euclid_vec.data());

                        return euclid_vec;
                    }

                    void feedback(crm_x_float_t score)
                    {
                        // stdx::
                        //let's do a silent error

                        if (std::exchange(this->was_feedback_received, true))
                        {
                            return;
                        }

                        if (std::isnan(score))
                        {
                            return;
                        }

                        if (std::isinf(score))
                        {
                            return;
                        }

                        if (this->coordinate_recommender == nullptr)
                        {
                            std::abort();
                        }

                        if (this->branch == nullptr)
                        {
                            std::abort();
                        }

                        this->coordinate_recommender->feedback(stdx::to_castable_vector_initializer(this->result), score);
                        this->branch->feedback(score); //can we prove that score is the right score for branch?
                    }
            };

            class RandomRecommendationResult: public virtual CosineRecommendationResultInterface
            {
                private:

                    std::vector<crm_x_float_t> result;
                    std::shared_ptr<coordinate_recommender_machine::CoordinateRecommenderMachineInterface> coordinate_recommender;
                    bool was_feedback_received;
                
                public:

                    RandomRecommendationResult(std::vector<crm_x_float_t> result,
                                               std::shared_ptr<coordinate_recommender_machine::CoordinateRecommenderMachineInterface> coordinate_recommender,
                                               bool was_feedback_received) noexcept: result(std::move(result)),
                                                                                     coordinate_recommender(std::move(coordinate_recommender)),
                                                                                     was_feedback_received(was_feedback_received){}

                    auto get() -> std::vector<crm_x_float_t>
                    {
                        std::vector<crm_x_float_t> euclid_vec(this->result.size());
                        space_operation::radian_to_euclidean_coordinate(this->result.data(), this->result.size(),
                                                                        euclid_vec.data());

                        return euclid_vec;
                    }

                    void feedback(crm_x_float_t score)
                    {
                        if (std::exchange(this->was_feedback_received, true))
                        {
                            return;
                        }

                        if (std::isnan(score))
                        {
                            return;
                        }

                        if (std::isinf(score))
                        {
                            return;
                        }

                        if (this->coordinate_recommender == nullptr)
                        {
                            std::abort();
                        }

                        this->coordinate_recommender->feedback(this->result, score);
                    }
            };

            auto make_random_recommendation_result(const std::vector<crm_x_float_t>& vec) -> std::unique_ptr<CosineRecommendationResultInterface>
            {
                return std::make_unique<RandomRecommendationResult>(vec,
                                                                    this->coordinate_recommender,
                                                                    false);
            }

            auto make_predicted_recommendation_result(const std::vector<crm_x_float_t>& vec,
                                                      const std::shared_ptr<branch_optimizer::MultipleBranchPredictionResultInterface>& branch) -> std::unique_ptr<CosineRecommendationResultInterface>
            {
                if (branch == nullptr)
                {
                    throw std::invalid_argument("bad argument, null");
                }

                return std::make_unique<InternalCosineRecommendationResult>(vec,
                                                                            this->coordinate_recommender,
                                                                            branch,
                                                                            false);
            }
    };

    class TraditionalCosineRecommenderMachineWrapper: public virtual CosineRecommenderMachineInterface
    {
        private:

            size_t space_sz;
            ChanceMachine chance_machine;
            std::shared_ptr<cosine_recommender_machine::CosineRecommenderMachineInterface> base;
            std::unique_ptr<VectorRandomizerMachineInterface> unit_randomizer;

        public:

            TraditionalCosineRecommenderMachineWrapper(size_t space_sz,
                                                       ChanceMachine chance_machine,
                                                       std::unique_ptr<cosine_recommender_machine::CosineRecommenderMachineInterface> base,
                                                       std::unique_ptr<VectorRandomizerMachineInterface> unit_randomizer) noexcept: space_sz(space_sz),
                                                                                                                                      chance_machine(std::move(chance_machine)),
                                                                                                                                      base(std::move(base)),
                                                                                                                                      unit_randomizer(std::move(unit_randomizer)){}

            auto next() -> std::unique_ptr<CosineRecommendationResultInterface>
            {
                bool coin_flip = this->chance_machine.flip_a_coin();

                if (coin_flip)
                {
                    return this->make_recommendation_result(this->unit_randomizer->randomize(this->space_sz));
                }
                else
                {
                    std::optional<std::vector<cosine_recommender_machine::crm_float_t>> result = this->base->next();

                    if (!result.has_value())
                    {
                        return this->make_recommendation_result(this->unit_randomizer->randomize(this->space_sz));
                    }

                    return this->make_recommendation_result(stdx::to_castable_vector_initializer(result.value()));
                }
            }

            auto space_size() -> size_t
            {
                return this->space_sz;
            }
        
        private:

            class InternalCosineRecommendationResult: public virtual CosineRecommendationResultInterface
            {
                private:

                    std::shared_ptr<cosine_recommender_machine::CosineRecommenderMachineInterface> base;
                    std::vector<crm_x_float_t> result;

                public:

                    InternalCosineRecommendationResult(std::shared_ptr<cosine_recommender_machine::CosineRecommenderMachineInterface> base,
                                                       std::vector<crm_x_float_t> result) noexcept: base(std::move(base)),
                                                                                                    result(std::move(result)){}
                    

                    auto get() -> std::vector<crm_x_float_t>
                    {
                        return this->result;
                    }

                    void feedback(crm_x_float_t score)
                    {
                        if (std::isnan(score))
                        {
                            return;
                        }

                        if (std::isinf(score))
                        {
                            return;
                        }

                        this->base->feedback(this->result, score);
                    }
            };

            auto make_recommendation_result(const std::vector<crm_x_float_t>& vec) -> std::unique_ptr<CosineRecommendationResultInterface>
            {
                return std::make_unique<InternalCosineRecommendationResult>(this->base, vec);
            }
    };

    class MixedCosineRecommenderMachine: public virtual CosineRecommenderMachineInterface
    {
        private:

            std::unique_ptr<CosineRecommenderMachineInterface> machine_1;
            std::unique_ptr<CosineRecommenderMachineInterface> machine_2;
            ChanceMachine chance_machine;
        
        public:

            MixedCosineRecommenderMachine(std::unique_ptr<CosineRecommenderMachineInterface> machine_1,
                                          std::unique_ptr<CosineRecommenderMachineInterface> machine_2,
                                          ChanceMachine chance_machine) noexcept: machine_1(std::move(machine_1)),
                                                                                  machine_2(std::move(machine_2)),
                                                                                  chance_machine(std::move(chance_machine)){}

            auto next() -> std::unique_ptr<CosineRecommendationResultInterface>
            {
                bool coin_flip = this->chance_machine.flip_a_coin();

                if (coin_flip)
                {
                    return this->machine_1->next();
                }
                else
                {
                    return this->machine_2->next();
                }
            }

            auto space_size() -> size_t
            {
                return this->machine_1->space_size();
            }
    };

    class PoolMachine: public virtual CosineRecommenderMachineInterface
    {
        private:

            std::vector<std::unique_ptr<CosineRecommenderMachineInterface>> base_vec;
            size_t space_sz;
            conventional_randomizer::RandomizerObject randomizer;

        public:

            PoolMachine(std::vector<std::unique_ptr<CosineRecommenderMachineInterface>> base_vec,
                        size_t space_sz,
                        conventional_randomizer::RandomizerObject randomizer) noexcept: base_vec(std::move(base_vec)),
                                                                                        space_sz(space_sz),
                                                                                        randomizer(std::move(randomizer)){}

            auto next() -> std::unique_ptr<CosineRecommendationResultInterface>
            {
                size_t slot_idx = this->randomizer.randomize_uint(0u, this->base_vec.size());

                return this->base_vec[slot_idx]->next();
            }

            auto space_size() -> size_t
            {
                return this->space_sz;
            }
    };

    class MachineFactory
    {
        private:

            static auto get_decisive_cosine_projector() -> std::unique_ptr<EnumerableCosineProjectorInterface>
            {
                return std::make_unique<DecisiveCosineProjector>();
            }

            static auto get_coordinate_recommender() -> std::unique_ptr<coordinate_recommender_machine::CoordinateRecommenderMachineInterface>
            {
                const size_t COORDINATE_ECHO_SZ     = size_t{1} << 4;
                const size_t COORDINATE_WINDOW_SZ   = size_t{1} << 6;

                std::unique_ptr<coordinate_recommender_machine::CoordinateRecommenderMachineInterface> base     = std::make_unique<coordinate_recommender_machine::MixedCoordinateRecommenderMachine>();
                std::unique_ptr<coordinate_recommender_machine::CoordinateRecommenderMachineInterface> result   = std::make_unique<coordinate_recommender_machine::EchoCoordinateRecommenderMachine>(std::move(base), COORDINATE_ECHO_SZ);

                result->set_window_size(COORDINATE_WINDOW_SZ);

                return result;
            }

            static auto get_radian_vector_randomizer() -> std::unique_ptr<VectorRandomizerMachineInterface>
            {
                return std::make_unique<NormalRadianRandomizerMachine>();
            }

            static auto get_unit_vector_randomizer() -> std::unique_ptr<VectorRandomizerMachineInterface>
            {
                return std::make_unique<NormalUnitRandomizerMachine>();
            }

        public:

            static auto get_traditional_recommender_machine(size_t space_sz) -> std::unique_ptr<CosineRecommenderMachineInterface>
            {
                const size_t MIN_SPACE_SZ           = 0u;
                const size_t MAX_SPACE_SZ           = size_t{1} << 30;
                const size_t RANDOM_CHANCE_NUM      = 1u;
                const size_t RANDOM_CHANCE_DENOM    = 10u;

                if (std::clamp(space_sz, MIN_SPACE_SZ, MAX_SPACE_SZ) != space_sz)
                {
                    throw std::invalid_argument("bad space size, out of range");
                }

                return std::make_unique<TraditionalCosineRecommenderMachineWrapper>(space_sz,
                                                                                    ChanceMachine(RANDOM_CHANCE_DENOM, RANDOM_CHANCE_NUM),
                                                                                    cosine_recommender_machine::CosineRecommenderMachineFactory::get_echo_temporal_cosine_recommender_machine(),
                                                                                    get_unit_vector_randomizer());
            }

            static auto get_statistical_recommender_machine(size_t space_sz) -> std::unique_ptr<CosineRecommenderMachineInterface>
            {
                const size_t MIN_SPACE_SZ           = 0u;
                const size_t MAX_SPACE_SZ           = size_t{1} << 30;
                const size_t RANDOM_CHANCE_NUM      = 1u;
                const size_t RANDOM_CHANCE_DENOM    = 10u;

                if (std::clamp(space_sz, MIN_SPACE_SZ, MAX_SPACE_SZ) != space_sz)
                {
                    throw std::invalid_argument("bad space size, out of range");
                }

                std::unique_ptr<EnumerableCosineProjectorInterface> cosine_projector                                    = get_decisive_cosine_projector();
                std::unique_ptr<branch_optimizer::MultipleBranchPredictorInterface> branch_predictor                    = branch_optimizer::HierarchicalBranchPredictorFactory::get_best_branch_predictor_from_preorder_tree(cosine_projector->get_enumeration_prefix_tree());
                std::unique_ptr<VectorRandomizerMachineInterface> vector_randomizer                                     = get_radian_vector_randomizer();
                std::unique_ptr<coordinate_recommender_machine::CoordinateRecommenderMachineInterface> coor_recommender = get_coordinate_recommender();
                ChanceMachine chance_machine                                                                            = ChanceMachine(RANDOM_CHANCE_DENOM, RANDOM_CHANCE_NUM);

                return std::make_unique<CosineRecommenderMachine>(std::move(branch_predictor),
                                                                  std::move(cosine_projector),
                                                                  std::move(vector_randomizer),
                                                                  std::move(coor_recommender),
                                                                  std::move(chance_machine),
                                                                  space_sz);
            }

            static auto get_trad_stat_recommender_machine(size_t space_sz) -> std::unique_ptr<CosineRecommenderMachineInterface>
            {
                const size_t TRAD_CHANCE_NUM    = 2u;
                const size_t TRAD_CHANCE_DENOM  = 10u;

                return std::make_unique<MixedCosineRecommenderMachine>(get_traditional_recommender_machine(space_sz),
                                                                       get_statistical_recommender_machine(space_sz),
                                                                       ChanceMachine(TRAD_CHANCE_DENOM, TRAD_CHANCE_NUM));
            }

            static auto get_best_recommender_machine(size_t space_sz) -> std::unique_ptr<CosineRecommenderMachineInterface>
            {
                const size_t POOL_SZ = 2u;

                std::vector<std::unique_ptr<CosineRecommenderMachineInterface>> machine_pool{};

                for (size_t i = 0u; i < POOL_SZ; ++i)
                {
                    machine_pool.push_back(get_trad_stat_recommender_machine(space_sz));
                }

                return std::make_unique<PoolMachine>(std::move(machine_pool),
                                                     space_sz,
                                                     conventional_randomizer::RandomizerObject{});
            }
    };
}

#endif