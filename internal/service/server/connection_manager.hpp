//
// Created by brucegoose on 5/18/23.
//

#ifndef AUGMENTEDNORMALCY_SERVICE_SERVER_CONNECTION_MANAGER_HPP
#define AUGMENTEDNORMALCY_SERVICE_SERVER_CONNECTION_MANAGER_HPP

#include <vector>
#include <map>
#include <shared_mutex>

#include <boost/asio/ip/tcp.hpp>

using boost::asio::ip::tcp;
typedef boost::asio::ip::address_v4 tcp_addr;

#include "infrastructure/tcp/tcp_server.hpp"

namespace service {

    typedef std::shared_ptr<infrastructure::TcpSession> Reader;
    typedef std::shared_ptr<infrastructure::WritableTcpSession> Writer;

    class ConnectionManager {
    public:
        // connection registration
        [[nodiscard]] unsigned long AddReaderSession(Reader &&session);
        void RemoveReaderSession(Reader &&session);
        [[nodiscard]] unsigned long AddWriterSession(Writer &&session);
        void RemoveWriterSession(Writer &&session);
        // connection
        void PostMessage(const tcp_addr &addr, std::shared_ptr<ResizableBuffer> &&buffer);
        // connection management
        [[nodiscard]] std::pair<int, int> GetConnectionCounts();
        bool RotateWriterConnection(const tcp_addr &writer_addr);
        [[nodiscard]] bool PointReaderAtWriters(const tcp_addr &reader_addr);
        void Clear();
    private:
        std::atomic<unsigned long> _last_session_number = { 0 };
        std::map<tcp_addr, Reader> _reader_sessions;
        mutable std::shared_mutex _reader_mutex;
        std::map<tcp_addr, Writer> _writer_sessions;
        mutable std::shared_mutex _writer_mutex;
        std::map<tcp_addr, std::vector<Writer>> _reader_connections;
        std::map<tcp_addr, tcp_addr> _writer_connections;
        mutable std::shared_mutex _connection_mutex;
    };

}

#endif //AUGMENTEDNORMALCY_SERVICE_SERVER_CONNECTION_MANAGER_HPP
