
add_library(tcp STATIC tcp_server.cpp tcp_client.cpp)
target_link_libraries(tcp PRIVATE Boost::system Boost::regex)
