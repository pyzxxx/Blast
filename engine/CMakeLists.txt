file(GLOB SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
list(APPEND SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/job_system.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/job_system.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/load_pipeline.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/load_pipeline.h")

add_library(engine STATIC ${SOURCES})
target_include_directories(engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${EXTERN_DIR}/mimalloc/include
                                         ${CMAKE_SOURCE_DIR}/assets/shaders)
target_link_libraries(engine PUBLIC glm volk Vulkan-Headers imgui vma mimalloc-static ktx)
