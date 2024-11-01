#include "trftp/udp_socket.h"

namespace trftp
{

UdpSocket::UdpSocket(std::uint16_t port)
    : fd_{ -1 }
{
    // Create a socket
    fd_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd_ < 0)
    {
        throw std::runtime_error("[UdpSocket] socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP) failed. err=" +
                                 std::to_string(errno));
    }

    // RAII pattern to ensure socket gets closed on errors
    auto socket_guard =
        std::unique_ptr<int, std::function<void(const int *)>>(&fd_, [](const int *fd) { *fd >= 0 && close(*fd); });

    // Set socket read timeout to 100 ms
    if (const timeval timeout = { .tv_sec = 0, .tv_usec = 100'000 };
        setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
    {
        throw std::runtime_error("[UdpSocket] setsockopt(SO_RCVTIMEO) failed. err=" + std::to_string(errno));
    }

    // Allow address reuse
    if (auto reuse_addr = 1; setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr)) < 0)
    {
        throw std::runtime_error("[UdpSocket] setsockopt(SO_REUSEADDR) failed. err=" + std::to_string(errno));
    }

    // Bind local address
    const sockaddr_in my_addr = {
        .sin_family = AF_INET,
        .sin_port = htobe16(port),
        .sin_addr = { .s_addr = INADDR_ANY },
    };

    if (bind(fd_, reinterpret_cast<const sockaddr *>(&my_addr), sizeof(my_addr)) < 0)
    {
        throw std::runtime_error("[UdpSocket] bind() failed. err=" + std::to_string(errno));
    }

    // Successfully opened the socket, release the guard to prevent closing
    std::ignore = socket_guard.release();
}

UdpSocket::~UdpSocket()
{
    if (fd_ >= 0)
    {
        close(fd_);
    }
}

bool UdpSocket::SetReadTimeout(std::chrono::microseconds ms) const
{
    if (fd_ < 0)
    {
        throw std::runtime_error("[UdpSocket] socket is closed");
    }

    const timeval timeout = {
        .tv_sec = (ms.count() / 1'000'000),
        .tv_usec = (ms.count() % 1'000'000),
    };

    if (setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
    {
        return false;
    }

    return true;
}

std::size_t UdpSocket::Receive(TrftpMessage &msg, sockaddr_in &addr) const
{
    socklen_t addr_len = sizeof(addr);
    auto bytes_received = recvfrom(fd_, &msg, sizeof(msg), 0, reinterpret_cast<sockaddr *>(&addr), &addr_len);

    if (bytes_received < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return 0;
        }
        if (errno == EINTR)
        {
            return 0;
        }

        throw std::runtime_error("[UdpSocket] recvfrom() failed. err=" + std::to_string(errno));
    }
    else if (bytes_received == 0)
    {
        throw std::runtime_error("[UdpSocket] recvfrom() failed. err=0");
    }

    return bytes_received;
}

std::size_t UdpSocket::Send(const TrftpMessage &msg, std::size_t len, const sockaddr_in &addr)
{
    if (fd_ < 0)
    {
        throw std::runtime_error("[UdpSocket] socket is closed");
    }

    std::scoped_lock lock(mutex_);
    auto bytes_sent = sendto(fd_, &msg, len, 0, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr));
    if (bytes_sent < 0)
    {
        throw std::runtime_error("[UdpSocket] sendto() failed. err=" + std::to_string(errno));
    }

    return bytes_sent;
}

} // namespace trftp