#ifndef __DEVIATION_PROJECTION_CLIENT_LOCAL_EXCEPTION_H__
#define __DEVIATION_PROJECTION_CLIENT_LOCAL_EXCEPTION_H__

#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <string_view>
#include <exception>
#include <stdexcept>

namespace deviation_projection_client
{
    using local_exception_t = uint8_t;

    static inline constexpr local_exception_t SUCCESS                               = 0u;
    static inline constexpr local_exception_t SERVER_INVALID_ARGUMENT_ERROR_CODE    = 1u;
    static inline constexpr local_exception_t SERVER_RUNTIME_ERROR_CODE             = 2u;
    static inline constexpr local_exception_t CLIENT_NOT_FOUND_ERROR_CODE           = 3u;
    static inline constexpr local_exception_t INOPERABLE_CLIENT_ERROR_CODE          = 4u;

    struct server_invalid_argument: std::invalid_argument
    {
        private:

            std::string msg;
        
        public:

            server_invalid_argument(std::string_view msg_arg): std::invalid_argument("invalid argument"),
                                                               msg(msg_arg){}

            server_invalid_argument(): server_invalid_argument("invalid argument"){}

            virtual auto what() const noexcept -> const char *
            {
                return this->msg.c_str();
            }
    };

    struct server_runtime_error: std::runtime_error
    {
        private:

            std::string msg;

        public:

            server_runtime_error(std::string_view msg_arg): std::runtime_error("runtime error"),
                                                            msg(msg_arg){}

            server_runtime_error(): server_runtime_error("runtime error"){}

            virtual auto what() const noexcept -> const char *
            {
                return this->msg.c_str();
            }
    };

    struct server_client_not_found_error: std::invalid_argument
    {
        private:

            std::string msg;
        
        public:

            server_client_not_found_error(std::string_view msg_arg): std::invalid_argument("invalid argument"),
                                                                     msg(msg_arg){}

            server_client_not_found_error(): server_client_not_found_error("bad client_box, client_box id not found"){}

            virtual auto what() const noexcept -> const char *
            {
                return this->msg.c_str();
            }
    };

    struct inoperable_client_error: std::runtime_error
    {
        private:

            std::string msg;

        public:

            inoperable_client_error(std::string_view msg_arg): std::runtime_error("runtime error"),
                                                               msg(msg_arg){}

            inoperable_client_error(): inoperable_client_error("corrupted client, client is in inoperatable state"){}

            virtual auto what() const noexcept -> const char *
            {
                return this->msg.c_str();
            }
    };

    void throw_error_code(local_exception_t err_code, std::string_view msg_arg)
    {
        switch (err_code)
        {
            case SUCCESS:
            {
                break;
            }
            case SERVER_INVALID_ARGUMENT_ERROR_CODE:
            {
                throw server_invalid_argument(msg_arg);
            }
            case SERVER_RUNTIME_ERROR_CODE:
            {
                throw server_runtime_error(msg_arg);
            }
            case CLIENT_NOT_FOUND_ERROR_CODE:
            {
                throw server_client_not_found_error(msg_arg);
            }
            case INOPERABLE_CLIENT_ERROR_CODE:
            {
                throw inoperable_client_error(msg_arg);
            }
            default:
            {
                throw std::runtime_error("invalid error code");
            }
        }
    }
}

#endif