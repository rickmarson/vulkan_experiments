echo off

set ROOT_PATH=%1

glslc %ROOT_PATH%/shaders/tutorial.vert -o %ROOT_PATH%/shaders/tutorial_vs.spv
glslc %ROOT_PATH%/shaders/tutorial.frag -o %ROOT_PATH%/shaders/tutorial_fs.spv
