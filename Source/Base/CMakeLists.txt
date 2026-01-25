project(Base)

set(BASE_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

file(GLOB_RECURSE SOURCE_FILES "${BASE_ROOT_DIR}/*.h" "${BASE_ROOT_DIR}/*.cpp")
add_library(Base STATIC ${SOURCE_FILES})

# glm
target_include_directories(Base PUBLIC ${EXTERN_DIR}/glm)
target_compile_definitions(Base PUBLIC
    GLM_ENABLE_EXPERIMENTAL=1
    GLM_FORCE_DEPTH_ZERO_TO_ONE=1
)

# spirv_reflect
add_library(spirv_reflect STATIC ${EXTERN_DIR}/spirv_reflect/spirv_reflect.c ${EXTERN_DIR}/spirv_reflect/spirv_reflect.h)
target_include_directories(Base PUBLIC ${EXTERN_DIR}/spirv_reflect)
target_link_libraries(Base PUBLIC spirv_reflect)

# volk
add_library(volk INTERFACE)
target_include_directories(volk INTERFACE ${EXTERN_DIR}/volk)
target_link_libraries(Base PUBLIC volk)

# Vulkan-Headers
add_library(Vulkan-Headers INTERFACE)
target_include_directories(Vulkan-Headers INTERFACE ${EXTERN_DIR}/Vulkan-Headers)
target_link_libraries(Base PUBLIC Vulkan-Headers)

# VulkanMemoryAllocator
add_library(vma INTERFACE)
target_include_directories(vma INTERFACE ${EXTERN_DIR}/VulkanMemoryAllocator)
target_link_libraries(Base PUBLIC vma)