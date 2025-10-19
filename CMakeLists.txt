cmake_minimum_required(VERSION 3.25)
project(Blast)

set(CMAKE_CXX_STANDARD 23)

file(GLOB_RECURSE SOURCE_FILES "Source/*.h" "Source/*.cpp")
add_executable(Blast ${SOURCE_FILES})
target_include_directories(Blast PUBLIC "src")

# volk
add_library(volk STATIC Extern/volk/volk.c Extern/volk/volk.h)
target_include_directories(volk PUBLIC Extern/volk)
target_include_directories(volk PUBLIC Extern/Vulkan-headers/include)
if (WIN32)
    target_compile_definitions(volk PRIVATE VK_USE_PLATFORM_WIN32_KHR)
endif()
target_link_libraries(Blast PUBLIC volk)

