#include "trftp/server/server.h"
#include "trftp/server/server_log.h"

namespace trftp
{

using namespace std::chrono_literals;

Server::Server(std::uint16_t port, std::shared_ptr<ServerTransactionFactory> factory)
    : udp_socket_(port)
    , is_running_(true)
    , thread_(&Server::HandleIncomingMessages, this)
    , factory_(std::move(factory))
{
}

Server::Server(std::shared_ptr<ServerTransactionFactory> factory)
    : Server(64920, std::move(factory))
{
}

Server::~Server()
{
    is_running_ = false;

    if (thread_.joinable())
    {
        thread_.join();
    }
}

FtpStatus Server::StartFileTransfer(const std::string &client_uri, const std::filesystem::path &file_path,
                                    std::uint32_t file_version, const Device device)
{
    auto pos = client_uri.find(':');
    if (pos == std::string::npos)
    {
        throw std::runtime_error("Invalid client address (" + client_uri + ")");
    }

    const auto &client_ip = client_uri.substr(0, pos);
    const auto &client_port = client_uri.substr(pos + 1);

    if (client_ip.size() >= INET_ADDRSTRLEN)
    {
        throw std::runtime_error("Invalid client IP (" + client_ip + ")");
    }

    if (!std::filesystem::exists(file_path))
    {
        throw std::runtime_error("File(" + file_path.string() + ") not found");
    }

    sockaddr_in client_addr;
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htobe16(std::stoi(client_port));
    client_addr.sin_addr.s_addr = inet_addr(client_ip.c_str());

    auto tran = factory_->CreateTransaction(device, client_addr, file_path, file_version);

    if (auto [it, inserted] = active_transactions_.try_emplace(client_ip, tran); !inserted)
    {
        throw std::runtime_error("Transaction already exists for <" + client_ip + ">");
    }

    tran->SendMessage(MessageId::NTF, udp_socket_);
    auto status = tran->WaitForStatus(1s);
    if (!status) // if client is not responding (e.g. not exist)
    {
        active_transactions_.erase(client_ip);
        return FtpStatus::NTF;
    }
    else if (status == FtpStatus::CXL)
    {
        active_transactions_.erase(client_ip);
        return FtpStatus::CXL;
    }
    else if (status != FtpStatus::CHK)
    {
        tran->SendMessage(MessageId::CXL, udp_socket_);
        active_transactions_.erase(client_ip);
        return FtpStatus::CXL;
    }

    tran->SendMessage(MessageId::INFO, udp_socket_);
    status = tran->WaitForStatus(1s);
    if (status == FtpStatus::CXL)
    {
        active_transactions_.erase(client_ip);
        return FtpStatus::CXL;
    }
    else if (status != FtpStatus::RDY)
    {
        tran->SendMessage(MessageId::CXL, udp_socket_);
        active_transactions_.erase(client_ip);
        return FtpStatus::CXL;
    }

    tran->SendMessage(MessageId::DATA, udp_socket_);
    status = tran->WaitForStatus(5min);
    if (status == FtpStatus::CXL)
    {
        active_transactions_.erase(client_ip);
        return FtpStatus::CXL;
    }
    else if (status != FtpStatus::DONE)
    {
        tran->SendMessage(MessageId::CXL, udp_socket_);
        active_transactions_.erase(client_ip);
        return FtpStatus::CXL;
    }

    tran->SendMessage(MessageId::FIN, udp_socket_);
    active_transactions_.erase(client_ip);
    return FtpStatus::FIN;
}

void Server::AbortFileTransfer(const std::string &client_ip)
{
    auto it = active_transactions_.find(client_ip);
    if (it == active_transactions_.end())
    {
        throw std::runtime_error("No transaction found for <" + client_ip + ">");
    }

    it->second->SendMessage(MessageId::CXL, udp_socket_);
}

void Server::HandleIncomingMessages()
{
    while (is_running_)
    {
        TrftpMessage msg;
        sockaddr_in client_addr;

        auto len = udp_socket_.Receive(msg, client_addr);
        if (len == 0)
        {
            continue;
        }

        auto it = active_transactions_.find(inet_ntoa(client_addr.sin_addr));
        if (it == active_transactions_.end())
        {
            terr << ServerLog() << "No transaction found for <" << inet_ntoa(client_addr.sin_addr) << ":"
                 << ntohs(client_addr.sin_port) << ">" << std::endl;
            continue;
        }

        it->second->OnReceive(msg, len, client_addr);
    }
}

} // namespace trftp