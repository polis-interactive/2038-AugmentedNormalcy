
set(SOURCES websocket_client.cpp websocket_server.cpp)
set(TARGET_LIBS pthread Boost::system Boost::regex)

add_library(websocket STATIC ${SOURCES})
target_link_libraries(websocket PRIVATE ${TARGET_LIBS})