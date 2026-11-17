cmake_minimum_required(VERSION 3.20)
project(orbiter_lan_coop CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The check build folder is always the same: orbiter-lan-coop.out
#   cmake -S . -B orbiter-lan-coop.out && cmake --build orbiter-lan-coop.out
# It is listed in .gitignore, binaries never get into the repository.

# Compiler warnings
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra)
endif()

# Tests run with ctest --test-dir orbiter-lan-coop.out
enable_testing()

# Network core (does not depend on Orbiter) and its tests
add_subdirectory(core)
add_subdirectory(tests)

# The Orbiter plugin: only on Windows and only if the SDK folder of a built
# Orbiter is given (plugin/CMakeLists.txt). On Mac and Linux only the core builds.
set(OLC_ORBITER_SDK_DIR "" CACHE PATH "Orbiter SDK directory with include/ and lib/ (Orbiter.lib, Orbitersdk.lib)")
if(WIN32 AND OLC_ORBITER_SDK_DIR)
    # The path may come with Windows backslashes - convert it to CMake form.
    file(TO_CMAKE_PATH "${OLC_ORBITER_SDK_DIR}" OLC_ORBITER_SDK_DIR)
    add_subdirectory(plugin)
endif()

# Network check between two machines (docs/STANDS.md)
add_subdirectory(tools/netcheck)

# English and Russian documentation in sync (docs/README.md, decision 29)
add_subdirectory(tools/doccheck)

# Motion smoothing simulation (measurements made before writing the network plugin)
add_subdirectory(tools/smoothing)
