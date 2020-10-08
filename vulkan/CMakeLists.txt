# CMakeList.txt : CMake project for vulkan, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Compile SPIRV-Reflect directly into this library
file(GLOB SPIRV_REFLECT_SRCS ${SPIRV_REFLECT_FOLDER}/spirv_reflect.h ${SPIRV_REFLECT_FOLDER}/spirv_reflect.c)

# Add source to this project's executable.
file(GLOB SHARED_SRCS "*.cpp")
file(GLOB SHARED_HDRS ".hpp")

add_library(vulkan ${SHARED_SRCS} ${SHARED_HDRS} ${SPIRV_REFLECT_SRCS})
add_dependencies(vulkan glfw tinyobjloader imgui)
