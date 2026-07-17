#ifndef __TEMPORAL_COEFFICIENT_PROJECTOR_3_H__
#define __TEMPORAL_COEFFICIENT_PROJECTOR_3_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <stdexcept>
#include <algorithm>
#include <utility>
#include "virtual_interval_coefficient_optimizer_tree.h"
#include "temporal_coefficient_projector_2_interface.h"
#include "temporal_coefficient_projector_3_interface.h"
#include "temporal_coefficient_projector_2.h"
#include "activation.h"
#include "temporal_coefficient_projector.h"
#include <general_definition/float_def.h>
#include "range_optimizer.h"

namespace temporal_coefficient_projector_3
{
    using std_float_t = float_def::std_float_t;

    //we'd try to optimize storages by leveraging leaf nodes, unit nodes, and improve memory usage of interval trees
    //this is important

    class DynamicFocalTemporalCoefficientProjectorGenerator: public virtual TemporalCoefficientProjectorGeneratorInterface
    {
        private:

            std::unique_ptr<virtual_interval_coefficient_optimizer_tree::TranslationOptimizerTreeInterface> focal_organizer;
            std::unique_ptr<temporal_coefficient_projector_2::TemporalCoefficientProjectorGeneratorInterface> base;
            std::unique_ptr<range_optimizer::RangePredictorInterface> range_predictor;

            conventional_randomizer::RandomizerObject raw_randomizer;
            conventional_randomizer::ApplicationRandomizerObject app_randomizer;
            conventional_randomizer::RangeRandomizerObject range_randomizer;

            size_t refocal_counter;
            size_t refocal_threshold;
            size_t projection_sz;

            static inline constexpr size_t MAX_VECTOR_SIZE = size_t{1} << 16;

        public:

            DynamicFocalTemporalCoefficientProjectorGenerator(std::unique_ptr<virtual_interval_coefficient_optimizer_tree::TranslationOptimizerTreeInterface> focal_organizer,
                                                              std::unique_ptr<temporal_coefficient_projector_2::TemporalCoefficientProjectorGeneratorInterface> base,
                                                              std::unique_ptr<range_optimizer::RangePredictorInterface> range_predictor,

                                                              conventional_randomizer::RandomizerObject raw_randomizer,
                                                              conventional_randomizer::ApplicationRandomizerObject app_randomizer,
                                                              conventional_randomizer::RangeRandomizerObject range_randomizer,

                                                              size_t refocal_counter,
                                                              size_t refocal_threshold,
                                                              size_t projection_sz) noexcept: focal_organizer(std::move(focal_organizer)),
                                                                                              base(std::move(base)),
                                                                                              range_predictor(std::move(range_predictor)),
                                                                                              raw_randomizer(std::move(raw_randomizer)),
                                                                                              app_randomizer(std::move(app_randomizer)),
                                                                                              range_randomizer(std::move(range_randomizer)),
                                                                                              refocal_counter(refocal_counter),
                                                                                              refocal_threshold(refocal_threshold),
                                                                                              projection_sz(projection_sz){}


            auto get() -> std::unique_ptr<TemporalCoefficientProjectorContainerInterface>
            {
                if (this->refocal_counter == this->refocal_threshold)
                {
                    this->focal_organizer->rearrange_focal();
                    this->refocal_counter = 0u;
                }

                auto [projector, translation_segment_vec] = [&]
                {
                    if (this->raw_randomizer.flip_a_coin())
                    {
                        return this->get_random_projector();
                    }
                    else
                    {
                        return this->get_predicted_projector();
                    }
                }();

                std::unique_ptr<virtual_interval_coefficient_optimizer_tree::TranslationSpaceTensorInterface> feedbackable = this->focal_organizer->get_translation_tensor(translation_segment_vec);
                std::vector<std::pair<size_t, size_t>> retranslation_segment_vec{};

                for (const std::vector<std::pair<size_t, size_t>>& current_segment_vec: feedbackable->get_translation_space())
                {
                    std::copy(current_segment_vec.begin(), current_segment_vec.end(), std::back_inserter(retranslation_segment_vec));
                }

                auto translated_projector   = this->get_translation_projector(projector->get(), retranslation_segment_vec, this->projection_sz);
                this->refocal_counter       += 1;

                return std::make_unique<InternalProjectorContainer>(std::move(translated_projector),
                                                                    std::move(feedbackable),
                                                                    std::move(projector));
            }

            auto space_size() -> size_t
            {
                return this->projection_sz;
            }

        private:

            class InternalProjectorContainer: public virtual TemporalCoefficientProjectorContainerInterface
            {
                private:

                    std::shared_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface> projector;
                    std::unique_ptr<virtual_interval_coefficient_optimizer_tree::TranslationSpaceTensorInterface> feedbackable;
                    std::shared_ptr<temporal_coefficient_projector_2::TemporalCoefficientProjectorContainerInterface> tensor_2;
               
                public:

                    InternalProjectorContainer(std::shared_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface> projector,
                                               std::unique_ptr<virtual_interval_coefficient_optimizer_tree::TranslationSpaceTensorInterface> feedbackable,
                                               std::shared_ptr<temporal_coefficient_projector_2::TemporalCoefficientProjectorContainerInterface> tensor_2) noexcept: projector(std::move(projector)),
                                                                                                                                                                     feedbackable(std::move(feedbackable)),
                                                                                                                                                                     tensor_2(std::move(tensor_2)){}

                    auto get() -> std::shared_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface>
                    {
                        return this->projector;
                    }

                    void feedback(double rating)
                    {
                        this->feedbackable->feedback(rating);
                        this->tensor_2->feedback(rating);
                    }
            };

            auto randomize_activation_vector(size_t sz) -> std::vector<activation::activation_codex_t>
            {
                std::vector<activation::activation_codex_t> rs{};

                for (size_t i = 0u; i < sz; ++i)
                {
                    size_t enumeration_idx = this->raw_randomizer.randomize_uint(0u, activation::ACTIVATION_CODEX_RANGE);
                    rs.push_back(enumeration_idx);
                }

                return rs;
            }

            auto randomize_activation_size_inclusive(size_t sz) -> size_t
            {
                if (sz == 0u)
                {
                    return 0u;
                }

                const size_t MIN_ACTIVATION_SZ  = 1u;
                size_t tentative_sz             = this->range_randomizer.randomize_range(sz + 1u);

                return std::max(tentative_sz, MIN_ACTIVATION_SZ);
            }

            auto min_chunk_size_for_vector_size_of(size_t sz) -> size_t
            {
                return sz / MAX_VECTOR_SIZE + static_cast<size_t>(sz % MAX_VECTOR_SIZE != 0u);
            }

            auto randomize_vector_chunk_size_inclusive(size_t sz) -> size_t
            {
                size_t tentative_chunk_size     = this->range_randomizer.randomize_range(sz + 1u);
                const size_t MIN_CHUNK_SIZE     = 1u;

                return std::max(std::max(MIN_CHUNK_SIZE, tentative_chunk_size), this->min_chunk_size_for_vector_size_of(sz));
            }

            auto to_range_translation_table(const std::vector<size_t>& chosen_suffix_table,
                                            size_t chunk_sz_per_suffix,
                                            size_t suffix_table_sz,
                                            size_t rem_sz) -> std::vector<std::pair<size_t, size_t>> 
            {
                std::vector<std::pair<size_t, size_t>> range_table{};

                for (size_t suffix: chosen_suffix_table)
                {
                    size_t offset = suffix * chunk_sz_per_suffix;
                    size_t suffix_sz;

                    if (suffix + 1u == suffix_table_sz)
                    {
                        suffix_sz = chunk_sz_per_suffix - rem_sz;
                    }
                    else
                    {
                        suffix_sz = chunk_sz_per_suffix;
                    }

                    range_table.push_back({offset, suffix_sz});
                }

                return range_table;
            }

            auto count_activated_nodes(const std::vector<size_t>& chosen_suffix_table,
                                       size_t chunk_sz_per_suffix,
                                       size_t suffix_table_sz,
                                       size_t rem_sz) -> size_t
            {
                size_t total = 0u;

                for (size_t suffix: chosen_suffix_table)
                {
                    if (suffix + 1u == suffix_table_sz)
                    {
                        total += chunk_sz_per_suffix - rem_sz;
                    }
                    else
                    {
                        total += chunk_sz_per_suffix;
                    }
                }

                return total;
            }

            auto get_random_projector() -> std::pair<std::shared_ptr<temporal_coefficient_projector_2::TemporalCoefficientProjectorContainerInterface>,
                                                     std::vector<std::pair<size_t, size_t>>>
            {
                size_t sz               = this->projection_sz;
                size_t chunk_sz         = this->randomize_vector_chunk_size_inclusive(sz);
                size_t segment_sz       = sz / chunk_sz + static_cast<size_t>(sz % chunk_sz != 0u);
                size_t activation_sz    = this->randomize_activation_size_inclusive(segment_sz);
                size_t rem_sz           = segment_sz * chunk_sz - sz;

                std::vector<activation::activation_codex_t> activation_codex_vec    = this->randomize_activation_vector(activation_sz);
                std::vector<size_t> suffix_array                                    = std::vector<size_t>(segment_sz);

                std::iota(suffix_array.begin(), suffix_array.end(), 0u);

                std::vector<size_t> activated_suffix_array                          = activation::activate(suffix_array, activation_codex_vec);
                std::vector<std::pair<size_t, size_t>> translation_table            = this->to_range_translation_table(activated_suffix_array, chunk_sz, segment_sz, rem_sz);
                size_t actual_projection_sz                                         = this->count_activated_nodes(activated_suffix_array, chunk_sz, segment_sz, rem_sz);

                return std::make_pair(this->base->get(actual_projection_sz), std::move(translation_table));
            }

            class InternalRangePredictedProjectorContainer: public virtual temporal_coefficient_projector_2::TemporalCoefficientProjectorContainerInterface
            {
                private:

                    std::shared_ptr<temporal_coefficient_projector_2::TemporalCoefficientProjectorContainerInterface> base;
                    std::unique_ptr<range_optimizer::RangePredictionResultInterface> range_prediction_result;

                public:

                    InternalRangePredictedProjectorContainer(std::shared_ptr<temporal_coefficient_projector_2::TemporalCoefficientProjectorContainerInterface> base,
                                                             std::unique_ptr<range_optimizer::RangePredictionResultInterface> range_prediction_result): base(std::move(base)),
                                                                                                                                                        range_prediction_result(std::move(range_prediction_result)){}

                    auto get() -> std::shared_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface>
                    {
                        return this->base->get();
                    }

                    void feedback(double rating)
                    {
                        this->base->feedback(rating);
                        this->range_prediction_result->feedback(rating);
                    }
            };

            auto get_predicted_projector() -> std::pair<std::shared_ptr<temporal_coefficient_projector_2::TemporalCoefficientProjectorContainerInterface>,
                                                        std::vector<std::pair<size_t, size_t>>>
            {
                std::unique_ptr<range_optimizer::RangePredictionResultInterface> range_prediction_result    = this->range_predictor->next();
                
                size_t max_activation_sz    = std::min(range_prediction_result->get_range(), this->projection_sz);
                size_t chunk_sz             = this->randomize_vector_chunk_size_inclusive(max_activation_sz);
                size_t max_segment_sz       = max_activation_sz / chunk_sz + static_cast<size_t>(max_activation_sz % chunk_sz != 0u);
                size_t activation_sz        = this->randomize_activation_size_inclusive(max_segment_sz);

                size_t sz                   = this->projection_sz;
                size_t segment_sz           = sz / chunk_sz + static_cast<size_t>(sz % chunk_sz != 0u);
                size_t rem_sz               = segment_sz * chunk_sz - sz;

                std::vector<activation::activation_codex_t> activation_codex_vec    = this->randomize_activation_vector(activation_sz);
                std::vector<size_t> suffix_array                                    = std::vector<size_t>(segment_sz);

                std::iota(suffix_array.begin(), suffix_array.end(), 0u);

                std::vector<size_t> activated_suffix_array                          = activation::activate(suffix_array, activation_codex_vec);
                std::vector<std::pair<size_t, size_t>> translation_table            = this->to_range_translation_table(activated_suffix_array, chunk_sz, segment_sz, rem_sz);
                size_t actual_projection_sz                                         = this->count_activated_nodes(activated_suffix_array, chunk_sz, segment_sz, rem_sz);

                return std::make_pair(std::make_shared<InternalRangePredictedProjectorContainer>(this->base->get(actual_projection_sz),
                                                                                                 std::move(range_prediction_result)),
                                      std::move(translation_table));
            }

            auto get_translation_projector(const std::shared_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface>& base,
                                           const std::vector<std::pair<size_t, size_t>>& translation_table,
                                           size_t sz) -> std::unique_ptr<temporal_coefficient_projector::TemporalCoefficientProjectorInterface>
            {
                std::vector<size_t> idx_table{};

                for (const auto& [first, range]: translation_table)
                {
                    for (size_t i = 0u; i < range; ++i)
                    {
                        idx_table.push_back(first + i);
                    }
                }

                return std::make_unique<temporal_coefficient_projector::TranslationProjector>(std::make_unique<temporal_coefficient_projector::SharedPointerProjector>(base),
                                                                                              std::move(idx_table),
                                                                                              sz);
            }
    };

    class PoolGenerator: public virtual TemporalCoefficientProjectorGeneratorInterface
    {
        private:

            std::vector<std::unique_ptr<TemporalCoefficientProjectorGeneratorInterface>> base_vec;
            conventional_randomizer::RandomizerObject raw_randomizer;
            size_t projection_sz;

        public:

            PoolGenerator(std::vector<std::unique_ptr<TemporalCoefficientProjectorGeneratorInterface>> base_vec,
                          conventional_randomizer::RandomizerObject raw_randomizer,
                          size_t projection_sz) noexcept: base_vec(std::move(base_vec)),
                                                          raw_randomizer(std::move(raw_randomizer)),
                                                          projection_sz(projection_sz){}

            auto get() -> std::unique_ptr<TemporalCoefficientProjectorContainerInterface>
            {
                size_t idx = this->raw_randomizer.randomize_uint(0u, this->base_vec.size());

                return this->base_vec[idx]->get();
            }

            auto space_size() -> size_t
            {
                return this->projection_sz;
            }
    };

    class GeneratorFactory
    {
        public:

            template <class PromotedFloatType = std_float_t>
            static auto get_normal_generator(size_t coefficient_sz,
                                             size_t leaf_sz = 8u) -> std::unique_ptr<TemporalCoefficientProjectorGeneratorInterface>
            {
                const size_t REFOCAL_THRESHOLD  = size_t{1} << 6;
                const size_t LEAF_SZ            = leaf_sz;

                return std::make_unique<DynamicFocalTemporalCoefficientProjectorGenerator>(virtual_interval_coefficient_optimizer_tree::TreeFactory::get_translation_focal_tree(coefficient_sz, LEAF_SZ),
                                                                                           temporal_coefficient_projector_2::GeneratorFactory::get_best_generator<PromotedFloatType>(),
                                                                                           std::make_unique<range_optimizer::ExponentialRangePredictor>(coefficient_sz),

                                                                                           conventional_randomizer::RandomizerObject{},
                                                                                           conventional_randomizer::ApplicationRandomizerObject{},
                                                                                           conventional_randomizer::RangeRandomizerObject{},

                                                                                           0u,
                                                                                           REFOCAL_THRESHOLD,
                                                                                           coefficient_sz);
            }

            template <class PromotedFloatType = std_float_t>
            static auto get_best_generator(size_t coefficient_sz,
                                           size_t leaf_sz = 8u) -> std::unique_ptr<TemporalCoefficientProjectorGeneratorInterface>
            {
                const size_t GREEDY_FACTOR      = size_t{1} << 0;

                std::vector<std::unique_ptr<TemporalCoefficientProjectorGeneratorInterface>> base_vec{};

                for (size_t i = 0u; i < GREEDY_FACTOR; ++i)
                {
                    base_vec.push_back(get_normal_generator<PromotedFloatType>(coefficient_sz, leaf_sz));
                }

                return std::make_unique<PoolGenerator>(std::move(base_vec),
                                                       conventional_randomizer::RandomizerObject{},
                                                       coefficient_sz);
            }
    };
}

#endif