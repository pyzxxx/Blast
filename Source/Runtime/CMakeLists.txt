project(Runtime)

file(GLOB_RECURSE SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.h" "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_executable(Runtime ${SOURCE_FILES})
target_include_directories(Runtime PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(Runtime PUBLIC Base)