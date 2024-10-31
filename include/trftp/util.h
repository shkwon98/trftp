#pragma once

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "trftp/common.h"

namespace trftp
{

constexpr const char *RESET = "\033[0m";
constexpr const char *BLACK = "\033[30m";              /* Black */
constexpr const char *RED = "\033[31m";                /* Red */
constexpr const char *GREEN = "\033[32m";              /* Green */
constexpr const char *YELLOW = "\033[33m";             /* Yellow */
constexpr const char *BLUE = "\033[34m";               /* Blue */
constexpr const char *MAGENTA = "\033[35m";            /* Magenta */
constexpr const char *CYAN = "\033[36m";               /* Cyan */
constexpr const char *WHITE = "\033[37m";              /* White */
constexpr const char *BOLDBLACK = "\033[1m\033[30m";   /* Bold Black */
constexpr const char *BOLDRED = "\033[1m\033[31m";     /* Bold Red */
constexpr const char *BOLDGREEN = "\033[1m\033[32m";   /* Bold Green */
constexpr const char *BOLDYELLOW = "\033[1m\033[33m";  /* Bold Yellow */
constexpr const char *BOLDBLUE = "\033[1m\033[34m";    /* Bold Blue */
constexpr const char *BOLDMAGENTA = "\033[1m\033[35m"; /* Bold Magenta */
constexpr const char *BOLDCYAN = "\033[1m\033[36m";    /* Bold Cyan */
constexpr const char *BOLDWHITE = "\033[1m\033[37m";   /* Bold White */

std::string FtpStatusToString(FtpStatus status);

std::uint32_t CalculateCrc32(const std::uint8_t *buf, std::size_t size, std::uint32_t crc32 = 0U);
std::uint32_t CalculateFileCrc32(const std::string &download_file);

} // namespace trftp
