echo off

set ROOT_PATH=%1

glslc %ROOT_PATH%/shaders/imgui.vert -o %ROOT_PATH%/shaders/imgui_vs.spv
glslc %ROOT_PATH%/shaders/imgui.frag -o %ROOT_PATH%/shaders/imgui_fs.spv

glslc %ROOT_PATH%/shaders/model_viewer.vert -o %ROOT_PATH%/shaders/model_viewer_vs.spv
glslc %ROOT_PATH%/shaders/model_viewer.frag -o %ROOT_PATH%/shaders/model_viewer_fs.spv
