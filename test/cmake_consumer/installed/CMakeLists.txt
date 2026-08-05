cmake_minimum_required(VERSION 3.12)
project("unordered_dense-find-package-consumer" CXX)

# Consume unordered_dense the way someone does after `cmake --install`, or the way a
# distribution's package expects to be used. Point CMAKE_PREFIX_PATH at the install prefix.
#
# UNORDERED_DENSE_REQUIRED_VERSION exists so CI can also check that a version that is not there
# is actually refused, rather than only checking the happy path.
set(UNORDERED_DENSE_REQUIRED_VERSION "" CACHE STRING "version to demand from find_package")

find_package(unordered_dense ${UNORDERED_DENSE_REQUIRED_VERSION} CONFIG REQUIRED)

add_executable(consumer-installed ../main.cpp)
target_link_libraries(consumer-installed PRIVATE unordered_dense::unordered_dense)
