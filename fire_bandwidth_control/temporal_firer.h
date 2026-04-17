#ifndef __FIRE_BANDWIDTH_CONTROL_TEMPORAL_FIRER_H__
#define __FIRE_BANDWIDTH_CONTROL_TEMPORAL_FIRER_H__

#include <stdint.h>
#include <stdlib.h>
#include "fireable_interface.h"
#include "fireable_firer_interface.h"
#include <chrono>
#include <common_exception/common_exception.h>
#include <common_exception/cancellation_token.h>
#include <functional>
#include <random>
#include <algorithm>
#include <utility>
#include <serializer/compact_serializer.h>
#include <stl_extension/hasher.h>
#include <thread>

namespace fire_bandwidth_control::temporal_firer
{
    using namespace fire_bandwidth_control::interface;

    struct TemporalFirerConfig
    {
        uint64_t window_population;
        std::chrono::nanoseconds window_dur;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(window_population, window_dur);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(window_population, window_dur);
        }
    };

    struct ExternalTemporalFirerConfig
    {
        std::string config_bytestream;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(config_bytestream);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(config_bytestream);
        }
    };

    auto to_external_temporal_firer_config(const TemporalFirerConfig& config) -> ExternalTemporalFirerConfig
    {
        return ExternalTemporalFirerConfig
        {
            .config_bytestream = dg::network_compact_serializer::dgstd_serialize<std::string>(config)
        };
    }

    auto to_internal_temporal_firer_config(const ExternalTemporalFirerConfig& config) -> TemporalFirerConfig
    {
        return dg::network_compact_serializer::dgstd_deserialize<TemporalFirerConfig>(config.config_bytestream);
    }

    struct RandomizationSeed
    {
        size_t time_seed;
        uintptr_t stack_seed;

        template <class Reflector>
        constexpr void dg_reflect(const Reflector& reflector) const noexcept
        {
            reflector(time_seed, stack_seed);
        }

        template <class Reflector>
        constexpr void dg_reflect(const Reflector& reflector) noexcept
        {
            reflector(time_seed, stack_seed);
        }
    };

    class TemporalFirer: public virtual FireableFirerInterface
    {
        private:

            size_t window_population;
            std::chrono::nanoseconds window_dur;

            static inline constexpr size_t MIN_WINDOW_POPULATION        = 1u;
            static inline constexpr size_t MAX_WINDOW_POPULATION        = size_t{1} << 40;

            static inline const std::chrono::nanoseconds MIN_WINDOW_DUR = std::chrono::nanoseconds(0);
            static inline const std::chrono::nanoseconds MAX_WINDOW_DUR = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::years(1));

        public:

            TemporalFirer(const TemporalFirerConfig& config)
            {
                this->window_population = std::clamp(static_cast<size_t>(config.window_population), MIN_WINDOW_POPULATION, MAX_WINDOW_POPULATION);
                this->window_dur        = std::clamp(config.window_dur, MIN_WINDOW_DUR, MAX_WINDOW_DUR);
            }

            TemporalFirer(const ExternalTemporalFirerConfig& config): TemporalFirer(to_internal_temporal_firer_config(config)){}

            void run(FireableInterface& fireable,
                     common_exception::CancellationTokenInterface& cancellation_token)
            {
                auto random_device = std::bind(std::uniform_int_distribution<size_t>{}, std::mt19937_64{this->get_seed()});
                std::chrono::time_point<std::chrono::steady_clock> window_first = std::chrono::steady_clock::now();

                while (true)
                {
                    for (size_t i = 0u; i < this->window_population; ++i)
                    {
                        if (!fireable.fire_one(cancellation_token))
                        {
                            return;
                        }
                    }

                    std::chrono::time_point<std::chrono::steady_clock> window_last = std::chrono::steady_clock::now();
                    std::chrono::nanoseconds lapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(window_last - window_first);

                    if (lapsed < this->window_dur)
                    {
                        std::chrono::nanoseconds wait_dur = this->window_dur - lapsed;
                        auto random_dur = this->get_random_wait_duration_whose_average(wait_dur * 2u, random_device);
                    
                        std::this_thread::sleep_for(random_dur);
                    }

                    window_first    = std::chrono::steady_clock::now();
                }
            }

        private:

            auto get_seed() -> size_t
            {
                char c;
                RandomizationSeed seed
                {
                    .time_seed  = static_cast<size_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count()),
                    .stack_seed = reinterpret_cast<uintptr_t>(static_cast<void *>(&c))
                };

                return hasher::hash_reflectible(seed);                
            }

            template <class Duration, class RandomDevice>
            auto get_random_wait_duration_whose_average(Duration dur,
                                                        RandomDevice&& random_device) -> Duration
            {
                size_t numerical_value  = dur.count();
                size_t range            = std::max(size_t{1}, numerical_value);

                return Duration(random_device() % range);
            }
    };
}

#endif