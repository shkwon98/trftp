#pragma once

#include <arpa/inet.h>
#include <functional>
#include <memory>
#include <mutex>
#include <unistd.h>

#include "trftp/common.h"

namespace trftp
{

class UdpSocket
{
public:
    explicit UdpSocket(std::uint16_t port = 0);
    ~UdpSocket();

    bool SetReadTimeout(std::chrono::microseconds ms) const;
    std::size_t Receive(TrftpMessage &msg, sockaddr_in &addr) const;
    std::size_t Send(const TrftpMessage &msg, std::size_t len, const sockaddr_in &addr);

private:
    int fd_;
    std::mutex mutex_;
};

} // namespace trftp