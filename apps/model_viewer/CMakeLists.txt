# CMakeList.txt : CMake project for vulkan, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Add source to this project's executable.
file(GLOB TUTORIAL_SRCS "model_viewer.cpp")

add_executable(model_viewer ${TUTORIAL_SRCS} ${TUTORIAL_HDRS})
add_dependencies(model_viewer glfw tinyobjloader imgui vulkan shaders)

target_link_libraries(model_viewer ${Vulkan_LIBRARIES} vulkan glfw tinyobjloader imgui)

file(GLOB SHADERS "${CMAKE_SOURCE_DIR}/shaders/model_viewer_vs.spv"
                  "${CMAKE_SOURCE_DIR}/shaders/model_viewer_fs.spv")
set(SHADERS ${SHADERS} ${IMGUI_SHADERS})

file(GLOB OBJECTS "${CMAKE_SOURCE_DIR}/meshes/viking_room.obj" 
                  "${CMAKE_SOURCE_DIR}/meshes/viking_room.mtl")
file(GLOB TEXTURES "${CMAKE_SOURCE_DIR}/textures/viking_room.png")

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/shaders/)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/meshes/)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/textures/)

foreach(SHADER_FILE IN LISTS SHADERS)
    add_custom_command(TARGET model_viewer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SHADER_FILE} ${CMAKE_CURRENT_BINARY_DIR}/shaders/
        COMMENT "Copying SPIRV shader: ${SHADER_FILE}"
    )
endforeach()

foreach(OBJECT_FILE IN LISTS OBJECTS)
    add_custom_command(TARGET model_viewer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${OBJECT_FILE} ${CMAKE_CURRENT_BINARY_DIR}/meshes/
        COMMENT "Copying object: ${OBJECT_FILE}"
    )
endforeach()

foreach(TEXTURE_FILE IN LISTS TEXTURES)
    add_custom_command(TARGET model_viewer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${TEXTURE_FILE} ${CMAKE_CURRENT_BINARY_DIR}/textures/
        COMMENT "Copying texture: ${TEXTURE_FILE}"
    )
endforeach()
