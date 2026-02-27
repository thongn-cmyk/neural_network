#ifndef __LOGGING_SUBSYSTEM_H__
#define __LOGGING_SUBSYSTEM_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <spdlog/spdlog.h>
#include <filesystem>
#include "stdx.h"
#include <format>
#include <string>
#include <spdlog/sinks/rotating_file_sink.h>

namespace logging_subsystem
{
    struct Signature{};

    struct bad_msg_log_topic: std::invalid_argument
    {
        bad_msg_log_topic(): std::invalid_argument("bad log message, msg kind enumeration out of range"){}
    };

    //this is hard because shared pointer is not thread safe, and access to such (that is not const reference& -> ) would be undefined, not many people would get this

    using SingletonContainer = stdx::singleton_container<std::shared_ptr<spdlog::logger>, Signature>;

    static inline constexpr uint8_t MSG_KIND_DEBUG              = 0u;
    static inline constexpr uint8_t MSG_KIND_INFO               = 1u;
    static inline constexpr uint8_t MSG_KIND_WARN               = 2u;
    static inline constexpr uint8_t MSG_KIND_ERROR              = 3u;
    static inline constexpr uint8_t MSG_KIND_CRITICAL           = 4u;

    static inline constexpr uint64_t LOG_BYTE_SIZE_PER_FILE     = 1024ULL * 1024ULL * 10ULL;
    static inline constexpr uint64_t LOG_FILE_COUNT             = 1u;

    static inline const std::chrono::nanoseconds FLUSH_DURATION = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(1));

    struct GenericLogMessage
    {
        uint8_t msg_kind;
        std::string msg;
    };

    void init(const std::filesystem::path& file_path, bool is_debug_mode = false)
    {
        SingletonContainer::get() = spdlog::rotating_logger_mt("basic_logger", file_path.native(), LOG_BYTE_SIZE_PER_FILE, LOG_FILE_COUNT);
        SingletonContainer::get()->set_pattern("%v");
        SingletonContainer::get()->flush_on(spdlog::level::err);

        spdlog::flush_every(FLUSH_DURATION);

        if (is_debug_mode)
        {
            spdlog::set_level(spdlog::level::debug);
        }
        else
        {
            spdlog::set_level(spdlog::level::info);
        }

        std::atomic_thread_fence(std::memory_order_seq_cst);
    }

    void deinit() noexcept
    {
        SingletonContainer::get() = nullptr;
    }

    void log(const GenericLogMessage& msg)
    {
        switch (msg.msg_kind)
        {
            case MSG_KIND_DEBUG:
            {
                SingletonContainer::get()->debug(msg.msg);
                return;
            }
            case MSG_KIND_INFO:
            {
                SingletonContainer::get()->info(msg.msg);
                return;
            }
            case MSG_KIND_WARN:
            {
                SingletonContainer::get()->warn(msg.msg);
                return;
            }
            case MSG_KIND_ERROR:
            {
                SingletonContainer::get()->error(msg.msg);
                return;
            }
            case MSG_KIND_CRITICAL:
            {
                SingletonContainer::get()->critical(msg.msg);
                return;
            }
            default:
            {
                throw bad_msg_log_topic{};
            }
        }
    }

    void noexcept_log(const GenericLogMessage& msg) noexcept
    {
        try
        {
            logging_subsystem::log(msg);
        }
        catch (const bad_msg_log_topic& e)
        {
            std::abort();
        }
        catch (const std::exception& e){}
    }

    class LogFactory
    {
        private:

            std::vector<std::string> topic_vec;
            std::exception_ptr custom_exception_ptr;
            std::string custom_log_msg;
            bool has_timestamp;
            uint8_t log_kind;
            bool has_stacktrace;

        public:

            LogFactory(): topic_vec(),
                          custom_exception_ptr(nullptr),
                          custom_log_msg(),
                          has_timestamp(true),
                          log_kind(MSG_KIND_INFO),
                          has_stacktrace(false){}

            auto topic(std::string_view topic) -> LogFactory&
            {
                this->topic_vec.push_back(std::string(topic));

                return *this;
            }

            auto message(std::exception_ptr exception) -> LogFactory&
            {
                this->custom_exception_ptr = exception;

                return *this;
            }

            auto message(std::string_view msg) -> LogFactory&
            {
                this->custom_log_msg = msg;

                return *this;
            }

            auto add_stacktrace() -> LogFactory&
            {
                this->has_stacktrace = true;

                return *this;
            }

            auto disable_timestamp() -> LogFactory&
            {
                this->has_timestamp = false;

                return *this;
            }

            auto warn() -> LogFactory&
            {
                this->log_kind = MSG_KIND_WARN;

                return *this;
            }

            auto critical() -> LogFactory&
            {
                this->log_kind = MSG_KIND_CRITICAL;

                return *this;
            }

            auto debug() -> LogFactory&
            {
                this->log_kind = MSG_KIND_DEBUG;

                return *this;
            }

            auto info() -> LogFactory&
            {
                this->log_kind = MSG_KIND_INFO;

                return *this;
            }

            auto error() -> LogFactory&
            {
                this->log_kind = MSG_KIND_ERROR;

                return *this;
            }

            auto get() -> GenericLogMessage
            {
                return GenericLogMessage
                {
                    .msg_kind   = this->log_kind,
                    .msg        = this->prettify()
                };
            }

        private:

            auto format_timestamp() -> std::string
            {
                std::chrono::seconds since_epoch(0);

                if (this->has_timestamp)
                {
                    since_epoch = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch());
                }

                return std::format("{} seconds_since_epoch", std::to_string(static_cast<uint64_t>(since_epoch.count())));
            }

            auto format_logkind() -> std::string
            {
                switch (this->log_kind)
                {
                    case MSG_KIND_DEBUG:
                    {
                        return "debug";
                    }
                    case MSG_KIND_INFO:
                    {
                        return "info";
                    }
                    case MSG_KIND_WARN:
                    {
                        return "warn";
                    }
                    case MSG_KIND_ERROR:
                    {
                        return "error";
                    }
                    case MSG_KIND_CRITICAL:
                    {
                        return "critical";
                    }
                    default:
                    {
                        std::abort();
                    }
                }
            }

            auto format_topic() -> std::string
            {
                std::string result = "<begin_topic>";

                for (size_t i = 0u; i < this->topic_vec.size(); ++i)
                {
                    result = result + std::format("<begin_subtopic_{}>", std::to_string(i));
                    result = result + this->topic_vec[i];
                    result = result + std::format("<end_subtopic_{}>", std::to_string(i));
                }

                result = result + "<end_topic>";

                return result;
            }

            auto format_content() -> std::string
            {
                return this->custom_log_msg;
            }

            auto prettify() -> std::string
            {
                return std::format("{},{},{}{}", this->format_timestamp(), this->format_logkind(), this->format_topic(), this->format_content());
            }
    };
}

#endif