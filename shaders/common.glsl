void worldToVulkan(inout mat4 world_coords) {
	mat4 vulkan_coords = mat4(
		vec4(0.0, 0.0, -1.0, 0.0),
		vec4(-1.0, 0.0, 0.0, 0.0),
		vec4(0.0, 1.0, 0.0, 0.0),
		vec4(0.0, 0.0, 0.0, 1.0)
	);

	world_coords = vulkan_coords * world_coords;
}

void projectionToVulkan(inout mat4 proj) {
	proj[1][1] *= -1.0;
}
