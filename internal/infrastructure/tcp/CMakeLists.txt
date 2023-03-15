
set(Boost_NO_WARN_NEW_VERSIONS 1)
find_package(Boost ${Boost_Version} REQUIRED COMPONENTS system regex)
include_directories(${BOOST_INCLUDEDIR})

add_library(tcp_server STATIC tcp_server.cpp)
target_link_libraries(tcp_server PRIVATE Boost::system Boost::regex)