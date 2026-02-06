// #include <spdlog/spdlog.h>
#define STRONG_MEMORY_ORDERING_FLAG true

#include "logging_subsystem.h"
#include <iostream>

int main()
{
    logging_subsystem::init("/Users/megazone/Downloads/SeriousBillionDollarProject/src/spdlog_test/spdlog_test.txt");

    while (true)
    {
        std::string s;
        std::cin >> s;

        logging_subsystem::log(logging_subsystem::LogFactory{}.topic("some_topic_1").info().message(s).get());
        logging_subsystem::log(logging_subsystem::LogFactory{}.info().message(s).get());
        logging_subsystem::log(logging_subsystem::LogFactory{}.topic("some_topic_1").topic("some_topic_2").critical().message(s).get());
        logging_subsystem::log(logging_subsystem::LogFactory{}.get());
    }

    // auto default_logger = spdlog::default_logger();
    // default_logger->set_pattern("%v");
    // default_logger->flush_every(std::chrono::nanoseconds(1));

    // spdlog::error("Raw output");
}