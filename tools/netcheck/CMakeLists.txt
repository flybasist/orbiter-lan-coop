# olc-netcheck: a network check between two machines on the OLC network core (without Orbiter).
add_executable(olc-netcheck main.cpp)
target_link_libraries(olc-netcheck PRIVATE olc-core)
