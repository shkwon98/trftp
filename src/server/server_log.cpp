#include "trftp/server/server_log.h"

namespace trftp
{

ServerLog::ServerLog()
{
    auto now = std::chrono::system_clock::now();
    auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch());
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);

    std::ostringstream os;
    os << "[TRFTP|" << BOLDCYAN << "S" << RESET << "] [" << BLUE << std::put_time(std::localtime(&now_c), "%H:%M:%S") << "."
       << std::setw(6) << std::setfill('0') << (now_us.count() % 1000000) << std::setfill(' ') << RESET << "] ";

    log_message_ = os.str(); // Store the generated log message.
}

ServerLog::ServerLog(ServerLog &&other) noexcept
    : log_message_(std::move(other.log_message_))
{
}

std::ostream &operator<<(std::ostream &os, const ServerLog &log)
{
    os << log.log_message_;
    return os;
}

} // namespace trftp