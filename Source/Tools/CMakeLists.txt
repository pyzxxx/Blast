project(Tools)

file(GLOB_RECURSE SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.h" "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_executable(Tools ${SOURCE_FILES})
target_include_directories(Tools PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(Tools PUBLIC Base)

set_target_properties(Tools PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${BINARY_DIR}"
)