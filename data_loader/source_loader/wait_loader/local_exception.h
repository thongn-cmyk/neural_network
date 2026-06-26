#ifndef __DATA_LOADER_SOURCE_LOADER_WAIT_LOADER_LOCAL_EXCEPTION_H__
#define __DATA_LOADER_SOURCE_LOADER_WAIT_LOADER_LOCAL_EXCEPTION_H__

#include <stdint.h>
#include <stdlib.h>
#include <exception>
#include <stdexcept>
#include <data_loader/exception_base.h>

namespace data_loader::source_loader::wait_loader
{
    struct corrupted_loader_error: data_loader::exception_base::runtime_error_base
    {
        corrupted_loader_error(): data_loader::exception_base::runtime_error_base("bad loader, loader is in corrupted state"){}
    };
}

#endif