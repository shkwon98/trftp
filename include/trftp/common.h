#pragma once

#include <cstdint>

namespace trftp
{

#define TRFTP_MAGIC (0x524F424C) // ROBL

enum class MessageId : std::uint32_t
{
    NTF = 0x4500'000F,
    CHK = 0x45FD'0001,
    INFO = 0x45FD'0002,
    RDY = 0x45FD'0003,
    CXL = 0x45FD'000C,
    DATA = 0x45FD'000D,
    RTX = 0x45FD'000E,
    DONE = 0x45FD'000F,
    FIN = 0x45FD'000A
};
using FtpStatus = MessageId;

struct Device
{
    std::uint32_t id = 0x0000U;
};

// TRFTP Mesage Header
#pragma pack(push, 1)
struct TrftpHeader
{
    std::uint32_t magic;
    std::uint16_t spid;
    std::uint16_t dpid;
    std::uint32_t tpn;
    std::uint32_t tpl;
    std::uint32_t xid;
    std::uint32_t crc32;
    std::uint32_t psn;
    std::uint32_t pl;
};

struct TrftpNtf
{
    std::uint32_t new_file_version;
};

struct TrftpChk
{
    std::uint32_t cur_file_version;
};

struct TrftpInfo
{
    std::uint32_t new_file_version;
    std::uint32_t file_length;
    std::uint32_t crc32;
};

struct TrftpRdy
{
    std::uint32_t new_file_version;
    std::uint32_t file_length;
    std::uint32_t inter_packet_gap;
};

struct TrftpData
{
    char new_file_data[1408];
};

struct TrftpDone
{
    std::uint32_t new_file_version;
    std::uint32_t file_length;
    std::uint32_t crc32;
};

struct TrftpCxl
{
};

struct TrftpRtx
{
    std::uint32_t retransmit_psn;
};

struct TrftpMessage
{
    TrftpHeader header; // Access the common header

    union {
        std::uint8_t payload[1408];
        TrftpNtf ntf;   // Notification message
        TrftpChk chk;   // Check message
        TrftpInfo info; // Info message
        TrftpRdy rdy;   // Ready message
        TrftpData data; // Data message
        TrftpDone done; // Done message
        TrftpCxl cxl;   // Cancel message
        TrftpRtx rtx;   // Retransmit message
    };
};

#pragma pack(pop)

} // namespace trftp