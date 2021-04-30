#!/bin/sh

ROOT_PATH="$1"

glslc $ROOT_PATH/shaders/imgui.vert -o $ROOT_PATH/shaders/imgui_vs.spv
glslc $ROOT_PATH/shaders/imgui.frag -o $ROOT_PATH/shaders/imgui_fs.spv

glslc $ROOT_PATH/shaders/shadow_map.vert -o $ROOT_PATH/shaders/shadow_map_vs.spv

glslc $ROOT_PATH/shaders/model_viewer.vert -o $ROOT_PATH/shaders/model_viewer_vs.spv
glslc $ROOT_PATH/shaders/model_viewer.frag -o $ROOT_PATH/shaders/model_viewer_fs.spv

glslc $ROOT_PATH/shaders/alley.vert -o $ROOT_PATH/shaders/alley_vs.spv
glslc $ROOT_PATH/shaders/alley.frag -o $ROOT_PATH/shaders/alley_fs.spv
glslc $ROOT_PATH/shaders/rain_drops_geom.vert -o $ROOT_PATH/shaders/rain_drops_geom_vs.spv
glslc $ROOT_PATH/shaders/rain_drops_geom.geom -o $ROOT_PATH/shaders/rain_drops_geom_gm.spv
glslc $ROOT_PATH/shaders/rain_drops_vb.vert -o $ROOT_PATH/shaders/rain_drops_vb_vs.spv
glslc $ROOT_PATH/shaders/rain_drops.frag -o $ROOT_PATH/shaders/rain_drops_fs.spv
glslc $ROOT_PATH/shaders/rainfall.comp -o $ROOT_PATH/shaders/rainfall_cp.spv
