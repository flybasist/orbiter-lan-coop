# olc-doccheck: checks that English and Russian documents have the same structure (decision 29).
# It runs as a CTest test, so CI catches a document changed in only one language.
add_executable(olc-doccheck main.cpp)
add_test(NAME doccheck COMMAND olc-doccheck ${PROJECT_SOURCE_DIR})
