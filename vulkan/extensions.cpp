/*
* extensions.cpp
*
* Copyright (C) 2021 Riccardo Marson
*/

#include "extensions.hpp"


PFN_vkCmdDrawMeshTasksNV VkDrawMeshTasksNV = nullptr;


void loadOptionalVkExtensions(VkInstance instance) {
    PFN_vkVoidFunction fn = vkGetInstanceProcAddr(instance, "vkCmdDrawMeshTasksNV");
    if (fn) {
        VkDrawMeshTasksNV = reinterpret_cast<PFN_vkCmdDrawMeshTasksNV>(fn);
    }
}
