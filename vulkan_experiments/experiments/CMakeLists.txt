# CMakeList.txt : CMake project for vulkan, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Add source to this project's executable.
file(GLOB TUTORIAL_SRCS "vulkan_tutorial.cpp")
file(GLOB TUTORIAL_HDRS "vulkan_tutorial.hpp")

add_executable(vulkan_tutorial ${TUTORIAL_SRCS} ${TUTORIAL_HDRS})
add_dependencies(vulkan_tutorial glfw tinyobjloader vulkan shaders)

target_link_libraries(vulkan_tutorial ${Vulkan_LIBRARIES} glfw tinyobjloader vulkan)

file(GLOB SHADERS "${CMAKE_SOURCE_DIR}/shaders/*.spv")
file(COPY ${SHADERS} DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/shaders/)
