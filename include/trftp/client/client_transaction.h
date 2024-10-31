#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <thread>

#include "trftp/common.h"
#include "trftp/thread_safe_log.h"
#include "trftp/udp_socket.h"
#include "trftp/util.h"

namespace trftp
{

using namespace std::chrono_literals;

class Client;

class ClientTransaction
{
public:
    explicit ClientTransaction(Client *client, std::uint32_t file_version);
    ~ClientTransaction();
    ClientTransaction(const ClientTransaction &) = delete;
    ClientTransaction &operator=(const ClientTransaction &) = delete;

    bool IsAlive() const;
    void Begin(const TrftpMessage &msg, std::size_t len, const sockaddr_in &addr);

private:
    void Reset();
    void HandleIncomingMessages();
    void OnReceive(const TrftpMessage &msg, std::size_t len, const sockaddr_in &addr);
    void SendMessage(MessageId id);
    bool ValidateMessageIntegrity(const TrftpMessage &msg, std::size_t len) const;

    void PrintRecvLog(const TrftpMessage &msg) const;
    void PrintSendLog(const TrftpMessage &msg) const;

    Client *client_;

    std::uint32_t cur_file_version_;
    std::chrono::microseconds inter_packet_gap_;

    std::atomic_bool is_active_;
    std::atomic<FtpStatus> status_;
    sockaddr_in server_address_;
    UdpSocket udp_socket_;
    std::thread thread_;

    std::uint32_t total_packet_number_;                 // (file-length / 1408) [+1]
    std::atomic<std::uint32_t> packet_sequence_number_; // [0..(tpn-1)]
    std::ofstream new_file_stream_;
    std::filesystem::path new_file_path_;

    // Data informed from the server
    std::uint32_t new_file_version_; // From NTF message
    std::uint32_t new_file_size_;    // From INFO message
    std::uint32_t new_file_crc32_;   // From INFO message
};

} // namespace trftp