#pragma once

#include <chrono>
#include <iostream>
#include <string>

class LogDuration {

public:
    using Clock = std::chrono::steady_clock;

    explicit LogDuration(const std::string& text, std::ostream& out = std::cerr);
    virtual ~LogDuration();

private:
    const Clock::time_point start_time_ {Clock::now()};
    const std::string name_func_;
    std::ostream& out_;
};

#define PROFILE_CONCAT_INTERNAL(X, Y) X ## Y
#define PROFILE_CONCAT(X, Y) PROFILE_CONCAT_INTERNAL(X, Y)
#define MAKE_UNIQUE_LOGGER PROFILE_CONCAT(profileGuard, __LINE__)
#define LOG_DURATION(text) LogDuration MAKE_UNIQUE_LOGGER(text)
#define LOG_DURATION_STREAM(text, stream) LogDuration MAKE_UNIQUE_LOGGER(text, stream)
