# olc-core: the OLC network core. It does not depend on Orbiter, so it builds and
# is tested on any system. The plugin (plugin/) uses it.
add_library(olc-core STATIC
    attitude.cpp
    client_clock.cpp
    client_link.cpp
    header.cpp
    host_link.cpp
    interpolation.cpp
    messages.cpp
    protocol.cpp
    serialize.cpp
    session_table.cpp
    socket.cpp
)
target_include_directories(olc-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Winsock on Windows
if(WIN32)
    target_link_libraries(olc-core PUBLIC ws2_32)
endif()
