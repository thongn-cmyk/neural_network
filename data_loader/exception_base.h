#ifndef __DATA_LOADER_EXCEPTION_BASE_H__
#define __DATA_LOADER_EXCEPTION_BASE_H__

#include <stdint.h>
#include <stdlib.h>
#include <stdexcept>
#include <exception>
#include <string_view>

namespace data_loader::exception_base
{
    struct invalid_argument_base: std::invalid_argument
    {
        private:

            const char * id;

        public:

            invalid_argument_base(const char * err_msg,
                                  const char * id = "invalid_argument_base"): std::invalid_argument(err_msg),
                                                                              id(id){}

            auto string_id() const noexcept -> std::string_view
            {
                return this->id;
            }
    };

    struct runtime_error_base: std::runtime_error
    {
        private:

            const char * id;
        
        public:

            runtime_error_base(const char * err_msg,
                               const char * id = "runtime_error_base"): std::runtime_error(err_msg),
                                                                        id(id){}

            auto string_id() const noexcept -> std::string_view
            {
                return this->id;
            }
    };

    struct retryable_error: runtime_error_base
    {
        retryable_error(const char * err_msg,
                        const char * id = "retryable_error"): runtime_error_base(err_msg, id){}
    };

    auto get_exception_id(std::exception_ptr exception) -> std::string_view
    {
        try
        {
            std::rethrow_exception(exception);
        }
        catch (invalid_argument_base& e)
        {
            return e.string_id();
        }
        catch (runtime_error_base& e)
        {
            return e.string_id();
        }
        catch (std::exception& e)
        {
            return std::string_view("__unidentified_exception");
        }
    }
}

#endif