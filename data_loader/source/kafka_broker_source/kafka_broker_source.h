#ifndef __DATASOURCE_KAFKA_BROKER_SOURCE_H__
#define __DATASOURCE_KAFKA_BROKER_SOURCE_H__

#include <stdint.h>
#include <stdlib.h>

namespace data_loader::kafka_broker_source
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