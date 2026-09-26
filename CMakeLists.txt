cmake_minimum_required(VERSION 3.25)
project(Blast)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(EXTERN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extern")

add_subdirectory(${EXTERN_DIR}/glfw EXCLUDE_FROM_ALL glfw.out)

set(MI_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(MI_BUILD_OBJECT OFF CACHE BOOL "" FORCE)
set(MI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(MI_PADDING OFF CACHE BOOL "" FORCE)
add_subdirectory(${EXTERN_DIR}/mimalloc EXCLUDE_FROM_ALL mimalloc.out)

add_library(glm INTERFACE)
target_include_directories(glm INTERFACE ${EXTERN_DIR}/glm)
target_compile_definitions(glm INTERFACE GLM_ENABLE_EXPERIMENTAL=1 GLM_FORCE_DEPTH_ZERO_TO_ONE=1)

add_library(volk INTERFACE)
target_include_directories(volk INTERFACE ${EXTERN_DIR}/volk)

add_library(vma INTERFACE)
target_include_directories(vma INTERFACE ${EXTERN_DIR}/VulkanMemoryAllocator)

add_library(Vulkan-Headers INTERFACE)
target_include_directories(Vulkan-Headers INTERFACE ${EXTERN_DIR}/Vulkan-Headers)

file(GLOB IMGUI_SOURCES "${EXTERN_DIR}/imgui/*.cpp")
add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui PUBLIC ${EXTERN_DIR}/imgui)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_TESTS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_TOOLS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_DOC OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_VK_UPLOAD OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_GL_UPLOAD OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_LOADTEST_APPS OFF CACHE STRING "" FORCE)
# Basis transcoding dominates asset load time; compile KTX optimized even in
# Debug. Asserts stay enabled: lib/texture2.c hides a required DFD init call
# inside assert(), so NDEBUG must not reach the library.
set(BLAST_SAVED_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
set(BLAST_SAVED_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
if (MSVC)
    foreach(lang C CXX)
        string(REPLACE "/Od" "/O2" CMAKE_${lang}_FLAGS_DEBUG "${CMAKE_${lang}_FLAGS_DEBUG}")
        string(REPLACE "/Ob0" "/Ob2" CMAKE_${lang}_FLAGS_DEBUG "${CMAKE_${lang}_FLAGS_DEBUG}")
        string(REPLACE "/RTC1" "" CMAKE_${lang}_FLAGS_DEBUG "${CMAKE_${lang}_FLAGS_DEBUG}")
    endforeach()
endif()
add_subdirectory(${EXTERN_DIR}/KTX-Software EXCLUDE_FROM_ALL ktx.out)
if (MSVC)
    set(CMAKE_C_FLAGS_DEBUG "${BLAST_SAVED_C_FLAGS_DEBUG}")
    set(CMAKE_CXX_FLAGS_DEBUG "${BLAST_SAVED_CXX_FLAGS_DEBUG}")
endif()

add_subdirectory(engine)
add_subdirectory(game)
add_subdirectory(os)
