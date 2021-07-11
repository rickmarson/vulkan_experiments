# CMakeList.txt : CMake project for vulkan, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Add source to this project's executable.
file(GLOB TUTORIAL_SRCS "model_viewer.cpp")

add_executable(model_viewer ${TUTORIAL_SRCS} ${TUTORIAL_HDRS})
add_dependencies(model_viewer glfw imgui vulkan shaders)

target_link_libraries(model_viewer ${Vulkan_LIBRARIES} vulkan glfw imgui)

install(TARGETS model_viewer
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
