#include "trftp/server/server_transaction.h"
#include "trftp/server/server_log.h"

namespace trftp
{
ServerTransaction::ServerTransaction(const sockaddr_in &addr, const std::filesystem::path &file_path,
                                     std::uint32_t file_version, const std::uint32_t device_id)
    : file_path_{ file_path }
    , new_file_version_{ file_version }
    , new_file_size_{ static_cast<std::uint32_t>(std::filesystem::file_size(file_path)) }
    , new_file_crc32_{ CalculateFileCrc32(file_path) }
    , status_{ FtpStatus::NTF }
    , device_id_{ device_id }
    , client_address_{ addr }
    , total_packet_number_{ (new_file_size_ + static_cast<std::uint32_t>(sizeof(TrftpMessage::payload)) - 1) /
                            static_cast<std::uint32_t>(sizeof(TrftpMessage::payload)) }
    , packet_sequence_number_{ 0 }
    , retransmit_psn_{ std::numeric_limits<std::uint32_t>::max() }
    , cur_file_version_{ 0 }
    , inter_packet_gap_{ std::chrono::microseconds(100) }
{
}

ServerTransaction::ServerTransaction(ServerTransaction &&other) noexcept
    : file_path_{ std::move(other.file_path_) }
    , new_file_version_{ other.new_file_version_ }
    , new_file_size_{ other.new_file_size_ }
    , new_file_crc32_{ other.new_file_crc32_ }
    , status_{ other.status_.load() }
    , device_id_{ other.device_id_ }
    , client_address_{ other.client_address_ }
    , total_packet_number_{ other.total_packet_number_ }
    , packet_sequence_number_{ other.packet_sequence_number_.load() }
    , retransmit_psn_{ other.retransmit_psn_.load() }
    , cur_file_version_{ other.cur_file_version_ }
    , inter_packet_gap_{ other.inter_packet_gap_ }
{
}

ServerTransaction::~ServerTransaction()
{
    if (thr_.joinable())
    {
        thr_.join();
    }
}

void ServerTransaction::SendMessage(MessageId id, UdpSocket &udp_socket)
{
    if ((id != MessageId::NTF) && (id != MessageId::INFO) && (id != MessageId::DATA) && (id != MessageId::FIN) &&
        (id != MessageId::CXL))
    {
        terr << ServerLog() << "Unknown XID (" << static_cast<std::uint32_t>(id) << "). Discarding..." << std::endl;
        return;
    }

    status_ = id;
    cv_.notify_one();

    auto payload_len = 0U;
    TrftpMessage msg;

    switch (id)
    {
    case MessageId::NTF:
        payload_len += sizeof(TrftpNtf);
        msg.ntf.new_file_version = new_file_version_;
        break;

    case MessageId::INFO:
        payload_len += sizeof(TrftpInfo);
        msg.info.new_file_version = new_file_version_;
        msg.info.file_length = new_file_size_;
        msg.info.crc32 = new_file_crc32_;
        break;

    case MessageId::FIN:
    case MessageId::CXL:
        break;

    case MessageId::DATA:
        SendFileAsync(udp_socket);
        return;

    default:
        return;
    }

    // Set up the message header
    CompleteHeader(msg, id, payload_len, 0U);

    // Send the message
    if (udp_socket.Send(msg, sizeof(TrftpHeader) + payload_len, client_address_))
    {
        PrintSendLog(msg);
    }
}

void ServerTransaction::OnReceive(const TrftpMessage &msg, std::size_t len, const sockaddr_in &addr)
{
    client_address_ = addr;
    PrintRecvLog(msg);

    if (!ValidateMessageIntegrity(msg, len))
    {
        terr << ServerLog() << "Message integrity check failed. Discarding..." << std::endl;
        return;
    }

    const auto &id = MessageId(msg.header.xid);
    const auto &payload_len = len - sizeof(TrftpHeader);

    switch (id)
    {
    case MessageId::CHK:
        if (status_ != FtpStatus::NTF)
        {
            terr << ServerLog() << "Transaction state is not <NTF>. Discarding..." << std::endl;
            return;
        }
        if (!ValidateMessage(msg.chk, payload_len))
        {
            return;
        }

        cur_file_version_ = msg.chk.cur_file_version;
        break;

    case MessageId::RDY:
        if (status_ != FtpStatus::INFO)
        {
            terr << ServerLog() << "Transaction state is not <INFO>. Discarding..." << std::endl;
            return;
        }
        if (!ValidateMessage(msg.rdy, payload_len))
        {
            return;
        }

        inter_packet_gap_ = std::chrono::microseconds(std::clamp(msg.rdy.inter_packet_gap, TRAN_IPG_MIN, TRAN_IPG_MAX));
        break;

    case MessageId::DONE:
        if (status_ != FtpStatus::DATA)
        {
            terr << ServerLog() << "Transaction state is not <DATA>. Discarding..." << std::endl;
            return;
        }
        if (packet_sequence_number_ != total_packet_number_)
        {
            terr << ServerLog() << "Transaction PSN (" << packet_sequence_number_ << ") does not match expected TPN ("
                 << total_packet_number_ << "). Discarding..." << std::endl;
            return;
        }
        if (!ValidateMessage(msg.done, payload_len, TrftpDone{ new_file_version_, new_file_size_, new_file_crc32_ }))
        {
            return;
        }
        break;

    case MessageId::CXL:
        if (!ValidateMessage(msg.cxl, payload_len))
        {
            return;
        }
        break;

    case MessageId::RTX:
        if (status_ != FtpStatus::DATA)
        {
            terr << ServerLog() << "Transaction state is not <DATA>. Discarding..." << std::endl;
            return;
        }
        if (!ValidateMessage(msg.rtx, payload_len, TrftpRtx{ packet_sequence_number_ }))
        {
            return;
        }

        retransmit_psn_ = msg.rtx.retransmit_psn;
        return;

    default:
        terr << ServerLog() << "Unknown XID (" << msg.header.xid << "). Discarding..." << std::endl;
        return;
    }

    status_ = id;
    cv_.notify_one();
}

std::optional<FtpStatus> ServerTransaction::WaitForStatus(std::chrono::seconds timeout)
{
    auto old_status = status_.load();

    if (std::unique_lock lock(mtx_); cv_.wait_for(lock, timeout, [this, old_status]() { return status_ != old_status; }))
    {
        return status_;
    }

    return std::nullopt;
}

void ServerTransaction::SendFileAsync(UdpSocket &udp_socket)
{
    thr_ = std::thread([this, &udp_socket]() {
        std::ifstream ifs(file_path_, std::ios::binary);

        if (!ifs.is_open())
        {
            SendMessage(MessageId::CXL, udp_socket);
            return;
        }

        for (packet_sequence_number_ = 0; packet_sequence_number_ < total_packet_number_; packet_sequence_number_++)
        {
            auto now = std::chrono::steady_clock::now();

            // 1. Check for CXL (Cancellation Request)
            if (status_ == FtpStatus::CXL)
            {
                ifs.close();
                return;
            }

            // 2. Check for RTX (Retransmission Request)
            if (retransmit_psn_ != -1U)
            {
                packet_sequence_number_.store(retransmit_psn_.exchange(-1));
            }

            // 3. Prepare DATA message
            std::uint32_t file_offset = packet_sequence_number_ * sizeof(TrftpData);
            std::uint32_t payload_len = sizeof(TrftpData);
            if (packet_sequence_number_ == total_packet_number_ - 1)
            {
                payload_len = new_file_size_ - file_offset;
            }

            TrftpMessage msg;
            if (!ifs.seekg(file_offset).read(msg.data.new_file_data, payload_len))
            {
                SendMessage(MessageId::CXL, udp_socket);
                ifs.close();
                return;
            }

            // 3. Send DATA message
            CompleteHeader(msg, MessageId::DATA, new_file_size_, packet_sequence_number_);

            // Send the message
            if (udp_socket.Send(msg, sizeof(TrftpHeader) + payload_len, client_address_))
            {
                PrintSendLog(msg);
            }

            std::this_thread::sleep_until(now + std::chrono::microseconds(inter_packet_gap_));
        }

        ifs.close();
    });
}

void ServerTransaction::CompleteHeader(TrftpMessage &msg, const MessageId xid, const std::uint32_t tpl,
                                       const std::uint32_t psn) const
{
    msg.header.magic = TRFTP_MAGIC;
    msg.header.spid = 0xFD00U;
    msg.header.dpid = device_id_;
    msg.header.xid = std::uint32_t(xid);
    msg.header.tpn = (tpl == 0) ? 1U : (tpl + sizeof(TrftpMessage::payload) - 1) / sizeof(TrftpMessage::payload);
    msg.header.tpl = tpl;
    msg.header.psn = psn;
    msg.header.pl = psn == (msg.header.tpn - 1) ? (tpl % sizeof(TrftpMessage::payload)) : sizeof(TrftpMessage::payload);
    msg.header.crc32 = 0U;
    msg.header.crc32 = CalculateCrc32(reinterpret_cast<std::uint8_t *>(&msg), sizeof(TrftpHeader) + msg.header.pl, 0U);
}

bool ServerTransaction::ValidateMessageIntegrity(const TrftpMessage &msg, std::size_t len) const
{
    // 1. check the CRC32
    auto crc32_saved = std::exchange(const_cast<TrftpMessage &>(msg).header.crc32, 0); // Set to 0 and save original crc32
    auto crc32_calculated = CalculateCrc32(reinterpret_cast<const std::uint8_t *>(&msg), len, 0);
    std::exchange(const_cast<TrftpMessage &>(msg).header.crc32, crc32_saved); // Restore original crc32

    if (crc32_calculated != crc32_saved)
    {
        terr << ServerLog() << "CRC32 mismatch. Discarding..." << std::endl;
        return false;
    }

    // 2. check the magic code
    if (msg.header.magic != TRFTP_MAGIC)
    {
        terr << ServerLog() << "Invalid magic code. Discarding..." << std::endl;
        return false;
    }

    // 3. check length
    if (len != sizeof(TrftpHeader) + msg.header.pl)
    {
        terr << ServerLog() << "Invalid message length. Discarding..." << std::endl;
        return false;
    }

    // 4. check TPN, PSN
    if (msg.header.tpn <= msg.header.psn)
    {
        terr << ServerLog() << "Invalid TPN, PSN(" << msg.header.tpn << ", " << msg.header.psn << "). Discarding..."
             << std::endl;
        return false;
    }

    return true;
}

bool ServerTransaction::ValidateMessage(const TrftpChk &payload, std::size_t payload_len) const
{
    if (payload_len != sizeof(payload))
    {
        terr << ServerLog() << "Invalid message length for <CHK>. Discarding..." << std::endl;
        return false;
    }

    return true;
}
bool ServerTransaction::ValidateMessage(const TrftpRdy &payload, std::size_t payload_len) const
{
    if (payload_len != sizeof(payload))
    {
        terr << ServerLog() << "Invalid message length for <RDY>. Discarding..." << std::endl;
        return false;
    }

    return true;
}
bool ServerTransaction::ValidateMessage(const TrftpDone &payload, std::size_t payload_len, const TrftpDone &expected) const
{
    if (payload_len != sizeof(payload))
    {
        terr << ServerLog() << "Invalid message length for <DONE>. Discarding..." << std::endl;
        return false;
    }
    if (payload.new_file_version != expected.new_file_version)
    {
        terr << ServerLog() << "Transaction file version (" << expected.new_file_version
             << ") does not match expected version (" << payload.new_file_version << "). Discarding..." << std::endl;
        return false;
    }
    if (payload.file_length != expected.file_length)
    {
        terr << ServerLog() << "Transaction file length (" << expected.file_length << ") does not match expected length ("
             << payload.file_length << "). Discarding..." << std::endl;
        return false;
    }
    if (payload.crc32 != expected.crc32)
    {
        terr << ServerLog() << "Transaction file CRC32 (" << expected.crc32 << ") does not match expected CRC32 ("
             << payload.crc32 << "). Discarding..." << std::endl;
        return false;
    }

    return true;
}
bool ServerTransaction::ValidateMessage(const TrftpCxl &payload, std::size_t payload_len) const
{
    if (payload_len != sizeof(payload) - 1)
    {
        terr << ServerLog() << "Invalid message length for <CXL>. Discarding..." << std::endl;
        return false;
    }

    return true;
}
bool ServerTransaction::ValidateMessage(const TrftpRtx &payload, std::size_t payload_len, const TrftpRtx &expected) const
{
    if (payload_len != sizeof(payload))
    {
        terr << ServerLog() << "Invalid message length for <RTX>. Discarding..." << std::endl;
        return false;
    }
    if (payload.retransmit_psn > expected.retransmit_psn)
    {
        terr << ServerLog() << "Retransmit PSN (" << expected.retransmit_psn << ") is greater than current PSN ("
             << payload.retransmit_psn << "). Discarding..." << std::endl;
        return false;
    }

    return true;
}

void ServerTransaction::PrintRecvLog(const TrftpMessage &msg) const
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

    tout << ServerLog() << "recv " << YELLOW << id_str << RESET << " from <" << inet_ntoa(client_address_.sin_addr) << ":"
         << be16toh(client_address_.sin_port) << "> (xid=" << std::hex << msg.header.xid << std::dec
         << ", tpn=" << msg.header.tpn << ", psn=" << msg.header.psn << ", tpl=" << msg.header.tpl
         << ", pl=" << msg.header.pl << ")" << std::endl;
};

void ServerTransaction::PrintSendLog(const TrftpMessage &msg) const
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

    tout << ServerLog() << "send " << YELLOW << id_str << RESET << " to <" << inet_ntoa(client_address_.sin_addr) << ":"
         << be16toh(client_address_.sin_port) << "> (xid=" << std::hex << msg.header.xid << std::dec
         << ", tpn=" << msg.header.tpn << ", psn=" << msg.header.psn << ", tpl=" << msg.header.tpl
         << ", pl=" << msg.header.pl << ")" << std::endl;
}

} // namespace trftp