#include "trftp/client/client_transaction.h"
#include "trftp/client/client.h"
#include "trftp/client/client_log.h"

namespace trftp
{

ClientTransaction::ClientTransaction(Client *client, std::uint32_t file_version)
    : client_{ client }
    , cur_file_version_{ file_version }
    , inter_packet_gap_{ std::chrono::microseconds(100) }
    , is_active_{ false }
    , status_{ FtpStatus::FIN }
    , server_address_{}
    , udp_socket_{}
    , total_packet_number_{ 0 }
    , packet_sequence_number_{ 0 }
    , new_file_stream_{}
    , new_file_path_{ std::filesystem::temp_directory_path() / "trftp_temp_file" }
    , new_file_version_{ 0 }
    , new_file_size_{ 0 }
    , new_file_crc32_{ 0 }
{
    udp_socket_.SetReadTimeout(3s);
}

ClientTransaction::~ClientTransaction()
{
    Reset();
}

bool ClientTransaction::IsAlive() const
{
    return is_active_;
}

void ClientTransaction::Begin(const TrftpMessage &msg, std::size_t len, const sockaddr_in &addr)
{
    const auto &id = MessageId(msg.header.xid);
    id == MessageId::NTF ? is_active_ = true : is_active_ = false;

    if (is_active_)
    {
        Reset();
        OnReceive(msg, len, addr);
    }
}

void ClientTransaction::Reset()
{
    server_address_ = {};
    total_packet_number_ = 0;
    packet_sequence_number_ = 0;
    new_file_stream_.close();
    new_file_path_ = std::filesystem::temp_directory_path() / "trftp_temp_file";
    new_file_version_ = 0;
    new_file_size_ = 0;
    new_file_crc32_ = 0;

    if (thread_.joinable())
    {
        thread_.join();
    }
}

void ClientTransaction::HandleIncomingMessages()
{
    while (is_active_)
    {
        TrftpMessage msg;
        sockaddr_in server_addr;

        auto len = udp_socket_.Receive(msg, server_addr);
        if (len == 0)
        {
            terr << ClientLog() << "Timeout occurred. Cancelling..." << std::endl;
            SendMessage(MessageId::CXL);
            break;
        }

        OnReceive(msg, len, server_addr);
    }
}

void ClientTransaction::OnReceive(const TrftpMessage &msg, std::size_t len, const sockaddr_in &addr)
{
    server_address_ = addr;
    PrintRecvLog(msg);

    if (!ValidateMessageIntegrity(msg, len))
    {
        terr << ClientLog() << "Message integrity check failed. Discarding..." << std::endl;
        return;
    }

    const auto &id = MessageId(msg.header.xid);
    const auto &payload_len = len - sizeof(TrftpHeader);

    switch (id)
    {
    case MessageId::NTF:
        if (payload_len != sizeof(TrftpNtf))
        {
            terr << ClientLog() << "Invalid message length for <CHK>. Discarding..." << std::endl;
            SendMessage(MessageId::CXL);
            break;
        }

        status_ = id;
        new_file_version_ = msg.ntf.new_file_version;

        if (thread_.joinable())
        {
            thread_.join();
        }
        thread_ = std::thread(&ClientTransaction::HandleIncomingMessages, this);
        SendMessage(MessageId::CHK);
        break;

    case MessageId::INFO:
        if (status_ != FtpStatus::CHK)
        {
            terr << ClientLog() << "Transaction state is not <CHK>. Discarding..." << std::endl;
            SendMessage(MessageId::CXL);
            break;
        }
        if (payload_len != sizeof(TrftpInfo))
        {
            terr << ClientLog() << "Invalid message length for <INFO>. Discarding..." << std::endl;
            SendMessage(MessageId::CXL);
            break;
        }

        if (new_file_version_ != msg.info.new_file_version)
        {
            terr << ClientLog() << "File version mismatch. Cancelling..." << std::endl;
            SendMessage(MessageId::CXL);
            break;
        }
        if (msg.info.file_length == 0)
        {
            terr << ClientLog() << "File size is zero. Cancelling..." << std::endl;
            SendMessage(MessageId::CXL);
            break;
        }

        status_ = id;
        new_file_size_ = msg.info.file_length;
        new_file_crc32_ = msg.info.crc32;
        total_packet_number_ = (new_file_size_ + sizeof(TrftpMessage::payload) - 1) / sizeof(TrftpMessage::payload);

        new_file_stream_.open(new_file_path_, std::ios::binary | std::ios::out);
        if (!new_file_stream_.is_open())
        {
            terr << ClientLog() << "Failed to open the file for writing. Cancelling..." << std::endl;
            SendMessage(MessageId::CXL);
            break;
        }

        SendMessage(MessageId::RDY);
        break;

    case MessageId::DATA:
        if (msg.header.psn == 0) // 첫번째 DATA 패킷
        {
            if (status_ != FtpStatus::RDY)
            {
                terr << ClientLog() << "Transaction state is not <INFO>. Discarding..." << std::endl;
                SendMessage(MessageId::CXL);
                break;
            }
            if (payload_len != sizeof(TrftpData))
            {
                terr << ClientLog() << "Invalid message length for <DATA>. Discarding..." << std::endl;
                SendMessage(MessageId::CXL);
                break;
            }
        }
        else if (msg.header.psn < (msg.header.tpn - 1)) // 중간 DATA 패킷
        {
            if (status_ != FtpStatus::DATA)
            {
                terr << ClientLog() << "Transaction state is not <DATA>. Discarding..." << std::endl;
                SendMessage(MessageId::CXL);
                break;
            }
            if (payload_len != sizeof(TrftpData))
            {
                // print if specific environment variable is set
                terr << ClientLog() << "Invalid message length for <DATA>. Discarding..." << std::endl;
                SendMessage(MessageId::CXL);
                break;
            }
        }
        else // 마지막 DATA 패킷
        {
            if (status_ != FtpStatus::DATA)
            {
                terr << ClientLog() << "Transaction state is not <DATA>. Discarding..." << std::endl;
                SendMessage(MessageId::CXL);
                break;
            }
            if (payload_len != (new_file_size_ % sizeof(TrftpMessage::payload)))
            {
                terr << ClientLog() << "Invalid message length for <DATA>. Discarding..." << std::endl;
                SendMessage(MessageId::CXL);
                break;
            }
        }

        if (msg.header.psn != packet_sequence_number_)
        {
            terr << ClientLog() << "PSN mismatch. Retransmitting..." << std::endl;
            SendMessage(MessageId::RTX);
            return;
        }

        new_file_stream_.write(msg.data.new_file_data, payload_len);
        if (new_file_stream_.fail())
        {
            terr << ClientLog() << "Failed to write to the file. Cancelling..." << std::endl;
            SendMessage(MessageId::CXL);
            break;
        }

        status_ = id;
        packet_sequence_number_++;

        if (packet_sequence_number_ == total_packet_number_) // 마지막 패킷까지 수신 완료
        {
            new_file_stream_.close();

            if (new_file_size_ != std::filesystem::file_size(new_file_path_))
            {
                terr << ClientLog() << "File size mismatch. Cancelling..." << std::endl;
                SendMessage(MessageId::CXL);
                break;
            }
            if (new_file_crc32_ != CalculateFileCrc32(new_file_path_))
            {
                terr << ClientLog() << "CRC32 mismatch. Cancelling..." << std::endl;
                SendMessage(MessageId::CXL);
                break;
            }

            SendMessage(MessageId::DONE);
        }

        break;

    case MessageId::FIN:
        if (status_ != FtpStatus::DONE)
        {
            terr << ClientLog() << "Transaction state is not <DONE>. Discarding..." << std::endl;
            SendMessage(MessageId::CXL);
            break;
        }
        if (payload_len != 0)
        {
            terr << ClientLog() << "Invalid message length for <FIN>. Discarding..." << std::endl;
            SendMessage(MessageId::CXL);
            break;
        }

        status_ = id;
        is_active_ = false;
        if (client_)
        {
            client_->OnFileReceived(new_file_path_, new_file_version_);
        }
        break;

    case MessageId::CXL:
        if (new_file_stream_.is_open())
        {
            new_file_stream_.close();
            std::filesystem::remove(new_file_path_);
        }

        status_ = id;
        SendMessage(MessageId::CXL);
        break;

    default:
        break;
    }
}

void ClientTransaction::SendMessage(MessageId id)
{
    if ((id != MessageId::CHK) && (id != MessageId::RDY) && (id != MessageId::RTX) && (id != MessageId::DONE) &&
        (id != MessageId::CXL))
    {
        terr << ClientLog() << "Unknown XID (" << static_cast<std::uint32_t>(id) << "). Discarding..." << std::endl;
        return;
    }

    auto payload_len = 0U;
    TrftpMessage msg;

    switch (id)
    {
    case MessageId::CHK:
        status_ = id;
        payload_len += sizeof(TrftpChk);
        msg.chk.cur_file_version = cur_file_version_;
        break;

    case MessageId::RDY:
        status_ = id;
        payload_len += sizeof(TrftpRdy);
        msg.rdy.new_file_version = new_file_version_;
        msg.rdy.file_length = new_file_size_;
        msg.rdy.inter_packet_gap = inter_packet_gap_.count();
        break;

    case MessageId::DONE:
        status_ = id;
        payload_len += sizeof(TrftpDone);
        msg.done.new_file_version = new_file_version_;
        msg.done.file_length = new_file_size_;
        msg.done.crc32 = new_file_crc32_;
        break;

    case MessageId::CXL:
        status_ = id;
        is_active_ = false;
        break;

    case MessageId::RTX:
        payload_len += sizeof(TrftpRtx);
        msg.rtx.retransmit_psn = packet_sequence_number_;
        break;

    default:
        return;
    }

    // Set up the message header
    msg.header.magic = TRFTP_MAGIC;
    msg.header.spid = 0x0000U;
    msg.header.dpid = 0xFD00U;
    msg.header.tpn = 1U;
    msg.header.tpl = payload_len;
    msg.header.xid = std::uint32_t(id);
    msg.header.psn = 0U;
    msg.header.pl = payload_len;
    msg.header.crc32 = 0U;
    msg.header.crc32 = CalculateCrc32(reinterpret_cast<std::uint8_t *>(&msg), sizeof(TrftpHeader) + payload_len, 0U);

    // Send the message
    if (udp_socket_.Send(msg, sizeof(TrftpHeader) + payload_len, server_address_))
    {
        PrintSendLog(msg);
    }
}

bool ClientTransaction::ValidateMessageIntegrity(const TrftpMessage &msg, std::size_t len) const
{
    // 1. check the CRC32
    auto crc32_saved = std::exchange(const_cast<TrftpMessage &>(msg).header.crc32, 0); // Set to 0 and save original crc32
    auto crc32_calculated = CalculateCrc32(reinterpret_cast<const std::uint8_t *>(&msg), len, 0);
    std::exchange(const_cast<TrftpMessage &>(msg).header.crc32, crc32_saved); // Restore original crc32

    if (crc32_calculated != crc32_saved)
    {
        terr << ClientLog() << "CRC32 mismatch. Discarding..." << std::endl;
        return false;
    }

    // 2. check the magic code
    if (msg.header.magic != TRFTP_MAGIC)
    {
        terr << ClientLog() << "Invalid magic code. Discarding..." << std::endl;
        return false;
    }

    // 3. check length
    if (len != sizeof(TrftpHeader) + msg.header.pl)
    {
        terr << ClientLog() << "Invalid message length. Discarding..." << std::endl;
        return false;
    }

    // 4. check TPN, PSN
    if (msg.header.tpn <= msg.header.psn)
    {
        terr << ClientLog() << "Invalid TPN, PSN(" << msg.header.tpn << ", " << msg.header.psn << "). Discarding..."
             << std::endl;
        return false;
    }

    return true;
}

void ClientTransaction::PrintRecvLog(const TrftpMessage &msg) const
{
    // TODO: DUMP message

    std::string id_str = "UNKNOWN";

    switch (MessageId(msg.header.xid))
    {
    case MessageId::NTF:
        id_str = "NTF";
        break;
    case MessageId::INFO:
        id_str = "INFO";
        break;
    case MessageId::DATA:
        id_str = "DATA";
        break;
    case MessageId::FIN:
        id_str = "FIN";
        break;
    case MessageId::CXL:
        id_str = "CXL";
        break;
    default:
        break;
    }

    tout << ClientLog() << "recv " << YELLOW << id_str << RESET << " from <" << inet_ntoa(server_address_.sin_addr) << ":"
         << be16toh(server_address_.sin_port) << "> (xid=" << std::hex << msg.header.xid << std::dec
         << ", tpn=" << msg.header.tpn << ", psn=" << msg.header.psn << ", tpl=" << msg.header.tpl
         << ", pl=" << msg.header.pl << ")" << std::endl;
};

void ClientTransaction::PrintSendLog(const TrftpMessage &msg) const
{
    // TODO: DUMP message

    std::string id_str = "UNKNOWN";

    switch (MessageId(msg.header.xid))
    {
    case MessageId::CHK:
        id_str = "CHK";
        break;
    case MessageId::RDY:
        id_str = "RDY";
        break;
    case MessageId::RTX:
        id_str = "RTX";
        break;
    case MessageId::DONE:
        id_str = "DONE";
        break;
    case MessageId::CXL:
        id_str = "CXL";
        break;
    default:
        break;
    }

    tout << ClientLog() << "send " << YELLOW << id_str << RESET << " to <" << inet_ntoa(server_address_.sin_addr) << ":"
         << be16toh(server_address_.sin_port) << "> (xid=" << std::hex << msg.header.xid << std::dec
         << ", tpn=" << msg.header.tpn << ", psn=" << msg.header.psn << ", tpl=" << msg.header.tpl
         << ", pl=" << msg.header.pl << ")" << std::endl;
}

} // namespace trftp