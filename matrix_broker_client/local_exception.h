#ifndef __MATRIX_BROKER_CLIENT_LOCAL_EXCEPTION_H__
#define __MATRIX_BROKER_CLIENT_LOCAL_EXCEPTION_H__

#include <stdint.h>
#include <stdlib.h>
#include <expected>
#include <exception>
#include <stdexcept>

namespace matrix_broker_client
{
    using local_exception_t = uint8_t;
    
    struct other_invalid_argument: std::invalid_argument
    {
        private:

            std::string msg;

        public:

            other_invalid_argument(std::string_view msg): std::invalid_argument(""),
                                                          msg(msg){}

            virtual auto what() const noexcept -> const char *
            {
                return this->msg.c_str();
            }
    };

    struct other_runtime_error: std::runtime_error
    {
        private:

            std::string msg;

        public:

            other_runtime_error(std::string_view msg): std::runtime_error(""),
                                                       msg(msg){}

            virtual auto what() const noexcept -> const char *
            {
                return this->msg.c_str();
            }
    };

    static inline constexpr local_exception_t SUCCESS                       = 0u;
    static inline constexpr local_exception_t OTHER_INVALID_ARGUMENT_CODE   = 1u;
    static inline constexpr local_exception_t OTHER_RUNTIME_ERROR_CODE      = 2u;

    void throw_error_code(local_exception_t err_code, std::string_view msg)
    {
        switch (err_code)
        {
            case SUCCESS:
            {
                return;
            }
            case OTHER_INVALID_ARGUMENT_CODE:
            {
                throw other_invalid_argument(msg);
            }
            case OTHER_RUNTIME_ERROR_CODE:
            {
                throw other_runtime_error(msg);
            }
            default:
            {
                throw other_runtime_error(msg);
            }
        }
    }
}

#endif