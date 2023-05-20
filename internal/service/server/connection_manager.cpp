//
// Created by brucegoose on 5/19/23.
//

#include "connection_manager.hpp"

namespace service {
    /*
     * Connection registration
     */
    unsigned long ConnectionManager::AddReaderSession(Reader &&session) {
        Reader reader_to_remove = nullptr;
        bool is_first_reader = false;
        const auto reader_addr = session->GetAddr();
        {
            std::unique_lock lk(_reader_mutex);
            is_first_reader = _reader_sessions.empty();
            auto current_session = _reader_sessions.find(reader_addr);
            if (current_session != _reader_sessions.end()) {
                reader_to_remove = std::move(current_session->second);
            }
            _reader_sessions[reader_addr] = std::move(session);
        }

        if (is_first_reader) {
            /* go ahead and attach all writers to the new reader */
            std::shared_lock lk1(_writer_mutex, std::defer_lock);
            std::unique_lock lk2(_connection_mutex, std::defer_lock);
            std::lock(lk1, lk2);
            std::vector<Writer> new_reader_connections = {};
            for (const auto &[writer_addr, writer]: _writer_sessions) {
                _writer_connections[writer_addr] = reader_addr;
                new_reader_connections.push_back(writer);
            }
            _reader_connections[reader_addr] = new_reader_connections;
        } else {
            std::unique_lock lk2(_connection_mutex);
            _reader_connections[reader_addr] = std::vector<Writer>{};
        }

        if (reader_to_remove != nullptr) {
            /*
             * technically we are handling mutexes better and can do this inline the lock;
             *      I don't trust it tho...
             */
            reader_to_remove->TryClose(false);
        }

        return ++_last_session_number;
    }

    void ConnectionManager::RemoveReaderSession(Reader &&session) {
        // I really wish cpp had something like defer to call _reader_sessions.erase(find_reader);
        std::unique_lock lk1(_reader_mutex, std::defer_lock);
        std::unique_lock lk2(_connection_mutex, std::defer_lock);
        std::lock(lk1, lk2);
        const auto dead_addr = session->GetAddr();
        const auto dead_id = session->GetSessionId();
        auto dead_reader = _reader_sessions.find(dead_addr);
        if (dead_reader == _reader_sessions.end() || dead_reader->second->GetSessionId() != dead_id) {
            /*
             * We've already been removed
             */
            return;
        }
        if (_reader_sessions.size() == 1) {
            /*
             * We are the only reader; goahead and remove all connections
             */
            _writer_connections.clear();
            _reader_connections.clear();
            _reader_sessions.clear();
            return;
        }
        auto dead_reader_connections = _reader_connections.find(dead_addr);
        if (dead_reader_connections == _reader_connections.end()) {
            // this should never get here
            _reader_sessions.erase(dead_reader);
            return;
        }
        auto &dead_connections = dead_reader_connections->second;
        if (dead_connections.empty()) {
            /*
             * Nobody is connected to the reader; remove the stubs and return
             */
            _reader_connections.erase(dead_reader_connections);
            _reader_sessions.erase(dead_reader);
            return;
        }
        /*
         * Get the next available reader; move connections to it
         */
        auto replacement_reader = std::next(dead_reader);
        if (replacement_reader == _reader_sessions.end()) {
            replacement_reader = _reader_sessions.begin();
        }
        const auto replacement_addr = replacement_reader->first;
        if (_reader_connections.find(replacement_addr) == _reader_connections.end()) {
            // this should never get here
            _reader_connections.insert({ replacement_addr, std::vector<Writer>{} });
        }
        auto &replacement_connections = _reader_connections[replacement_addr];
        for (auto it = dead_connections.begin(); it != dead_connections.end(); /* increment in loop body */) {
            replacement_connections.push_back(std::move(*it));
            _writer_connections[replacement_connections.back()->GetAddr()] = replacement_addr;
            it = dead_connections.erase(it);
        }
        // finally remove the connection / session stubs
        _reader_connections.erase(dead_reader_connections);
        _reader_sessions.erase(dead_reader);
    }

    unsigned long ConnectionManager::AddWriterSession(Writer &&session) {
        Writer writer_to_remove = nullptr;
        tcp_addr *replace_connection = nullptr;
        const auto writer_addr = session->GetAddr();
        {
            std::unique_lock lk1(_writer_mutex);
            const auto current_session = _writer_sessions.find(writer_addr);
            if (current_session != _writer_sessions.end()) {
                writer_to_remove = std::move(current_session->second);
                // check if it was connected
                std::shared_lock lk2(_connection_mutex);
                const auto current_connection = _writer_connections.find(writer_addr);
                if (current_connection != _writer_connections.end()) {
                    replace_connection = &current_connection->second;
                }
            }
            _writer_sessions[writer_addr] = session;
        }

        if (replace_connection == nullptr) {
            /* this is a new connection; if there is an available reader, attach it */
            std::shared_lock lk1(_reader_mutex, std::defer_lock);
            std::unique_lock lk2(_connection_mutex, std::defer_lock);
            std::lock(lk1, lk2);
            if (_reader_sessions.begin() != _reader_sessions.end()) {
                // found an available reader
                const auto &reader_addr = _reader_sessions.begin()->first;
                auto reader_connection = _reader_connections.find(reader_addr);
                if (reader_connection != _reader_connections.end()) {
                    // reader is accepting connections
                    reader_connection->second.push_back(std::move(session));
                    _writer_connections[writer_addr] = reader_addr;
                }
                // if the above fails, we should throw an error...
            }
        } else {
            /* this is an existing connection; go ahead and swap the current one with it */
            std::unique_lock lk(_connection_mutex);
            auto reader_connection = _reader_connections.find(*replace_connection);
            if (reader_connection != _reader_connections.end()) {
                auto &connections = reader_connection->second;
                auto can_replace = std::find(connections.begin(), connections.end(), writer_to_remove);
                if (can_replace != connections.end()) {
                    can_replace->swap(session);
                    session.reset();
                }
                // if this fails, we should throw an error...
            }
            // if this fails, we should throw an error...
        }

        if (writer_to_remove != nullptr) {
            /*
             * technically we are handling mutexes better and can do this inline the lock;
             *      I don't trust it tho...
             */
            writer_to_remove->TryClose(false);
        }

        return ++_last_session_number;
    }

    void ConnectionManager::RemoveWriterSession(Writer &&session) {
        std::unique_lock lk1(_writer_mutex, std::defer_lock);
        std::unique_lock lk2(_connection_mutex, std::defer_lock);
        std::lock(lk1, lk2);
        const auto dead_addr = session->GetAddr();
        const auto dead_id = session->GetSessionId();
        auto dead_writer = _writer_sessions.find(dead_addr);
        if (dead_writer == _writer_sessions.end() || dead_writer->second->GetSessionId() != dead_id) {
            /*
             * We've already been removed
             */
            return;
        }
        if (_writer_sessions.size() == 1) {
            /*
             * We are the only writer; clean up
             */
            _writer_connections.clear();
            for (auto &[reader_addr, connections]: _reader_connections) {
                connections.clear();
            }
            _writer_sessions.clear();
            return;
        }
        auto dead_writer_connection = _writer_connections.find(dead_addr);
        if (dead_writer_connection == _writer_connections.end()) {
            // this should never get here
            _writer_sessions.erase(dead_writer);
            return;
        }
        auto remove_reader_connection = _reader_connections.find(dead_writer_connection->second);
        if (remove_reader_connection == _reader_connections.end() || remove_reader_connection->second.empty()) {
            // this should never get here
            _writer_connections.erase(dead_writer_connection);
            _writer_sessions.erase(dead_writer);
            return;
        }
        auto &connections = remove_reader_connection->second;
        auto remove = std::find(connections.begin(), connections.end(), session);
        if (remove == connections.end()) {
            // this should never get here
            _writer_connections.erase(dead_writer_connection);
            _writer_sessions.erase(dead_writer);
            return;
        }
        connections.erase(remove);
        _writer_connections.erase(dead_writer_connection);
        _writer_sessions.erase(dead_writer);
    }

    /*
     * Connection
     */
    void ConnectionManager::PostMessage(const tcp_addr &addr, std::shared_ptr<ResizableBuffer> &&buffer) {
        std::shared_lock lk(_connection_mutex);
        auto connections = _reader_connections.find(addr);
        if (connections == _reader_connections.end()) {
            // should never get here
            return;
        }
        for (auto &writer : connections->second) {
            auto b_copy(buffer);
            writer->Write(std::move(b_copy));
        }
        buffer.reset();
    }

    /*
     * Connection Management
     */
    std::pair<int, int> ConnectionManager::GetConnectionCounts() {
        std::shared_lock lk1(_writer_mutex, std::defer_lock);
        std::shared_lock lk2(_reader_mutex, std::defer_lock);
        std::lock(lk1, lk2);
        return { _reader_connections.size(), _writer_connections.size() };
    }

    bool ConnectionManager::RotateWriterConnection(const tcp_addr &addr) {
        std::shared_lock lk1(_reader_mutex, std::defer_lock);
        std::unique_lock lk2(_connection_mutex, std::defer_lock);
        std::lock(lk1, lk2);
        auto writer_connection = _writer_connections.find(addr);
        if (writer_connection == _writer_connections.end()) {
            // couldn't find the writer
            return false;
        }
        const auto &current_reader_addr = writer_connection->second;
        // get next reader if possible
        auto reader = _reader_sessions.find(current_reader_addr);
        if (reader == _reader_sessions.end()) {
            // couldn't find the reader
            return false;
        }
        if (_reader_sessions.size() == 1) {
            // we have the only reader hooked up; nothing to do
            return true;
        }
        auto current_reader_connection = _reader_connections.find(current_reader_addr);
        if (current_reader_connection == _reader_connections.end() || current_reader_connection->second.empty()) {
            // we have no recording of the connection; bail
            return false;
        }
        auto &current_connection = current_reader_connection->second;
        auto next_reader = std::next(reader);
        if (next_reader == _reader_sessions.end()) {
            // we have the last reader; rotate around the pool
            next_reader = _reader_sessions.begin();
        }
        const auto &next_reader_addr = next_reader->first;
        auto next_reader_connection = _reader_connections.find(next_reader_addr);
        if (next_reader_connection == _reader_connections.end()) {
            // nowhere to put the new connection; bail
            return false;
        }
        auto &next_connection = next_reader_connection->second;
        // I really wish I could just compare by addr here
        auto move_it = std::find_if(
            current_connection.begin(), current_connection.end(), [&addr](const Writer &writer) {
                return writer->GetAddr() == addr;
            }
        );
        if (move_it == current_connection.end()) {
            // couldn't find the connection; bail
            return false;
        }
        next_connection.push_back(std::move(*move_it));
        current_connection.erase(move_it);
        writer_connection->second = next_reader_addr;
        return true;
    }

    bool ConnectionManager::PointReaderAtWriters(const tcp_addr &addr) {
        std::shared_lock lk1(_reader_mutex, std::defer_lock);
        std::unique_lock lk2(_connection_mutex, std::defer_lock);
        std::lock(lk1, lk2);
        if (_reader_sessions.find(addr) == _reader_sessions.end()) {
            return false;
        }
        auto reader_connection = _reader_connections.find(addr);
        if (reader_connection == _reader_connections.end()) {
            // shouldn't get here
            return false;
        }
        auto &move_to_connection = reader_connection->second;

        for (auto &writer_connection : _writer_connections) {
            writer_connection.second = addr;
        }

        for (auto &[r_addr, connections]: _reader_connections) {
            if (r_addr == addr || connections.empty()) {
                continue;
            }
            std::move(connections.begin(), connections.end(), std::back_inserter(move_to_connection));
            connections.erase(connections.begin(), connections.end());
        }

        return true;
    }

    void ConnectionManager::Clear() {
        std::vector<Reader> removed_readers;
        std::vector<Writer> removed_writers;
        {
            std::unique_lock lk1(_reader_mutex, std::defer_lock);
            std::unique_lock lk2(_writer_mutex, std::defer_lock);
            std::unique_lock lk3(_connection_mutex, std::defer_lock);
            _reader_connections.clear();
            _writer_connections.clear();
            removed_readers.reserve(_reader_sessions.size());
            for (auto &[_, reader]: _reader_sessions) {
                removed_readers.push_back(std::move(reader));
            }
            _reader_sessions.clear();
            removed_writers.reserve(_writer_sessions.size());
            for (auto &[_, writer]: _writer_sessions) {
                removed_writers.push_back(std::move(writer));
            }
            _writer_sessions.clear();
        }
        for (auto &reader: removed_readers) {
            reader->TryClose(false);
        }
        for (auto &writer: removed_writers) {
            writer->TryClose(false);
        }
    }


}