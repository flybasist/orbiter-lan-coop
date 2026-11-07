cmake_minimum_required(VERSION 3.20)
project(orbiter_lan_coop CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Каталог проверочной сборки всегда один и тот же: orbiter-lan-coop.out
#   cmake -S . -B orbiter-lan-coop.out && cmake --build orbiter-lan-coop.out
# Он внесён в .gitignore, бинарники в репозиторий не попадают.

# Предупреждения компилятора
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra)
endif()

# Тесты запускаются через ctest --test-dir orbiter-lan-coop.out
enable_testing()

# Сетевое ядро (не зависит от Orbiter) и его тесты
add_subdirectory(core)
add_subdirectory(tests)

# Плагин для Orbiter: только на Windows и только если указан каталог SDK собранного
# Orbiter (plugin/CMakeLists.txt). На Mac и Linux собирается только ядро.
set(OLC_ORBITER_SDK_DIR "" CACHE PATH "Orbiter SDK directory with include/ and lib/ (Orbiter.lib, Orbitersdk.lib)")
if(WIN32 AND OLC_ORBITER_SDK_DIR)
    # Путь мог прийти с обратными слешами Windows - приводим к виду CMake.
    file(TO_CMAKE_PATH "${OLC_ORBITER_SDK_DIR}" OLC_ORBITER_SDK_DIR)
    add_subdirectory(plugin)
endif()

# Симуляция сглаживания движения (измерения до написания сетевого плагина)
add_subdirectory(tools/smoothing)
