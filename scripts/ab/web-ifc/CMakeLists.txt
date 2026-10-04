cmake_minimum_required(VERSION 3.28)
project(wifc320 LANGUAGES CXX)
set(WEBIFC_SRC "" CACHE PATH "web-ifc src/cpp")
add_subdirectory(${WEBIFC_SRC} webifc EXCLUDE_FROM_ALL)
add_executable(bench_load bench_load.cpp)
target_link_libraries(bench_load PRIVATE web-ifc-library)
# web-ifc adds its dependencies' include directories PRIVATE to its library, so bench_load names them again
include(FetchContent)
foreach(dep tinynurbs fastfloat cdt earcut spdlog stduuid unordered_dense)
    FetchContent_GetProperties(${dep})
endforeach()
target_include_directories(bench_load PRIVATE ${tinynurbs_SOURCE_DIR}/include ${fastfloat_SOURCE_DIR}/include
    ${cdt_SOURCE_DIR}/CDT/include ${earcut_SOURCE_DIR}/include ${spdlog_SOURCE_DIR}/include
    ${stduuid_SOURCE_DIR}/include ${unordered_dense_SOURCE_DIR}/include)
