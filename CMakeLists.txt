cmake_minimum_required(VERSION 3.25)
project(Blast)

set(CMAKE_CXX_STANDARD 23)

file(GLOB_RECURSE SOURCE_FILES "Source/*.h" "Source/*.cpp")
add_executable(Blast ${SOURCE_FILES})
target_include_directories(Blast PUBLIC "Source")

# spirv_reflect
add_library(spirv_reflect INTERFACE)
target_include_directories(spirv_reflect INTERFACE Extern/spirv_reflect)
target_link_libraries(Blast PUBLIC spirv_reflect)

# volk
add_library(volk INTERFACE)
target_include_directories(volk INTERFACE Extern/volk)
target_link_libraries(Blast PUBLIC volk)

# Vulkan-Headers
add_library(Vulkan-Headers INTERFACE)
target_include_directories(Vulkan-Headers INTERFACE Extern/Vulkan-Headers)
target_link_libraries(Blast PUBLIC Vulkan-Headers)

# VulkanMemoryAllocator
add_library(vma INTERFACE)
target_include_directories(vma INTERFACE Extern/VulkanMemoryAllocator)
target_link_libraries(Blast PUBLIC vma)