# olc-core: сетевое ядро OLC. Не зависит от Orbiter, поэтому собирается и
# тестируется на любой системе. Плагин для Orbiter подключит его позже.
add_library(olc-core STATIC
    header.cpp
    messages.cpp
    protocol.cpp
    serialize.cpp
)
target_include_directories(olc-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
