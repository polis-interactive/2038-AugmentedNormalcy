
set(SOURCES)
set(TARGET_LIBS pthread Boost::system Boost::regex)

if (NOT AN_PLATFORM_TYPE STREQUAL JETSON)
    set(SOURCES ${SOURCES} websocket_client.cpp)
endif()

if (NOT AN_PLATFORM_TYPE STREQUAL RPI)
    set(SOURCES ${SOURCES} websocket_server.cpp)
endif()

add_library(websocket STATIC ${SOURCES})
target_link_libraries(websocket PRIVATE ${TARGET_LIBS})