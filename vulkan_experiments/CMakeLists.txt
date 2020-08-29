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

# Include sub-projects.
add_subdirectory("utilities/glfw")
add_subdirectory("utilities/tinyobjloader")
add_subdirectory ("vulkan")

