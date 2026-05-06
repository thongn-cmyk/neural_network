#ifndef __GLOBAL_STRING_ENCODER_GENERIC_ENCODER_H__
#define __GLOBAL_STRING_ENCODER_GENERIC_ENCODER_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <string>
#include <serializer/compact_serializer.h>
#include <variant>
#include "hex_encoder/hex_encoder.h"
#include "identity_encoder/identity_encoder.h"
#include <stl_extension/stdx.h>
#include "encoder_interface.h"

namespace global_string_encoder
{
    struct HexEncoderResource
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

    class HexEncoder: public virtual EncoderInterface
    {
        public:

            auto encode(const std::string& str) -> std::string
            {
                return global_string_encoder::hex_encoder::hex_encode<std::string>(str);
            }
    };

    struct HexDecoderResource
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

    class HexDecoder: public virtual EncoderInterface
    {
        public:

            auto encode(const std::string& str) -> std::string
            {
                return global_string_encoder::hex_encoder::hex_decode<std::string>(str);
            }
    };

    struct IdentityEncoderResource
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

    class IdentityEncoder: public virtual EncoderInterface
    {
        public:

            auto encode(const std::string& str) -> std::string
            {
                return global_string_encoder::identity_encoder::identity_encode<std::string>(str);
            }
    };

    struct IdentityDecoderResource
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

    class IdentityDecoder: public virtual EncoderInterface
    {
        public:

            auto encode(const std::string& str) -> std::string
            {
                return global_string_encoder::identity_encoder::identity_decode<std::string>(str);
            }
    };

    struct GenericEncoderResource
    {
        std::variant<stdx::reflectible_monostate,
                     HexEncoderResource,
                     HexDecoderResource,
                     IdentityEncoderResource,
                     IdentityDecoderResource> resource;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(resource);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(resource);
        }
    };

    struct ExternalGenericEncoderResource
    {
        std::string resource_bytestream;

        template <class Reflector>
        void dg_reflect(const Reflector& reflector) const
        {
            reflector(resource_bytestream);
        }

        template <class Reflector>
        void dg_reflect(const Reflector& reflector)
        {
            reflector(resource_bytestream);
        }
    };

    auto to_external_generic_encoder_resource(const GenericEncoderResource& resource) -> ExternalGenericEncoderResource
    {
        return ExternalGenericEncoderResource
        {
            .resource_bytestream = dg::network_compact_serializer::dgstd_serialize<std::string>(resource)
        };
    }

    auto to_internal_generic_encoder_resource(const ExternalGenericEncoderResource& resource) -> GenericEncoderResource
    {
        return dg::network_compact_serializer::dgstd_deserialize<GenericEncoderResource>(resource.resource_bytestream);
    }

    using StringTransformationRule = ExternalGenericEncoderResource;

    class GenericEncoder: public virtual EncoderInterface
    {
        private:

            std::unique_ptr<EncoderInterface> base;

        public:

            GenericEncoder(const GenericEncoderResource& resource)
            {
                if (std::holds_alternative<HexEncoderResource>(resource.resource))
                {
                    this->base = std::make_unique<HexEncoder>();
                }
                else if (std::holds_alternative<HexDecoderResource>(resource.resource))
                {
                    this->base = std::make_unique<HexDecoder>();
                }
                else if (std::holds_alternative<IdentityEncoderResource>(resource.resource))
                {
                    this->base = std::make_unique<IdentityEncoder>();
                }
                else if (std::holds_alternative<IdentityDecoderResource>(resource.resource))
                {
                    this->base = std::make_unique<IdentityDecoder>();
                }
                else
                {
                    throw std::invalid_argument("bad generic encoder resource, dispatch code not found");
                }
            }

            GenericEncoder(const ExternalGenericEncoderResource& resource): GenericEncoder(to_internal_generic_encoder_resource(resource)){}

            auto encode(const std::string& str) -> std::string
            {
                return this->base->encode(str);
            }
    };

    auto get_empty_transformation_rule() -> ExternalGenericEncoderResource
    {
        return to_external_generic_encoder_resource(GenericEncoderResource{.resource = IdentityEncoderResource{}});
    }
}

#endif