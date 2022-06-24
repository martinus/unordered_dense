cmake_minimum_required(VERSION 3.12)
project("unordered_dense_map"
    VERSION 0.0.1
    DESCRIPTION "A fast & densely stored hashmap based on robin-hood backward shift deletion."
    HOMEPAGE_URL "https://github.com/martinus/unordered_dense_map")

# determine whether this is a standalone project or included by other projects
set(_unordered_dense_map_is_included ON)
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(_unordered_dense_map_is_included OFF)
endif()

if(_unordered_dense_map_is_included)
    add_library(unordered_dense_map INTERFACE)
    add_library(unordered_dense_map::unordered_dense_map ALIAS unordered_dense_map)
    target_compile_features(unordered_dense_map INTERFACE cxx_std_17)
    target_include_directories(
        unordered_dense_map
        INTERFACE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
endif()
