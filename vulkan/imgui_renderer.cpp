/*
* imgui_renderer.hpp
*
* Copyright (C) 2020 Riccardo Marson
*/

#include "imgui_renderer.hpp"
#include "vulkan_backend.hpp"
#include "texture.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#ifdef _WIN32
#undef APIENTRY
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>   // for glfwGetWin32Window
#endif

#include <algorithm>


// Internal functions

namespace {

    struct GLFWCallbacks {
        GLFWmousebuttonfun   mouseButtonCallback = nullptr;
        GLFWscrollfun        scrollCallback = nullptr;
        GLFWkeyfun           keyCallback = nullptr;
        GLFWcharfun          charCallback = nullptr;
    };

    struct GLFWInterface {
        GLFWwindow* glfw_window = nullptr;
        double      time = 0.0;
        bool        mouse_just_pressed[ImGuiMouseButton_COUNT] = {};
        GLFWcursor* glfw_mouse_cursors[ImGuiMouseCursor_COUNT] = {};
       
        GLFWCallbacks imgui_callbacks;
        GLFWCallbacks external_callbacks;
    };

    static GLFWInterface glfw_interface;


    static const char* GetClipboardText(void* user_data)
    {
        return glfwGetClipboardString((GLFWwindow*)user_data);
    }

    static void SetClipboardText(void* user_data, const char* text)
    {
        glfwSetClipboardString((GLFWwindow*)user_data, text);
    }

    void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
    {
        if (glfw_interface.external_callbacks.mouseButtonCallback != nullptr)
            glfw_interface.external_callbacks.mouseButtonCallback(window, button, action, mods);

        if (action == GLFW_PRESS && button >= 0 && button < IM_ARRAYSIZE(glfw_interface.mouse_just_pressed))
            glfw_interface.mouse_just_pressed[button] = true;
    }

    void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
    {
        if (glfw_interface.external_callbacks.scrollCallback != NULL)
            glfw_interface.external_callbacks.scrollCallback(window, xoffset, yoffset);

        ImGuiIO& io = ImGui::GetIO();
        io.MouseWheelH += (float)xoffset;
        io.MouseWheel += (float)yoffset;
    }

    void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
    {
        if (glfw_interface.external_callbacks.keyCallback != NULL)
            glfw_interface.external_callbacks.keyCallback(window, key, scancode, action, mods);

        ImGuiIO& io = ImGui::GetIO();
        if (action == GLFW_PRESS)
            io.KeysDown[key] = true;
        if (action == GLFW_RELEASE)
            io.KeysDown[key] = false;

        // Modifiers are not reliable across systems
        io.KeyCtrl = io.KeysDown[GLFW_KEY_LEFT_CONTROL] || io.KeysDown[GLFW_KEY_RIGHT_CONTROL];
        io.KeyShift = io.KeysDown[GLFW_KEY_LEFT_SHIFT] || io.KeysDown[GLFW_KEY_RIGHT_SHIFT];
        io.KeyAlt = io.KeysDown[GLFW_KEY_LEFT_ALT] || io.KeysDown[GLFW_KEY_RIGHT_ALT];
#ifdef _WIN32
        io.KeySuper = false;
#else
        io.KeySuper = io.KeysDown[GLFW_KEY_LEFT_SUPER] || io.KeysDown[GLFW_KEY_RIGHT_SUPER];
#endif
    }

    void CharCallback(GLFWwindow* window, unsigned int c)
    {
        if (glfw_interface.external_callbacks.charCallback != NULL)
            glfw_interface.external_callbacks.charCallback(window, c);

        ImGuiIO& io = ImGui::GetIO();
        io.AddInputCharacter(c);
    }

    VertexFormatInfo getVertexFormatInfo() {
        std::vector<size_t> offsets = { offsetof(ImDrawVert, pos), offsetof(ImDrawVert, uv), offsetof(ImDrawVert, col) };
        return { sizeof(ImDrawVert) , offsets };
    }
}

// ImGuiRenderer

std::unique_ptr<ImGuiRenderer> ImGuiRenderer::create(VulkanBackend* backend) {
    return std::make_unique<ImGuiRenderer>(backend);
}

ImGuiRenderer::ImGuiRenderer(VulkanBackend* backend) :
    vulkan_backend_(backend) {

}

bool ImGuiRenderer::setUp(GLFWWindowHandle window) {
    InitImGui(window);
    InitVulkanAssets();
  
    return true;
}

void ImGuiRenderer::shutDown() {
    vulkan_backend_->destroyBuffer(index_buffer_);
    vulkan_backend_->destroyBuffer(vertex_buffer_);
    fonts_texture_.reset();
    imgui_vertex_shader_.reset();
    imgui_fragment_shader_.reset();
    
    cleanupGraphicsPipeline();

    vulkan_backend_->freeCommandBuffers(vk_drawing_buffers_);

    glfwSetMouseButtonCallback(glfw_interface.glfw_window, glfw_interface.external_callbacks.mouseButtonCallback);
    glfwSetScrollCallback(glfw_interface.glfw_window, glfw_interface.external_callbacks.scrollCallback);
    glfwSetKeyCallback(glfw_interface.glfw_window, glfw_interface.external_callbacks.keyCallback);
    glfwSetCharCallback(glfw_interface.glfw_window, glfw_interface.external_callbacks.charCallback);
    
    ImGui::DestroyContext();
}

bool ImGuiRenderer::createGraphicsPipeline(RenderPass& render_pass, uint32_t subpass_number) {
    GraphicsPipelineConfig config;
    config.name = "UI Overlay";
    config.vertex = imgui_vertex_shader_;
    config.fragment = imgui_fragment_shader_;
    config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    config.vertex_buffer_binding_desc = imgui_vertex_shader_->getInputBindingDescription();
    config.vertex_buffer_attrib_desc = imgui_vertex_shader_->getInputAttributes();
    config.render_pass = render_pass;
    config.subpass_number = subpass_number;
    config.cullBackFace = false;
    config.enableDepthTesting = false;
    config.enableTransparency = true;

    ui_pipeline_ = vulkan_backend_->createGraphicsPipeline(config);

    createDescriptorSets(ui_pipeline_.vk_descriptor_set_layouts);
    updateDescriptorSets(ui_pipeline_.descriptor_metadata);

    subpass_number_ = subpass_number;

    return ui_pipeline_.vk_pipeline != VK_NULL_HANDLE;
}

void ImGuiRenderer::cleanupGraphicsPipeline() {
    vulkan_backend_->destroyPipeline(ui_pipeline_);
}

DescriptorPoolConfig ImGuiRenderer::getDescriptorsCount() const {
    DescriptorPoolConfig config;
    config.image_samplers_count = 1;
    return config;
}

void ImGuiRenderer::beginFrame() {
    ImGuiIO& io = ImGui::GetIO();
   
    // Setup display size (every frame to accommodate for window resizing)
    int w, h;
    int display_w, display_h;
    glfwGetWindowSize(glfw_interface.glfw_window, &w, &h);
    glfwGetFramebufferSize(glfw_interface.glfw_window, &display_w, &display_h);
    io.DisplaySize = ImVec2((float)w, (float)h);
    if (w > 0 && h > 0)
        io.DisplayFramebufferScale = ImVec2((float)display_w / w, (float)display_h / h);

    // Setup time step
    double current_time = glfwGetTime();
    io.DeltaTime = glfw_interface.time > 0.0 ? (float)(current_time - glfw_interface.time) : (float)(1.0f / 60.0f);
    glfw_interface.time = current_time;

    // update mouse position, cursor and buttons

    for (int i = 0; i < IM_ARRAYSIZE(io.MouseDown); i++)
    {
        io.MouseDown[i] = glfw_interface.mouse_just_pressed[i] || glfwGetMouseButton(glfw_interface.glfw_window, i) != 0;
        glfw_interface.mouse_just_pressed[i] = false;
    }

    // Update mouse position
    const ImVec2 mouse_pos_backup = io.MousePos;
    io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
#ifdef __EMSCRIPTEN__
    const bool focused = true; // Emscripten
#else
    const bool focused = glfwGetWindowAttrib(glfw_interface.glfw_window, GLFW_FOCUSED) != 0;
#endif
    if (focused)
    {
        if (io.WantSetMousePos)
        {
            glfwSetCursorPos(glfw_interface.glfw_window, (double)mouse_pos_backup.x, (double)mouse_pos_backup.y);
        }
        else
        {
            double mouse_x, mouse_y;
            glfwGetCursorPos(glfw_interface.glfw_window, &mouse_x, &mouse_y);
            io.MousePos = ImVec2((float)mouse_x, (float)mouse_y);
        }
    }

    if ((io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange) || glfwGetInputMode(glfw_interface.glfw_window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED) {
        ImGui::NewFrame();
        return;
    }

    ImGuiMouseCursor imgui_cursor = ImGui::GetMouseCursor();
    if (imgui_cursor == ImGuiMouseCursor_None || io.MouseDrawCursor)
    {
        // Hide OS mouse cursor if imgui is drawing it or if it wants no cursor
        glfwSetInputMode(glfw_interface.glfw_window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
    }
    else
    {
        // Show OS mouse cursor
        // FIXME-PLATFORM: Unfocused windows seems to fail changing the mouse cursor with GLFW 3.2, but 3.3 works here.
        glfwSetCursor(glfw_interface.glfw_window, glfw_interface.glfw_mouse_cursors[imgui_cursor] ? glfw_interface.glfw_mouse_cursors[imgui_cursor] : glfw_interface.glfw_mouse_cursors[ImGuiMouseCursor_Arrow]);
        glfwSetInputMode(glfw_interface.glfw_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }

    ImGui::NewFrame();
}

void ImGuiRenderer::endFrame() {
    ImGui::Render();
    ImDrawData* draw_data = ImGui::GetDrawData();

    // update vertex and index buffers
    // because of the memory layout of imgui buffers, we have to do it manually
    if (draw_data->TotalVtxCount <= 0) {
        return;
    }

    createBuffers();
    updateBuffers();
}


RecordCommandsResult ImGuiRenderer::recordCommands(uint32_t swapchain_image, VkRenderPassBeginInfo& render_pass_info, const ImGuiProfileConfig& profile_config) {
    ImDrawData* draw_data = ImGui::GetDrawData();

    // we might need to combine multiple command buffers in one frame in the future
    std::vector<VkCommandBuffer> command_buffers = { vk_drawing_buffers_[swapchain_image] };
    vulkan_backend_->resetCommandBuffers(command_buffers);

    if (draw_data->TotalVtxCount <= 0) {
        return makeRecordCommandsResult(false, command_buffers);
    }

    int fb_width = (int)(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
    int fb_height = (int)(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
    if (fb_width <= 0 || fb_height <= 0) {
        return makeRecordCommandsResult(false, command_buffers);
    }

    VkCommandBufferInheritanceInfo inherit_info{};
    inherit_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
    inherit_info.renderPass = render_pass_info.renderPass;
    inherit_info.subpass = subpass_number_;
    inherit_info.framebuffer = render_pass_info.framebuffer;
    inherit_info.occlusionQueryEnable = VK_FALSE;
    inherit_info.queryFlags = 0;
    inherit_info.pipelineStatistics = 0;

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
    begin_info.pInheritanceInfo = &inherit_info;

    if (vkBeginCommandBuffer(command_buffers[0], &begin_info) != VK_SUCCESS) {
        std::cerr << "[IMGUI Renderer] Failed to begin recording command buffer!" << std::endl;
        return makeRecordCommandsResult(false, command_buffers);
    }

    // update push constants
    auto pc_iter = ui_pipeline_.push_constants.find(UI_TRANSFORM_PUSH_CONSTANT);
    if (pc_iter == ui_pipeline_.push_constants.end()) {
        return makeRecordCommandsResult(false, command_buffers);
    }

    auto pc = pc_iter->second;
    ui_transform_push_constant_.scale[0] = 2.0f / draw_data->DisplaySize.x;
    ui_transform_push_constant_.scale[1] = 2.0f / draw_data->DisplaySize.y;
    ui_transform_push_constant_.translate[0] = -1.0f - draw_data->DisplayPos.x * ui_transform_push_constant_.scale[0];
    ui_transform_push_constant_.translate[1] = -1.0f - draw_data->DisplayPos.y * ui_transform_push_constant_.scale[1];

    vkCmdPushConstants(command_buffers[0], ui_pipeline_.vk_pipeline_layout, pc.stageFlags, pc.offset, pc.size, &ui_transform_push_constant_);

    vkCmdBindPipeline(command_buffers[0], VK_PIPELINE_BIND_POINT_GRAPHICS, ui_pipeline_.vk_pipeline);
    vkCmdBindDescriptorSets(command_buffers[0], VK_PIPELINE_BIND_POINT_GRAPHICS, ui_pipeline_.vk_pipeline_layout, UI_UNIFORM_SET_ID, 1, &vk_descriptor_sets_[swapchain_image], 0, NULL);

    VkBuffer vertex_buffers[1] = { vertex_buffer_.vk_buffer };
    VkDeviceSize vertex_offset[1] = { 0 };
    vkCmdBindVertexBuffers(command_buffers[0], 0, 1, vertex_buffers, vertex_offset);
    vkCmdBindIndexBuffer(command_buffers[0], index_buffer_.vk_buffer, 0, sizeof(ImDrawIdx) == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);

    VkViewport viewport;
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = (float)fb_width;
    viewport.height = (float)fb_height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(command_buffers[0], 0, 1, &viewport);

    ImVec2 clip_off = draw_data->DisplayPos;         // (0,0) unless using multi-viewports
    ImVec2 clip_scale = draw_data->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

    if (profile_config.profile_draw) {
        vulkan_backend_->writeTimestampQuery(command_buffers[0], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, profile_config.start_query_num); // does nothing if not in debug
    }

    // Render command lists
    // (Because we merged all buffers into a single one, we maintain our own offset into them)
    int global_vtx_offset = 0;
    int global_idx_offset = 0;
    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
        {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];

            // Project scissor/clipping rectangles into framebuffer space
            ImVec4 clip_rect;
            clip_rect.x = (pcmd->ClipRect.x - clip_off.x) * clip_scale.x;
            clip_rect.y = (pcmd->ClipRect.y - clip_off.y) * clip_scale.y;
            clip_rect.z = (pcmd->ClipRect.z - clip_off.x) * clip_scale.x;
            clip_rect.w = (pcmd->ClipRect.w - clip_off.y) * clip_scale.y;

            if (clip_rect.x < fb_width && clip_rect.y < fb_height && clip_rect.z >= 0.0f && clip_rect.w >= 0.0f)
            {
                // Negative offsets are illegal for vkCmdSetScissor
                if (clip_rect.x < 0.0f)
                    clip_rect.x = 0.0f;
                if (clip_rect.y < 0.0f)
                    clip_rect.y = 0.0f;

                // Apply scissor/clipping rectangle
                VkRect2D scissor;
                scissor.offset.x = (int32_t)(clip_rect.x);
                scissor.offset.y = (int32_t)(clip_rect.y);
                scissor.extent.width = (uint32_t)(clip_rect.z - clip_rect.x);
                scissor.extent.height = (uint32_t)(clip_rect.w - clip_rect.y);
                vkCmdSetScissor(command_buffers[0], 0, 1, &scissor);
                vkCmdDrawIndexed(command_buffers[0], pcmd->ElemCount, 1, pcmd->IdxOffset + global_idx_offset, pcmd->VtxOffset + global_vtx_offset, 0);
            }
        }
        global_idx_offset += cmd_list->IdxBuffer.Size;
        global_vtx_offset += cmd_list->VtxBuffer.Size;
    }

    if (profile_config.profile_draw) {
        vulkan_backend_->writeTimestampQuery(command_buffers[0], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, profile_config.stop_query_num); // does nothing if not in debug
    }

    if (vkEndCommandBuffer(command_buffers[0]) != VK_SUCCESS) {
        std::cerr << "[IMGUI Renderer] Failed to record command buffer!" << std::endl;
        return makeRecordCommandsResult(false, command_buffers);
    }

    return makeRecordCommandsResult(true, command_buffers);
}

void ImGuiRenderer::InitImGui(GLFWWindowHandle window) {
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGui::StyleColorsDark();
    ImGui::GetIO().Fonts->AddFontDefault();

    glfw_interface.glfw_window = static_cast<GLFWwindow*>(window);

    glfw_interface.time = 0.0;

    // Setup back-end capabilities flags
    ImGuiIO& io = ImGui::GetIO();
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;         // We can honor GetMouseCursor() values (optional)
    io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;          // We can honor io.WantSetMousePos requests (optional, rarely used)
    io.BackendPlatformName = "imgui_impl_glfw";

    // Keyboard mapping. ImGui will use those indices to peek into the io.KeysDown[] array.
    io.KeyMap[ImGuiKey_Tab] = GLFW_KEY_TAB;
    io.KeyMap[ImGuiKey_LeftArrow] = GLFW_KEY_LEFT;
    io.KeyMap[ImGuiKey_RightArrow] = GLFW_KEY_RIGHT;
    io.KeyMap[ImGuiKey_UpArrow] = GLFW_KEY_UP;
    io.KeyMap[ImGuiKey_DownArrow] = GLFW_KEY_DOWN;
    io.KeyMap[ImGuiKey_PageUp] = GLFW_KEY_PAGE_UP;
    io.KeyMap[ImGuiKey_PageDown] = GLFW_KEY_PAGE_DOWN;
    io.KeyMap[ImGuiKey_Home] = GLFW_KEY_HOME;
    io.KeyMap[ImGuiKey_End] = GLFW_KEY_END;
    io.KeyMap[ImGuiKey_Insert] = GLFW_KEY_INSERT;
    io.KeyMap[ImGuiKey_Delete] = GLFW_KEY_DELETE;
    io.KeyMap[ImGuiKey_Backspace] = GLFW_KEY_BACKSPACE;
    io.KeyMap[ImGuiKey_Space] = GLFW_KEY_SPACE;
    io.KeyMap[ImGuiKey_Enter] = GLFW_KEY_ENTER;
    io.KeyMap[ImGuiKey_Escape] = GLFW_KEY_ESCAPE;
    io.KeyMap[ImGuiKey_KeyPadEnter] = GLFW_KEY_KP_ENTER;
    io.KeyMap[ImGuiKey_A] = GLFW_KEY_A;
    io.KeyMap[ImGuiKey_C] = GLFW_KEY_C;
    io.KeyMap[ImGuiKey_V] = GLFW_KEY_V;
    io.KeyMap[ImGuiKey_X] = GLFW_KEY_X;
    io.KeyMap[ImGuiKey_Y] = GLFW_KEY_Y;
    io.KeyMap[ImGuiKey_Z] = GLFW_KEY_Z;

    io.SetClipboardTextFn = SetClipboardText;
    io.GetClipboardTextFn = GetClipboardText;
    io.ClipboardUserData = glfw_interface.glfw_window;
#if defined(_WIN32)
    io.ImeWindowHandle = (void*)glfwGetWin32Window(glfw_interface.glfw_window);
#endif

    // scale up for high-dpi screens
    float x_scale, y_scale;
    glfwGetWindowContentScale(glfw_interface.glfw_window, &x_scale, &y_scale);
    high_dpi_scale_ = x_scale > y_scale ? x_scale : y_scale;
    io.FontGlobalScale *= high_dpi_scale_;

    // Create mouse cursors
    // (By design, on X11 cursors are user configurable and some cursors may be missing. When a cursor doesn't exist,
    // GLFW will emit an error which will often be printed by the app, so we temporarily disable error reporting.
    // Missing cursors will return NULL and our _UpdateMouseCursor() function will use the Arrow cursor instead.)
    GLFWerrorfun prev_error_callback = glfwSetErrorCallback(NULL);
    glfw_interface.glfw_mouse_cursors[ImGuiMouseCursor_Arrow] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
    glfw_interface.glfw_mouse_cursors[ImGuiMouseCursor_TextInput] = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
    glfw_interface.glfw_mouse_cursors[ImGuiMouseCursor_ResizeNS] = glfwCreateStandardCursor(GLFW_VRESIZE_CURSOR);
    glfw_interface.glfw_mouse_cursors[ImGuiMouseCursor_ResizeEW] = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);
    glfw_interface.glfw_mouse_cursors[ImGuiMouseCursor_Hand] = glfwCreateStandardCursor(GLFW_HAND_CURSOR);
#if GLFW_HAS_NEW_CURSORS
    glfw_interface.glfw_mouse_cursors[ImGuiMouseCursor_ResizeAll] = glfwCreateStandardCursor(GLFW_RESIZE_ALL_CURSOR);
    glfw_interface.glfw_mouse_cursors[ImGuiMouseCursor_ResizeNESW] = glfwCreateStandardCursor(GLFW_RESIZE_NESW_CURSOR);
    glfw_interface.glfw_mouse_cursors[ImGuiMouseCursor_ResizeNWSE] = glfwCreateStandardCursor(GLFW_RESIZE_NWSE_CURSOR);
    glfw_interface.glfw_mouse_cursors[ImGuiMouseCursor_NotAllowed] = glfwCreateStandardCursor(GLFW_NOT_ALLOWED_CURSOR);
#else
    glfw_interface.glfw_mouse_cursors[ImGuiMouseCursor_ResizeAll] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
    glfw_interface.glfw_mouse_cursors[ImGuiMouseCursor_ResizeNESW] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
    glfw_interface.glfw_mouse_cursors[ImGuiMouseCursor_ResizeNWSE] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
    glfw_interface.glfw_mouse_cursors[ImGuiMouseCursor_NotAllowed] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
#endif
    glfwSetErrorCallback(prev_error_callback);

    glfw_interface.imgui_callbacks.mouseButtonCallback = glfwSetMouseButtonCallback(glfw_interface.glfw_window, MouseButtonCallback);
    glfw_interface.imgui_callbacks.scrollCallback = glfwSetScrollCallback(glfw_interface.glfw_window, ScrollCallback);
    glfw_interface.imgui_callbacks.keyCallback = glfwSetKeyCallback(glfw_interface.glfw_window, KeyCallback);
    glfw_interface.imgui_callbacks.charCallback = glfwSetCharCallback(glfw_interface.glfw_window, CharCallback);
}

void ImGuiRenderer::InitVulkanAssets() {
    uploadFonts();

    imgui_vertex_shader_ = vulkan_backend_->createShaderModule("imgui_vertex");
    imgui_vertex_shader_->loadSpirvShader("shaders/imgui_vs.spv");

    if (!imgui_vertex_shader_->isVertexFormatCompatible(getVertexFormatInfo())) {
        std::cerr << "[IMGUI Renderer] Requested Vertex format is not compatible with pipeline input!" << std::endl;
        return;
    }

    imgui_fragment_shader_ = vulkan_backend_->createShaderModule("imgui_fragment");
    imgui_fragment_shader_->loadSpirvShader("shaders/imgui_fs.spv");

    if (!imgui_vertex_shader_->isValid() || !imgui_fragment_shader_->isValid()) {
        std::cerr << "[IMGUI Renderer] Failed to validate shaders!" << std::endl;
        return;
    }

    vk_drawing_buffers_ = vulkan_backend_->createSecondaryCommandBuffers(vulkan_backend_->getSwapChainSize());
    if (vk_drawing_buffers_.empty()) {
        std::cerr << "[IMGUI Renderer] Failed to create secondary command buffers!" << std::endl;
        return;
    }
}

void ImGuiRenderer::uploadFonts() {
    ImGuiIO& io = ImGui::GetIO();

    unsigned char* src_pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&src_pixels, &width, &height);
    IM_ASSERT(io.Fonts->Fonts[0]->IsLoaded());

    size_t upload_size = static_cast<size_t>(width) * height * 4 * sizeof(char);

    auto pixels = std::vector<unsigned char>(src_pixels, src_pixels + upload_size);

    fonts_texture_ = vulkan_backend_->createTexture("ui_fonts");
    fonts_texture_->loadImageRGBA(static_cast<uint32_t>(width),
                                  static_cast<uint32_t>(height),
                                  static_cast<uint32_t>(4),
                                  false,
                                  pixels);

   io.Fonts->TexID = (ImTextureID)(intptr_t)fonts_texture_->getImage();

   fonts_texture_->createSampler();
}

void ImGuiRenderer::createDescriptorSets(const std::map<uint32_t, VkDescriptorSetLayout>& descriptor_set_layouts) {
    const auto& layout = descriptor_set_layouts.find(UI_UNIFORM_SET_ID)->second;
    std::vector<VkDescriptorSetLayout> layouts(vulkan_backend_->getSwapChainSize(), layout);
    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = vulkan_backend_->getDescriptorPool();
    alloc_info.descriptorSetCount = vulkan_backend_->getSwapChainSize();
    alloc_info.pSetLayouts = layouts.data();

    std::vector<VkDescriptorSet> layout_descriptor_sets(vulkan_backend_->getSwapChainSize());
    if (vkAllocateDescriptorSets(vulkan_backend_->getDevice(), &alloc_info, layout_descriptor_sets.data()) != VK_SUCCESS) {
        std::cerr << "[IMGUI Renderer] Failed to allocate Mesh descriptor sets!" << std::endl;
        return;
    }

    vk_descriptor_sets_.clear();
    vk_descriptor_sets_ = std::move(layout_descriptor_sets);
}

void ImGuiRenderer::updateDescriptorSets(const DescriptorSetMetadata& metadata) {
    const auto& bindings = metadata.set_bindings.find(UI_UNIFORM_SET_ID)->second;
    fonts_texture_->updateDescriptorSets(vk_descriptor_sets_, bindings.find(UI_TEXTURE_SAMPLER_BINDING_NAME)->second);
}

void ImGuiRenderer::createBuffers() {
    ImDrawData* draw_data = ImGui::GetDrawData();
    if (draw_data->TotalVtxCount <= 0) {
        std::cerr << "[IMGUI Renderer] Call to ImGuiRenderer::createBuffers with empty Draw Data!" << std::endl;
        return;
    }

    if (vertex_buffer_.vk_buffer == VK_NULL_HANDLE) {
        max_vertex_count_ = draw_data->TotalVtxCount * 5; // leave enough room for 
        auto empty_vertex_buffer = std::vector<ImDrawVert>(max_vertex_count_, ImDrawVert());
        vertex_buffer_ = vulkan_backend_->createVertexBuffer<ImDrawVert>("imgui_vertex_buffer", empty_vertex_buffer, false);
    }

    if (index_buffer_.vk_buffer == VK_NULL_HANDLE) {
        max_index_count_ = draw_data->TotalIdxCount * 5;
        auto empty_index_buffer = std::vector<ImDrawIdx>(max_index_count_, ImDrawIdx());
        index_buffer_ = vulkan_backend_->createIndexBuffer<ImDrawIdx>("imgui_index_buffer", empty_index_buffer, false);
    }
}

void ImGuiRenderer::updateBuffers() {
    ImDrawData* draw_data = ImGui::GetDrawData();
    ImDrawVert* vtx_dst = nullptr;
    ImDrawIdx* idx_dst = nullptr;
    size_t vertex_size = draw_data->TotalVtxCount * sizeof(ImDrawVert);
    size_t index_size = draw_data->TotalIdxCount * sizeof(ImDrawIdx);

    VkResult result = vkMapMemory(vulkan_backend_->getDevice(), vertex_buffer_.vk_buffer_memory, 0, vertex_size, 0, (void**)(&vtx_dst));
    if (result != VK_SUCCESS) {
        std::cerr << "[IMGUI Renderer] Failed to map UI vertex buffer" << std::endl;
        return;
    }

    result = vkMapMemory(vulkan_backend_->getDevice(), index_buffer_.vk_buffer_memory, 0, index_size, 0, (void**)(&idx_dst));
    if (result != VK_SUCCESS) {
        vkUnmapMemory(vulkan_backend_->getDevice(), vertex_buffer_.vk_buffer_memory);
        std::cerr << "[IMGUI Renderer] Failed to map UI index buffer" << std::endl;
        return;
    }

    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        memcpy(vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
        vtx_dst += cmd_list->VtxBuffer.Size;
        idx_dst += cmd_list->IdxBuffer.Size;
    }

    VkMappedMemoryRange range[2] = {};
    range[0].sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    range[0].memory = vertex_buffer_.vk_buffer_memory;
    range[0].size = VK_WHOLE_SIZE;
    range[1].sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    range[1].memory = index_buffer_.vk_buffer_memory;
    range[1].size = VK_WHOLE_SIZE;
    result = vkFlushMappedMemoryRanges(vulkan_backend_->getDevice(), 2, range);
    if (result != VK_SUCCESS) {
        std::cerr << "[IMGUI Renderer] Failed to flush UI buffers" << std::endl;
    }

    vkUnmapMemory(vulkan_backend_->getDevice(), vertex_buffer_.vk_buffer_memory);
    vkUnmapMemory(vulkan_backend_->getDevice(), index_buffer_.vk_buffer_memory);
}
