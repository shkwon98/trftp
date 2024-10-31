#pragma once

#include <chrono>
#include <iomanip>
#include <sstream>
#include <string>

#include "trftp/util.h"

namespace trftp
{

class ServerLog
{
public:
    explicit ServerLog();
    ServerLog(ServerLog &&other) noexcept;

    friend std::ostream &operator<<(std::ostream &os, const ServerLog &log);

private:
    std::string log_message_;
};

} // namespace trftp