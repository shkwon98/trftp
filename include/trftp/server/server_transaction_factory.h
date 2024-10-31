#pragma once

#include <filesystem>
#include <memory>
#include <netinet/in.h>

#include "trftp/common.h"

namespace trftp
{

class ServerTransaction; // Forward declaration

class ServerTransactionFactory
{
public:
    virtual ~ServerTransactionFactory() = default;
    virtual std::shared_ptr<ServerTransaction> CreateTransaction(const trftp::Device device, const sockaddr_in &addr,
                                                                 const std::filesystem::path &file_path,
                                                                 std::uint32_t file_version) = 0;
};

class DefaultServerTransactionFactory : public ServerTransactionFactory
{
public:
    std::shared_ptr<ServerTransaction> CreateTransaction(const trftp::Device device, const sockaddr_in &addr,
                                                         const std::filesystem::path &file_path,
                                                         std::uint32_t file_version) override
    {
        return std::make_shared<ServerTransaction>(addr, file_path, file_version, device.id);
    }
};

} // namespace trftp