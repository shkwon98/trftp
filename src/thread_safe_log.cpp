#include "trftp/thread_safe_log.h"

namespace trftp
{

ThreadStream::ThreadStream(std::ostream &os)
    : os_(&os)
    , os_mutex_(GetConsoleMutex(os))
    , owns_file_(false)
{
    InitializeStreamProperties();
}

ThreadStream::ThreadStream(std::string_view file_path)
    : os_(&file_stream_)
    , os_mutex_(GetFileMutex(file_path))
    , owns_file_(true)
{
    file_stream_.open(file_path.data(), std::ios::out | std::ios::app);
    if (!file_stream_.is_open())
    {
        throw std::runtime_error("Failed to open file: " + std::string(file_path));
    }

    InitializeStreamProperties();
}

ThreadStream::~ThreadStream()
{
    std::scoped_lock lock(*os_mutex_);
    if (os_)
    {
        *os_ << this->str();
        if (owns_file_ && file_stream_.is_open())
        {
            file_stream_.close();
        }
    }
}

void ThreadStream::InitializeStreamProperties()
{
    std::ignore = this->imbue(os_->getloc());
    std::ignore = this->precision(os_->precision());
    std::ignore = this->width(os_->width());
    std::ignore = this->setf(std::ios::fixed, std::ios::floatfield);
}

// Get mutex for console streams (cout, cerr)
std::mutex *ThreadStream::GetConsoleMutex(const std::ostream &os)
{
    static std::mutex cout_mutex;
    static std::mutex cerr_mutex;

    if (&os == &std::cout)
    {
        return &cout_mutex;
    }
    if (&os == &std::cerr)
    {
        return &cerr_mutex;
    }

    throw std::runtime_error("Unknown console stream");
}

// Get mutex for file streams
std::mutex *ThreadStream::GetFileMutex(std::string_view file_path)
{
    static std::unordered_map<std::string, std::unique_ptr<std::mutex>> file_mutexes;
    static std::mutex map_mutex;

    // Thread-safe access to the map
    std::scoped_lock lock(map_mutex);

    auto &mutex_ptr = file_mutexes[std::string(file_path)];
    if (!mutex_ptr)
    {
        mutex_ptr = std::make_unique<std::mutex>();
    }
    return mutex_ptr.get();
}

} // namespace trftp