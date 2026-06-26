#ifndef __DATA_LOADER_SOURCE_KAFKA_BROKER_SOURCE_MODEL_H__
#define __DATA_LOADER_SOURCE_KAFKA_BROKER_SOURCE_MODEL_H__

#include <stdint.h>
#include <stdlib.h>

namespace data_loader::source::kafka_broker_source
{
    struct KafkaBrokerConfig
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

    struct ExternalKafkaBrokerConfig
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
}

#endif