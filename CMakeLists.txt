cmake_minimum_required(VERSION 3.25)
project(Blast)

set(CMAKE_CXX_STANDARD 23)
set(BINARY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Bin")
set(EXTERN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Extern")
set(ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Assets")

add_subdirectory(${EXTERN_DIR}/glfw EXCLUDE_FROM_ALL glfw.out)

add_subdirectory(Source)