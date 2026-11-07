# olc-core: сетевое ядро OLC. Не зависит от Orbiter, поэтому собирается и
# тестируется на любой системе. Его использует плагин (plugin/).
add_library(olc-core STATIC
    attitude.cpp
    header.cpp
    interpolation.cpp
    messages.cpp
    protocol.cpp
    serialize.cpp
    session_table.cpp
    socket.cpp
)
target_include_directories(olc-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Winsock на Windows
if(WIN32)
    target_link_libraries(olc-core PUBLIC ws2_32)
endif()
