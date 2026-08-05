cmake_minimum_required(VERSION 3.12)
project("unordered_dense-cmake-consumer" CXX)

# Consume unordered_dense the way a downstream project does that vendors the sources. Nothing
# else in CI configures CMakeLists.txt without ANKERL_ENABLE_MODULES, so a typo in the interface
# target's include directories or in its compile features would otherwise ship unnoticed.
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../.." unordered_dense)

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE unordered_dense::unordered_dense)
