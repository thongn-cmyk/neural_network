#ifndef __DATA_LOADER_DETACH_SOURCE_LOADER_H__
#define __DATA_LOADER_DETACH_SOURCE_LOADER_H__

namespace data_loader::source_loader::detach_loader
{
    struct DetachLoaderConfig
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

    struct ExternalDetachLoaderConfig
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