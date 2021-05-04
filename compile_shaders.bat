echo off

set ROOT_PATH=%1

glslc %ROOT_PATH%/shaders/imgui.vert -o %ROOT_PATH%/shaders/imgui_vs.spv
glslc %ROOT_PATH%/shaders/imgui.frag -o %ROOT_PATH%/shaders/imgui_fs.spv

glslc %ROOT_PATH%/shaders/shadow_map.vert -o %ROOT_PATH%/shaders/shadow_map_vs.spv

glslc %ROOT_PATH%/shaders/model_viewer.vert -o %ROOT_PATH%/shaders/model_viewer_vs.spv
glslc %ROOT_PATH%/shaders/model_viewer.frag -o %ROOT_PATH%/shaders/model_viewer_fs.spv

glslc %ROOT_PATH%/shaders/alley.vert -o %ROOT_PATH%/shaders/alley_vs.spv
glslc %ROOT_PATH%/shaders/alley.frag -o %ROOT_PATH%/shaders/alley_fs.spv
glslc %ROOT_PATH%/shaders/rain_drops_geom.vert -o %ROOT_PATH%/shaders/rain_drops_geom_vs.spv
glslc %ROOT_PATH%/shaders/rain_drops_geom.geom -o %ROOT_PATH%/shaders/rain_drops_geom_gm.spv
glslc %ROOT_PATH%/shaders/rain_drops_pr.vert -o %ROOT_PATH%/shaders/rain_drops_pr_vs.spv
glslc %ROOT_PATH%/shaders/rain_drops_geom.frag -o %ROOT_PATH%/shaders/rain_drops_geom_fs.spv
glslc %ROOT_PATH%/shaders/rain_drops_pr.frag -o %ROOT_PATH%/shaders/rain_drops_pr_fs.spv
glslc %ROOT_PATH%/shaders/rainfall_geom.comp -o %ROOT_PATH%/shaders/rainfall_geom_cp.spv
glslc %ROOT_PATH%/shaders/rainfall_pr.comp -o %ROOT_PATH%/shaders/rainfall_pr_cp.spv
