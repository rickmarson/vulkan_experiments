# CMakeList.txt : CMake project for vulkan, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Add source to this project's executable.
file(GLOB TUTORIAL_SRCS "rainy_alley.cpp")

add_executable(rainy_alley ${TUTORIAL_SRCS} ${TUTORIAL_HDRS})
add_dependencies(rainy_alley glfw imgui vulkan shaders)

target_link_libraries(rainy_alley ${Vulkan_LIBRARIES} vulkan glfw imgui)

install(TARGETS rainy_alley
          CONFIGURATIONS Debug Release
          RUNTIME DESTINATION "bin")

install(DIRECTORY "${CMAKE_SOURCE_DIR}/shaders" 
           DESTINATION "bin"
           FILES_MATCHING PATTERN "*.spv")

install(DIRECTORY "${CMAKE_SOURCE_DIR}/meshes" 
           DESTINATION "bin"
           FILES_MATCHING PATTERN "*.glb")

install(DIRECTORY "${CMAKE_SOURCE_DIR}/textures" 
           DESTINATION "bin")
