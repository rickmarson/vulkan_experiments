/*
* extensions.hpp
*
* Copyright (C) 2021 Riccardo Marson
*/

#pragma once

#include <vulkan/vulkan.h>
#include <iostream>


extern PFN_vkCmdDrawMeshTasksNV VkDrawMeshTasksNV;


void loadOptionalVkExtensions(VkInstance instance);
