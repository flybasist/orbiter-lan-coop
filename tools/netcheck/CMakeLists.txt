# olc-netcheck: проверка сети между двумя машинами на сетевом ядре OLC (без Orbiter).
add_executable(olc-netcheck main.cpp)
target_link_libraries(olc-netcheck PRIVATE olc-core)
