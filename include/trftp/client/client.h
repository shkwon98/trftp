#pragma once

#include <atomic>
#include <iostream>
#include <thread>

#include "trftp/client/client_transaction.h"
#include "trftp/common.h"
#include "trftp/thread_safe_log.h"
#include "trftp/udp_socket.h"

namespace trftp
{

class Client
{
    using FileHandler = std::function<void(const std::string &, const std::uint32_t)>;

public:
    explicit Client(std::uint16_t port, std::uint32_t cur_version = 0);
    ~Client();

    void AttachFileHandler(FileHandler callback);
    void DetachFileHandler();

    void OnFileReceived(const std::string &file_path, const std::uint32_t version) const;

private:
    void HandleIncomingMessages();

    UdpSocket udp_socket_;
    std::atomic_bool is_running_;
    std::thread thread_;
    std::unique_ptr<FileHandler> file_handler_;

    ClientTransaction transaction_;
};

} // namespace trftp