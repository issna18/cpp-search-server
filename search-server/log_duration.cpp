#include "log_duration.h"

LogDuration::LogDuration(const std::string& text, std::ostream& out)
    : name_func_ {text},
      out_ {out}
{

}

LogDuration::~LogDuration()
{
    using namespace std::chrono;
    using namespace std::literals;
    const auto end_time {Clock::now()};
    const auto dur {end_time - start_time_};
    out_ << name_func_ << ": "s << duration_cast<milliseconds>(dur).count() << " ms"s << std::endl;
}
