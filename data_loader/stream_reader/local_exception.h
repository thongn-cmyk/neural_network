#ifndef __DATA_LOADER_STREAM_READER_LOCAL_EXCEPTION_H__
#define __DATA_LOADER_STREAM_READER_LOCAL_EXCEPTION_H__

#include <stdexcept>
#include <exception>
#include <data_loader/exception_base.h>

namespace data_loader::stream_reader
{
    struct token_overflow_error: data_loader::exception_base::runtime_error_base
    {
        token_overflow_error(): data_loader::exception_base::runtime_error_base("max token size reached", "token_overflow_error"){}
    };
}

#endif