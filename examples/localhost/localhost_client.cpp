#include <condition_variable>
#include <filesystem>
#include <iostream>
#include <mutex>

#include <trftp/client/client.h>

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <port>" << std::endl;
        return EXIT_FAILURE;
    }

    const auto port = std::stoi(argv[1]);

    if (port < 1024 || port > 65535)
    {
        std::cerr << "Invalid port: " << port << std::endl;
        return EXIT_FAILURE;
    }

    trftp::Client client(port);
    std::cout << "Waiting for file transfer..." << std::endl;

    std::condition_variable cv;
    std::mutex m;

    client.AttachFileHandler([&cv](const std::string &file_path, const std::uint32_t version) {
        std::filesystem::path rx_path = "trftp_file";
        std::filesystem::rename(file_path, rx_path);
        std::cout << "File received: " << rx_path << " (version: " << version << ")" << std::endl;
        cv.notify_one();
    });

    if (std::unique_lock lock(m); cv.wait_for(lock, std::chrono::seconds(30)) == std::cv_status::timeout)
    {
        std::cerr << "Timeout" << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}