# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

project ("vulkan_experiments")

set(CMAKE_CXX_STANDARD 17)

find_package(Vulkan REQUIRED FATAL_ERROR)

include_directories(${Vulkan_INCLUDE_DIRS})
include_directories("utilities/glm")
include_directories("utilities/glfw/include")
include_directories("utilities/stb")
include_directories("utilities/SPIRV-Reflect")
include_directories("vulkan")

# compile shaders
if(WIN32)
	set(SCRIPT_EXT ".bat")
else()
	set(SCRIPT_EXT ".sh")
endif()

set(COMPILE_SHADERS_CMD "${CMAKE_SOURCE_DIR}/compile_shaders${SCRIPT_EXT}")
add_custom_target(shaders ALL ${COMPILE_SHADERS_CMD} ${CMAKE_SOURCE_DIR})

set(SPIRV_REFLECT_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}/utilities/SPIRV-Reflect)

# Include sub-projects.
add_subdirectory("utilities/glfw")
add_subdirectory("utilities/tinyobjloader")
add_subdirectory("vulkan")
add_subdirectory("experiments")
