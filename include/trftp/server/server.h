#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>
#include <unordered_map>

#include "trftp/common.h"
#include "trftp/server/server_transaction.h"
#include "trftp/server/server_transaction_factory.h"
#include "trftp/thread_safe_log.h"
#include "trftp/udp_socket.h"

namespace trftp
{

class Server
{
public:
    explicit Server(std::uint16_t port,
                    std::shared_ptr<ServerTransactionFactory> factory = std::make_shared<DefaultServerTransactionFactory>());
    explicit Server(std::shared_ptr<ServerTransactionFactory> factory = std::make_shared<DefaultServerTransactionFactory>());
    ~Server();

    FtpStatus StartFileTransfer(const std::string &client_uri, const std::filesystem::path &file_path,
                                std::uint32_t file_version, const Device device = Device());
    void AbortFileTransfer(const std::string &client_ip);

private:
    void HandleIncomingMessages();

    UdpSocket udp_socket_;
    std::atomic_bool is_running_;
    std::thread thread_;

    std::shared_ptr<ServerTransactionFactory> factory_;
    std::unordered_map<std::string, std::shared_ptr<ServerTransaction>> active_transactions_;
};

} // namespace trftp