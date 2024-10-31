#pragma once

#include <chrono>
#include <iomanip>
#include <sstream>
#include <string>

#include "trftp/util.h"

namespace trftp
{

class ClientLog
{
public:
    explicit ClientLog();
    ClientLog(ClientLog &&other) noexcept;

    friend std::ostream &operator<<(std::ostream &os, const ClientLog &log);

private:
    std::string log_message_;
};

} // namespace trftp