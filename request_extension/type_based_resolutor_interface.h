#ifndef __TYPE_BASED_RESOLUTOR_INTERFACE_H__
#define __TYPE_BASED_RESOLUTOR_INTERFACE_H__

namespace request_extension::resolutor
{
    template <class T_In, class T_Out>
    class TypeBasedResolutorInterface
    {
        public:

            virtual ~TypeBasedResolutorInterface() noexcept = default;

            virtual auto handle(const T_In& inp) -> T_Out = 0;
    };
}

#endif