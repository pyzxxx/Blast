project(Tools)

file(GLOB_RECURSE SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.h" "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_executable(Tools ${SOURCE_FILES})
target_include_directories(Tools PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(Tools PUBLIC Base)

# Reuse precompiled header from Base
target_precompile_headers(Tools REUSE_FROM Base)

# cgltf
add_library(cgltf INTERFACE)
target_include_directories(cgltf INTERFACE ${EXTERN_DIR}/cgltf)
target_link_libraries(Tools PUBLIC cgltf)

# stb_image (header only)
target_include_directories(Tools PUBLIC ${EXTERN_DIR}/stb)

set_target_properties(Tools PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${BINARY_DIR}"
)