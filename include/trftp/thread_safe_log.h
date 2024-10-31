#pragma once

#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>

namespace trftp
{

#define terr             ThreadStream(std::cerr)
#define tout             ThreadStream(std::cout)
#define tfout(file_path) ThreadStream::CreateFileStream(file_path)

/**
 * Thread-safe std::ostream class with file support.
 * Each output type (cout, cerr, and different files) uses its own mutex.
 */
class ThreadStream : public std::ostringstream
{
public:
    ThreadStream(const ThreadStream &) = delete;
    ThreadStream &operator=(const ThreadStream &) = delete;
    ThreadStream(ThreadStream &&) = delete;
    ThreadStream &operator=(ThreadStream &&) = delete;

    // Constructor for console streams (std::cout, std::cerr)
    explicit ThreadStream(std::ostream &os);

    // Destructor
    ~ThreadStream() override;

    // Constructor for file output (compile-time string enforcement using constexpr)
    template <size_t N, typename = std::enable_if_t<N >= 2 && std::is_array_v<char[N]>>>
    static constexpr ThreadStream CreateFileStream(const char (&file_path)[N])
    {
        static_assert(N > 1, "File path cannot be empty.");
        return ThreadStream(std::string(file_path));
    }

private:
    // Constructor for file output (internal, only used by CreateFileStream)
    explicit ThreadStream(std::string_view file_path);

    void InitializeStreamProperties();
    static std::mutex *GetConsoleMutex(const std::ostream &os);
    static std::mutex *GetFileMutex(std::string_view file_path);

    std::ostream *os_;          // Output stream (console or file)
    std::mutex *os_mutex_;      // Pointer to the mutex for this stream
    bool owns_file_;            // Whether this object owns the file stream
    std::ofstream file_stream_; // File stream for file output
};

} // namespace trftp