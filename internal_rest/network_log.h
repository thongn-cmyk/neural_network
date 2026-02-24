#ifndef __NETWORK_LOGGER_H__
#define __NETWORK_LOGGER_H__

//define HEADER_CONTROL 7 

#include <string>
#include <filesystem> 
#include <fstream>
#include <utility>
#include <algorithm>
#include <format>
#include <memory>
#include <array>
#include <string_view>
#include <mutex>
#include "stdx.h"
#include <thread>
#include "network_std_container.h"
#include <stacktrace>
#include "network_logging_subsystem.h"
#include <filesystem>

namespace dg_sock::network_log
{
    void init(const std::filesystem::path& file_path, bool is_debug_mode = false, bool flush_on_error = false)
    {
        dg_sock::network_logging_subsystem::init(file_path, is_debug_mode, flush_on_error);
    }

    void deinit() noexcept
    {    
        dg_sock::network_logging_subsystem::deinit();
    }

    void critical(const char * what) noexcept
    {
        dg_sock::network_logging_subsystem::noexcept_log(dg_sock::network_logging_subsystem::LogFactory{}.message(what).critical());
    }

    void error(const char * what) noexcept
    {
        dg_sock::network_logging_subsystem::noexcept_log(dg_sock::network_logging_subsystem::LogFactory{}.message(what).error());
    }

    void error_fast(const char * what) noexcept
    {
        dg_sock::network_logging_subsystem::noexcept_log(dg_sock::network_logging_subsystem::LogFactory{}.message(what).error());
    }
    
    void error_optional(const char * what) noexcept
    {
        dg_sock::network_logging_subsystem::noexcept_log(dg_sock::network_logging_subsystem::LogFactory{}.message(what).error());
    }

    void error_fast_optional(const char * what) noexcept
    {
        dg_sock::network_logging_subsystem::noexcept_log(dg_sock::network_logging_subsystem::LogFactory{}.message(what).error());
    }

    void journal(const char * what) noexcept
    {
        dg_sock::network_logging_subsystem::noexcept_log(dg_sock::network_logging_subsystem::LogFactory{}.message(what).info());
    }

    void journal_fast(const char * what) noexcept
    {
        dg_sock::network_logging_subsystem::noexcept_log(dg_sock::network_logging_subsystem::LogFactory{}.message(what).info());
    }

    void journal_optional(const char * what) noexcept
    {
        dg_sock::network_logging_subsystem::noexcept_log(dg_sock::network_logging_subsystem::LogFactory{}.message(what).info());
    }

    void journal_fast_optional(const char * what) noexcept
    {
        dg_sock::network_logging_subsystem::noexcept_log(dg_sock::network_logging_subsystem::LogFactory{}.message(what).info());
    }
}

namespace dg_sock::network_log_stackdump
{
    void critical(const char * what) noexcept
    {
        dg_sock::network_logging_subsystem::noexcept_log(dg_sock::network_logging_subsystem::LogFactory{}.message(what).critical().add_stacktrace());
    }

    void critical() noexcept
    {
        dg_sock::network_logging_subsystem::noexcept_log(dg_sock::network_logging_subsystem::LogFactory{}.critical().add_stacktrace());
    }
    
    void error(const char * what) noexcept
    {
        dg_sock::network_logging_subsystem::noexcept_log(dg_sock::network_logging_subsystem::LogFactory{}.message(what).error().add_stacktrace());
    }

    void error() noexcept
    {
        dg_sock::network_logging_subsystem::noexcept_log(dg_sock::network_logging_subsystem::LogFactory{}.error().add_stacktrace());
    }

    void error_fast(const char * what) noexcept
    {
        dg_sock::network_logging_subsystem::noexcept_log(dg_sock::network_logging_subsystem::LogFactory{}.message(what).error().add_stacktrace());
    }
    
    void error_fast() noexcept
    {
        dg_sock::network_logging_subsystem::noexcept_log(dg_sock::network_logging_subsystem::LogFactory{}.error().add_stacktrace());
    }

    void error_optional(const char * what) noexcept
    {
        dg_sock::network_logging_subsystem::noexcept_log(dg_sock::network_logging_subsystem::LogFactory{}.message(what).error().add_stacktrace());
    }

    void error_optional() noexcept
    {
        dg_sock::network_logging_subsystem::noexcept_log(dg_sock::network_logging_subsystem::LogFactory{}.error().add_stacktrace());
    }

    void error_fast_optional(const char * what) noexcept
    {
        dg_sock::network_logging_subsystem::noexcept_log(dg_sock::network_logging_subsystem::LogFactory{}.message(what).error().add_stacktrace());
    }

    void error_fast_optional() noexcept
    {
        dg_sock::network_logging_subsystem::noexcept_log(dg_sock::network_logging_subsystem::LogFactory{}.error().add_stacktrace());
    }

    void journal(const char * what) noexcept
    {
        dg_sock::network_logging_subsystem::noexcept_log(dg_sock::network_logging_subsystem::LogFactory{}.message(what).info().add_stacktrace());
    }

    void journal() noexcept
    {
        dg_sock::network_logging_subsystem::noexcept_log(dg_sock::network_logging_subsystem::LogFactory{}.info().add_stacktrace());
    }

    void journal_fast(const char * what) noexcept
    {
        dg_sock::network_logging_subsystem::noexcept_log(dg_sock::network_logging_subsystem::LogFactory{}.message(what).info().add_stacktrace());
    }

    void journal_fast() noexcept
    {
        dg_sock::network_logging_subsystem::noexcept_log(dg_sock::network_logging_subsystem::LogFactory{}.info().add_stacktrace());
    }

    void journal_optional(const char * what) noexcept
    {
        dg_sock::network_logging_subsystem::noexcept_log(dg_sock::network_logging_subsystem::LogFactory{}.message(what).info().add_stacktrace());
    }

    void journal_optional() noexcept
    {
        dg_sock::network_logging_subsystem::noexcept_log(dg_sock::network_logging_subsystem::LogFactory{}.info().add_stacktrace());
    }

    void journal_fast_optional(const char * what) noexcept
    {
        dg_sock::network_logging_subsystem::noexcept_log(dg_sock::network_logging_subsystem::LogFactory{}.message(what).info().add_stacktrace());
    }

    void journal_fast_optional() noexcept
    {
        dg_sock::network_logging_subsystem::noexcept_log(dg_sock::network_logging_subsystem::LogFactory{}.info().add_stacktrace());
    }
}

#endif