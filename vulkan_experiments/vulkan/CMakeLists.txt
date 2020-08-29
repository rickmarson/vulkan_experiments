# CMakeList.txt : CMake project for vulkan, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Add source to this project's executable.
file(GLOB SRCS "*.cpp")
file(GLOB HDRS ".hpp .h")

add_executable(vulkan ${SRCS} ${HDRS})
add_dependencies(vulkan glfw)
add_dependencies(vulkan tinyobjloader)

target_link_libraries(vulkan ${Vulkan_LIBRARIES} glfw tinyobjloader)
