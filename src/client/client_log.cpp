#include "trftp/client/client_log.h"

namespace trftp
{

ClientLog::ClientLog()
{
    auto now = std::chrono::system_clock::now();
    auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch());
    auto now_c = std::chrono::system_clock::to_time_t(now);

    std::ostringstream os;
    os << "[TRFTP|" << BOLDMAGENTA << "C" << RESET << "] [" << BLUE << std::put_time(std::localtime(&now_c), "%H:%M:%S")
       << "." << std::setw(6) << std::setfill('0') << (now_us.count() % 1000000) << std::setfill(' ') << RESET << "] ";

    log_message_ = os.str(); // Store the generated log message.
}

ClientLog::ClientLog(ClientLog &&other) noexcept
    : log_message_(std::move(other.log_message_))
{
}

std::ostream &operator<<(std::ostream &os, const ClientLog &log)
{
    os << log.log_message_;
    return os;
}

} // namespace trftp