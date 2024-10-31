#pragma once

// C++ Standard Library
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <thread>
#include <unordered_map>

// TRFTP
#include "trftp/common.h"
#include "trftp/thread_safe_log.h"
#include "trftp/udp_socket.h"
#include "trftp/util.h"

namespace trftp
{

#define TRAN_IPG_MIN (100U) // 100 usec
#define TRAN_IPG_MAX (300U) // 300 usec

class ServerTransaction
{
public:
    explicit ServerTransaction(const sockaddr_in &addr, const std::filesystem::path &file_path, std::uint32_t file_version,
                               const std::uint32_t device_id);
    virtual ~ServerTransaction();
    ServerTransaction(ServerTransaction &&other) noexcept;
    ServerTransaction(const ServerTransaction &) = delete;
    ServerTransaction &operator=(const ServerTransaction &) = delete;

    void SendMessage(MessageId id, UdpSocket &udp_socket);
    void OnReceive(const TrftpMessage &msg, std::size_t len, const sockaddr_in &addr);
    std::optional<FtpStatus> WaitForStatus(std::chrono::seconds timeout);

protected:
    virtual void CompleteHeader(TrftpMessage &msg, const MessageId xid, const std::uint32_t tpl,
                                const std::uint32_t psn) const;
    virtual bool ValidateMessageIntegrity(const TrftpMessage &msg, std::size_t len) const;
    virtual bool ValidateMessage(const TrftpChk &payload, std::size_t payload_len) const;
    virtual bool ValidateMessage(const TrftpRdy &payload, std::size_t payload_len) const;
    virtual bool ValidateMessage(const TrftpDone &payload, std::size_t payload_len, const TrftpDone &expected) const;
    virtual bool ValidateMessage(const TrftpCxl &payload, std::size_t payload_len) const;
    virtual bool ValidateMessage(const TrftpRtx &payload, std::size_t payload_len, const TrftpRtx &expected) const;

private:
    void SendFileAsync(UdpSocket &udp_socket);

    void PrintRecvLog(const TrftpMessage &msg) const;
    void PrintSendLog(const TrftpMessage &msg) const;

    // Informations about the file to be sent
    std::filesystem::path file_path_;
    std::uint32_t new_file_version_;
    std::uint32_t new_file_size_;
    std::uint32_t new_file_crc32_;

    std::atomic<FtpStatus> status_;
    std::uint32_t device_id_;
    sockaddr_in client_address_;

    std::uint32_t total_packet_number_;                 // (file-length / 1408) [+1]
    std::atomic<std::uint32_t> packet_sequence_number_; // [0..(tpn-1)]
    std::atomic<std::uint32_t> retransmit_psn_;         // default:-1, [0..(tpn-1)] but must less than 'psn'

    // Data informed from the client
    std::uint32_t cur_file_version_;             // From CHK message
    std::chrono::microseconds inter_packet_gap_; // From RDY message

    std::mutex mtx_;
    std::condition_variable cv_;
    std::thread thr_;
};

} // namespace trftp