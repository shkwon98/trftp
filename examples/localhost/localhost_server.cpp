#include <filesystem>
#include <iostream>

#include <trftp/server/server.h>

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << " <file_path> <client_port>" << std::endl;
        return EXIT_FAILURE;
    }

    const auto file_path = std::filesystem::path(argv[1]);
    const auto client_port = std::stoi(argv[2]);
    const auto client_uri = "127.0.0.1:" + std::to_string(client_port);

    if (!std::filesystem::exists(file_path))
    {
        std::cerr << "File does not exist: " << file_path << std::endl;
        return EXIT_FAILURE;
    }
    if (!std::filesystem::is_regular_file(file_path))
    {
        std::cerr << "Not a regular file: " << file_path << std::endl;
        return EXIT_FAILURE;
    }

    if (client_port < 1024 || client_port > 65535)
    {
        std::cerr << "Invalid port: " << client_port << std::endl;
        return EXIT_FAILURE;
    }

    trftp::Server server;

    std::cout << "Starting file transfer to " << client_uri << "..." << std::endl;
    const auto result = server.StartFileTransfer(client_uri, file_path, 0);

    if (result == trftp::FtpStatus::NTF)
    {
        std::cerr << "Cannot find client: " << client_uri << std::endl;
        return EXIT_FAILURE;
    }
    else if (result == trftp::FtpStatus::CXL)
    {
        std::cerr << "Transfer cancelled by error" << std::endl;
        return EXIT_FAILURE;
    }
    else
    {
        std::cout << "Successfully transferred file to " << client_uri << std::endl;
    }

    return EXIT_SUCCESS;
}