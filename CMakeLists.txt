cmake_minimum_required(VERSION 3.12)
project("unordered_dense"
    VERSION 1.0.2
    DESCRIPTION "A fast & densely stored hashmap and hashset based on robin-hood backward shift deletion"
    HOMEPAGE_URL "https://github.com/martinus/unordered_dense")

# determine whether this is a standalone project or included by other projects
set(_unordered_dense_is_included ON)
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(_unordered_dense_is_included OFF)
endif()

if(_unordered_dense_is_included)
    add_library(unordered_dense INTERFACE)
    add_library(unordered_dense::unordered_dense ALIAS unordered_dense)
    target_compile_features(unordered_dense INTERFACE cxx_std_17)
    target_include_directories(
        unordered_dense
        INTERFACE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
endif()
