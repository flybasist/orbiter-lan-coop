# Network core tests. No third-party libraries: the checks are in check.h.
# Run: ctest --test-dir orbiter-lan-coop.out

add_executable(test-protocol test_protocol.cpp)
target_link_libraries(test-protocol PRIVATE olc-core)
add_test(NAME protocol COMMAND test-protocol)

add_executable(test-serialize test_serialize.cpp)
target_link_libraries(test-serialize PRIVATE olc-core)
add_test(NAME serialize COMMAND test-serialize)

add_executable(test-header test_header.cpp)
target_link_libraries(test-header PRIVATE olc-core)
add_test(NAME header COMMAND test-header)

add_executable(test-messages test_messages.cpp)
target_link_libraries(test-messages PRIVATE olc-core)
add_test(NAME messages COMMAND test-messages)

add_executable(test-attitude test_attitude.cpp)
target_link_libraries(test-attitude PRIVATE olc-core)
add_test(NAME attitude COMMAND test-attitude)

add_executable(test-interpolation test_interpolation.cpp)
target_link_libraries(test-interpolation PRIVATE olc-core)
add_test(NAME interpolation COMMAND test-interpolation)

add_executable(test-session-table test_session_table.cpp)
target_link_libraries(test-session-table PRIVATE olc-core)
add_test(NAME session_table COMMAND test-session-table)

add_executable(test-socket test_socket.cpp)
target_link_libraries(test-socket PRIVATE olc-core)
add_test(NAME socket COMMAND test-socket)

add_executable(test-client-clock test_client_clock.cpp)
target_link_libraries(test-client-clock PRIVATE olc-core)
add_test(NAME client_clock COMMAND test-client-clock)

add_executable(test-link test_link.cpp)
target_link_libraries(test-link PRIVATE olc-core)
add_test(NAME link COMMAND test-link)
