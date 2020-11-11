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

bool worldToScreen(in mat4 proj, in mat4 view, in vec2 framebuffer_size, in vec3 world_pos, out vec3 screen_pos) {
	worldToVulkan(view);
    projectionToVulkan(proj);

    vec4 clip_pos = proj * view * vec4(world_pos, 1.0);
	if (clip_pos.w != 0.0) {
		screen_pos = clip_pos.xyz / clip_pos.w;
		screen_pos.xy = (screen_pos.xy + 1.0) * 0.5 * framebuffer_size;
		return true;
	}
	return false;
}
