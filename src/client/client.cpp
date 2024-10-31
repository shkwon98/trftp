#include "trftp/client/client.h"
#include "trftp/client/client_log.h"

namespace trftp
{

Client::Client(std::uint16_t port, std::uint32_t cur_version)
    : udp_socket_(port)
    , is_running_(true)
    , thread_(&Client::HandleIncomingMessages, this)
    , file_handler_(nullptr)
    , transaction_(this, cur_version)
{
}

Client::~Client()
{
    is_running_ = false;

    if (thread_.joinable())
    {
        thread_.join();
    }
}

void Client::AttachFileHandler(FileHandler callback)
{
    file_handler_ = std::make_unique<FileHandler>(std::move(callback));
}

void Client::DetachFileHandler()
{
    file_handler_.reset();
}

void Client::OnFileReceived(const std::string &file_path, const std::uint32_t version) const
{
    if (file_handler_)
    {
        (*file_handler_)(file_path, version);
    }
}

void Client::HandleIncomingMessages()
{
    while (is_running_)
    {
        TrftpMessage msg;
        sockaddr_in server_addr;

        auto len = udp_socket_.Receive(msg, server_addr);
        if (len == 0)
        {
            continue;
        }

        if (transaction_.IsAlive())
        {
            terr << ClientLog() << "Transaction is already in progress. Discarding..." << std::endl;
        }
        else
        {
            transaction_.Begin(msg, len, server_addr);
        }
    }
}

} // namespace trftp