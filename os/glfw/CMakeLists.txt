add_library(os_glfw STATIC os_glfw.cpp os_glfw.h)
target_include_directories(os_glfw PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(os_glfw PUBLIC engine glfw)
target_compile_definitions(os_glfw PRIVATE BLAST_PROJECT_DIR="${CMAKE_SOURCE_DIR}")

add_executable(runtime WIN32 main.cpp)
target_link_libraries(runtime PRIVATE engine os_glfw viewer)
target_link_options(runtime PRIVATE /ENTRY:mainCRTStartup)
set_target_properties(runtime PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
