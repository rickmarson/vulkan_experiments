/*
* vulkan_app.hpp
*
* Copyright (C) 2020 Riccardo Marson
*/

#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "vulkan_backend.hpp"

class VulkanApp {
public:
    bool setup();
    bool run();

    void onWindowResized() { window_resized_ = true; }

protected:
    // boilerplate creation and cleanup
    void createWindow();
    bool recreateSwapChain();

    virtual bool createGraphicsPipeline() = 0;
    virtual bool recordCommands() = 0;

    // actual rendering code
    void mainLoop();
    void drawFrame();

    virtual bool loadAssets() = 0;
    virtual bool setupScene() = 0;
    virtual void updateScene() = 0;
    virtual void cleanup() = 0;

    // window
    const uint32_t WIDTH = 800;
    const uint32_t HEIGHT = 600;
    GLFWwindow* window_ = nullptr;
    bool window_resized_ = false;

    VulkanBackend vulkan_backend_;
};
