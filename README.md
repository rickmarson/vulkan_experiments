A simple framework for experimenting with the Vulkan API.

Most of the original boilerplate code comes from the [Vulkan Tutorial](https://vulkan-tutorial.com/Introduction), but it has been extensively refactored and modified to be more flexible and usable in a wider range of applications. 

Major differences include:

- Integration of the [SPIRV-Reflect](https://github.com/KhronosGroup/SPIRV-Reflect.git) library to automate parts of the graphics pipeline creation
- Integration wit [ImGui](https://github.com/ocornut/imgui.git)
- Use of subpasses
- Use of secondary command buffers

## Experiments

### Model Viewer

Loads and displays a Wavefront OBJ model. Only supports 1 material per model.
This is essentially a fresh re-implementation of the Vulkan Tutorial to test the framework.

# ![viewer](/pics/model_viewer.png)

## Dependencies

- [CMake](https://cmake.org/) 3.8 or higher
- The [LunarG Vulkan SDK](https://vulkan.lunarg.com/) must be installed in the system
- Run `git submodule update --init --recursive` to initialise thirdparty dependencies (they will all be placed in `./utilities`)

### List of third party library 

- [glfw](https://github.com/glfw/glfw.git)
- [glm](https://github.com/g-truc/glm.git)
- [imgui](https://github.com/ocornut/imgui.git)
- [tinyobjloader](https://github.com/tinyobjloader/tinyobjloader.git)
- [stb](https://github.com/nothings/stb.git)
- [SPIRV-Reflect](https://github.com/KhronosGroup/SPIRV-Reflect.git)

## Compatibility 

A discrete GPU is likely required. To simplify the code a bit a few assumptions have been made on the support and/or availability of device features. 

Tested with:
- Vulkan 1.2.148
- Windows 10 + Visual Studio 2019
- Ubuntu 20.04 + gcc 9.3.0

## Docker

A Docker image that can be used as development environment is provided for Ubuntu 20.04 and NVidia GPUs:

```bash
docker pull nevil82/vulkan_experiments:vulkan_base_ubuntu20.04
```

There's also an additional image packaging the examples executables:

```bash
docker pull nevil82/vulkan_experiments:1.0.0
```

Look in `docker/linux` for Dockerfile(s) and rules to build and run.  
