//
// Created by brucegoose on 5/29/23.
//

#include "utils/clock.hpp"

#include "websocket_client.hpp"

namespace infrastructure {
    WebsocketClientPtr WebsocketClient::Create(
        const WebsocketClientConfig &config, net::io_context &context, WebsocketClientManagerPtr manager
    ){
        return std::make_shared<WebsocketClient>(config, context, std::move(manager));
    }

    WebsocketClient::WebsocketClient(
        const WebsocketClientConfig &config, net::io_context &context, WebsocketClientManagerPtr &&manager
    ):
        _remote_endpoint(
            net::ip::address::from_string(config.get_websocket_server_host()),
            config.get_websocket_server_port()
        ),
        _host(
            config.get_websocket_server_host() + ":" +
            std::to_string(config.get_websocket_server_port())
        ),
        _manager(std::move(manager)),
        _op_timeout(config.get_websocket_client_op_timeout()),
        _connection_timeout(config.get_websocket_client_connection_timeout()),
        _strand(net::make_strand(context)),
        _use_fixed_port(config.get_websocket_client_used_fixed_port()),
        _is_camera(config.get_websocket_client_is_camera())
    {}

    void WebsocketClient::Start() {
        if (!_is_stopped) return;

        _is_stopped = false;
        auto session_number = _session_number.load();
        startConnection(session_number, true);
    }

    void WebsocketClient::Stop() {
        if (_is_stopped) return;

        _is_stopped = true;
        _session_number++;

        std::promise<void> done_promise;
        auto done_future = done_promise.get_future();
        net::post(
            _strand,
            [this, self = shared_from_this(), p = std::move(done_promise)]() mutable {
                if (_ws) {
                    beast::get_lowest_layer(*_ws).socket().shutdown(tcp::socket::shutdown_both);
                    _ws.reset();
                    _ws = nullptr;
                }
                _manager->DestroyWebsocketClientConnection();
                p.set_value();
            }
        );
        done_future.wait();
    }

    void WebsocketClient::startConnection(const unsigned long session_number, bool is_initial_connection) {
        if (_is_stopped || session_number != _session_number) return;
        if (!is_initial_connection) {
            std::this_thread::sleep_for(1s);
        }
        if (_use_fixed_port) {
            auto endpoint = tcp::endpoint(tcp::v4(), _is_camera ? 33333 : 44444);
            _ws = std::make_shared<websocket::stream<beast::tcp_stream>>(_strand, endpoint);
        } else {
            _ws = std::make_shared<websocket::stream<beast::tcp_stream>>(_strand);
        }
        beast::get_lowest_layer(*_ws).expires_after(std::chrono::seconds(_connection_timeout));
        beast::get_lowest_layer(*_ws).async_connect(
            _remote_endpoint, beast::bind_front_handler(
                &WebsocketClient::onConnection, shared_from_this(), session_number
            )
        );
    }

    void WebsocketClient::onConnection(const unsigned long session_number, beast::error_code ec) {
        if (_is_stopped || session_number != _session_number) return;
        if (ec) {
            std::cout << "WebsocketClient::onConnection received err " << ec << " while connecting" <<std::endl;
            startConnection(session_number, false);
            return;
        }

        beast::get_lowest_layer(*_ws).expires_never();

        _ws->set_option(
            websocket::stream_base::timeout{
                .handshake_timeout = std::chrono::seconds(_connection_timeout),
                .idle_timeout = std::chrono::seconds(_op_timeout),
                .keep_alive_pings = true
            }
        );

        _ws->set_option(websocket::stream_base::decorator(
            [](websocket::request_type& req) {
                req.set(
                    http::field::user_agent,
                    std::string(BOOST_BEAST_VERSION_STRING) +
                    " websocket-client-async"
                );
            }
        ));

        _ws->async_handshake(
            _host, "/", beast::bind_front_handler(
                &WebsocketClient::onHandshake, shared_from_this(), session_number
            )
        );

    }

    void WebsocketClient::onHandshake(const unsigned long session_number, beast::error_code ec) {
        if (_is_stopped || session_number != _session_number) return;
        if (ec) {
            std::cout << "WebsocketClient::onHandshake received err " << ec << " while connecting" <<std::endl;
            disconnect(session_number);
            return;
        }

        _manager->CreateWebsocketClientConnection();
        read(session_number);
        if (!_send_queue.empty()) {
            doWrite(session_number);
        }
    }

    void WebsocketClient::read(const unsigned long session_number) {
        if (_is_stopped || session_number != _session_number) return;
        _ws->async_read(
            _read_buffer,
            beast::bind_front_handler(
                &WebsocketClient::onRead, shared_from_this(), session_number
            )
        );
    }

    void WebsocketClient::onRead(const unsigned long session_number, beast::error_code ec, std::size_t bytes_transferred) {
        if (_is_stopped || session_number != _session_number) return;
        if (ec) {
            std::cout << "WebsocketClient::onRead received err " << ec << " while reading" <<std::endl;
            disconnect(session_number);
            return;
        } else if (bytes_transferred == 0) {
            // probably a ping
            read(session_number);
            return;
        }

        auto buffers = _read_buffer.data();
        std::string data = boost::beast::buffers_to_string(buffers);
        try {
            auto js = nlohmann::json::parse(data);
            const bool success = _manager->PostWebsocketServerMessage(std::move(js));
            if (success) {
                _read_buffer.consume(bytes_transferred);
                read(session_number);
            } else {
                std::cout << "WebsocketSession::onRead failed to send valid json; bailing" << std::endl;
                disconnect(session_number);
            }
        } catch (nlohmann::json::parse_error& e) {
            std::cout << "WebsocketSession::onRead failed to parse json; bailing" << std::endl;
            disconnect(session_number);
        }
    }

    void WebsocketClient::PostWebsocketClientMessage(nlohmann::json &&message) {
        if (_is_stopped) return;
        auto str_message = message.dump();
        net::post(
            _strand,
            beast::bind_front_handler(&WebsocketClient::write, shared_from_this(), std::move(str_message))
        );
    }

    void WebsocketClient::write(std::string &&message) {
        _send_queue.push(std::move(message));
        auto session_number = _session_number.load();
        if (_send_queue.size() == 1) {
            doWrite(session_number);
        }
    }

    void WebsocketClient::doWrite(const unsigned long session_number) {
        if (_is_stopped || session_number != _session_number) return;
        if (_ws) {
            _ws->async_write(
                net::buffer(_send_queue.front()),
                beast::bind_front_handler(
                    &WebsocketClient::onWrite, shared_from_this(), session_number
                )
            );
        }
    }


    void WebsocketClient::onWrite(
        const unsigned long session_number, beast::error_code ec, std::size_t bytes_transferred
    ) {
        // we disconnect here because the reader is the only one who is allowed to disconnect
        if (_is_stopped) return;
        auto current_session_number = _session_number.load();
        if (!ec && bytes_transferred == _send_queue.front().size()) {
            _send_queue.pop();
            if (session_number == current_session_number && !_send_queue.empty()) {
                doWrite(session_number);
            }
            return;
        }
        if (session_number != current_session_number) return;

        if (ec) {
            std::cout << "WebsocketClient::onWrite failed to with ec: " << ec << "; bailing" << std::endl;
        } else if (bytes_transferred != _send_queue.front().size()) {
            std::cout << "WebsocketClient::onWrite only sent " << bytes_transferred << " of " <<
            _send_queue.front().size() << " bytes; bailing" << std::endl;
        }
        disconnect(session_number);
    }


    void WebsocketClient::disconnect(const unsigned long session_number) {
        if (_is_stopped || _session_number != session_number) return;
        auto next_session_number = ++_session_number;
        _manager->DestroyWebsocketClientConnection();
        if (_ws && _ws->is_open()) {
            _ws->async_close(
                websocket::close_code::normal,
                beast::bind_front_handler(&WebsocketClient::onDisconnect, shared_from_this(), next_session_number)
            );
        } else {
            startConnection(next_session_number, false);
        }
    }

    void WebsocketClient::onDisconnect(const unsigned long session_number, beast::error_code ec) {
        if (ec) {
            std::cout << "WebsocketSession::onClose failed to close gracefully" << std::endl;
        }
        if (!_is_stopped && session_number == _session_number) {
            startConnection(session_number, false);
        }
    }


}