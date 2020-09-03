# CMakeList.txt : CMake project for vulkan, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Add source to this project's executable.
file(GLOB SHARED_SRCS "*.cpp")
file(GLOB SHARED_HDRS ".hpp")

add_library(vulkan ${SHARED_SRCS} ${SHARED_HDRS})
add_dependencies(vulkan glfw tinyobjloader)
