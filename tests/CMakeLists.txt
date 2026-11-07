# Тесты сетевого ядра. Без сторонних библиотек: проверки - в check.h.
# Запуск: ctest --test-dir orbiter-lan-coop.out

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
