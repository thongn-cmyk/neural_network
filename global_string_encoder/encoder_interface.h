#ifndef __GLOBAL_STRING_ENCODER_ENCODER_INTERFACE_H__
#define __GLOBAL_STRING_ENCODER_ENCODER_INTERFACE_H__

namespace global_string_encoder
{
    class EncoderInterface
    {
        public:

            virtual ~EncoderInterface() noexcept = default;

            virtual auto encode(const std::string& str) -> std::string = 0;
    };
}

#endif