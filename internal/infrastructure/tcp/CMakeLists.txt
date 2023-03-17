
set(Boost_NO_WARN_NEW_VERSIONS 1)
find_package(Boost ${Boost_Version} REQUIRED COMPONENTS system regex)
include_directories(${BOOST_INCLUDEDIR})

add_library(tcp STATIC tcp_server.cpp tcp_client.cpp)
target_link_libraries(tcp PRIVATE Boost::system Boost::regex)
