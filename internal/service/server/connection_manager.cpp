//
// Created by brucegoose on 5/19/23.
//

#include "connection_manager.hpp"

namespace service {
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
            _reader_sessions.erase(dead_reader);
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

    }

}