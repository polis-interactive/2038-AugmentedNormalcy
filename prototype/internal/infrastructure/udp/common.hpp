//
// Created by bruce on 12/29/2022.
//

#ifndef AUGMENTEDNORMALCY_PROTOTYPE_UDP_COMMON_HPP
#define AUGMENTEDNORMALCY_PROTOTYPE_UDP_COMMON_HPP


#include <boost/asio.hpp>
#include "infrastructure/common.hpp"

namespace net = boost::asio;
using boost::asio::ip::udp;
using boost::system::error_code;
namespace err = boost::system::errc;
typedef  net::basic_datagram_socket<udp, net::io_context::executor_type> udp_socket;


#endif //AUGMENTEDNORMALCY_PROTOTYPE_UDP_COMMON_HPP
