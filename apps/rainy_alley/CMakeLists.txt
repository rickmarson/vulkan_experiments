# CMakeList.txt : CMake project for vulkan, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Add source to this project's executable.
file(GLOB TUTORIAL_SRCS "rainy_alley.cpp")

add_executable(rainy_alley ${TUTORIAL_SRCS} ${TUTORIAL_HDRS})
add_dependencies(rainy_alley glfw imgui vulkan shaders)

target_link_libraries(rainy_alley ${Vulkan_LIBRARIES} vulkan glfw imgui)

file(GLOB SHADERS "${CMAKE_SOURCE_DIR}/shaders/shadow_map_vs.spv"
                  "${CMAKE_SOURCE_DIR}/shaders/alley_vs.spv"
                  "${CMAKE_SOURCE_DIR}/shaders/alley_fs.spv"
                  "${CMAKE_SOURCE_DIR}/shaders/rain_drops_vs.spv"
                  "${CMAKE_SOURCE_DIR}/shaders/rain_drops_gm.spv"
                  "${CMAKE_SOURCE_DIR}/shaders/rain_drops_fs.spv"
                  "${CMAKE_SOURCE_DIR}/shaders/rainfall_cp.spv")
set(SHADERS ${SHADERS} ${IMGUI_SHADERS})

file(GLOB OBJECTS "${CMAKE_SOURCE_DIR}/meshes/alley.glb")
file(GLOB TEXTURES "${CMAKE_SOURCE_DIR}/textures/rain_drops.png")

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/shaders/)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/meshes/)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/textures/)

foreach(SHADER_FILE IN LISTS SHADERS)
    add_custom_command(TARGET rainy_alley POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SHADER_FILE} ${CMAKE_CURRENT_BINARY_DIR}/shaders/
        COMMENT "Copying SPIRV shader: ${SHADER_FILE}"
    )
endforeach()

foreach(OBJECT_FILE IN LISTS OBJECTS)
    add_custom_command(TARGET rainy_alley POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${OBJECT_FILE} ${CMAKE_CURRENT_BINARY_DIR}/meshes/
        COMMENT "Copying object: ${OBJECT_FILE}"
    )
endforeach()

foreach(TEXTURE_FILE IN LISTS TEXTURES)
    add_custom_command(TARGET rainy_alley POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${TEXTURE_FILE} ${CMAKE_CURRENT_BINARY_DIR}/textures/
        COMMENT "Copying texture: ${TEXTURE_FILE}"
    )
endforeach()
