/*
* vulkan_app.hpp
*
* Copyright (C) 2020 Riccardo Marson
*/

#pragma once

#include "vulkan_backend.hpp"

struct GLFWwindow;

class VulkanApp {
public:
    bool setup();
    bool run();

    void onWindowResized() { window_resized_ = true; }

protected:
    // boilerplate creation and cleanup
    void setWindowTitle(const std::string& title) { window_title_ = title; }
    void createWindow();
    bool recreateSwapChain();
    void mainLoop();

    virtual bool createGraphicsPipeline() = 0;

    // actual rendering code
    void drawFrame();

    virtual bool loadAssets() = 0;
    virtual bool setupScene() = 0;
    virtual void updateScene() = 0;
    virtual RecordCommandsResult recordCommands(uint32_t swapchain_image) = 0;
    virtual void cleanupSwapChainAssets() = 0;
    virtual void cleanup() = 0;

    // window
    const uint32_t WIDTH = 1600;
    const uint32_t HEIGHT = 1200;
    GLFWwindow* window_ = nullptr;
    bool window_resized_ = false;
    std::string window_title_;

    VulkanBackend vulkan_backend_;
};
