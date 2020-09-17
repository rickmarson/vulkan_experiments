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
file(GLOB TEXTURES "${CMAKE_SOURCE_DIR}/textures/*.jpg")

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/shaders/)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/textures/)

foreach(SHADER_FILE IN LISTS SHADERS)
    add_custom_command(TARGET vulkan_tutorial POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SHADER_FILE} ${CMAKE_CURRENT_BINARY_DIR}/shaders/
        COMMENT "Copying SPIRV shader: ${SHADER_FILE}"
    )
endforeach()

foreach(TEXTURE_FILE IN LISTS TEXTURES)
    add_custom_command(TARGET vulkan_tutorial POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${TEXTURE_FILE} ${CMAKE_CURRENT_BINARY_DIR}/textures/
        COMMENT "Copying texture: ${TEXTURE_FILE}"
    )
endforeach()