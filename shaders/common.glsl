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

mat3 objectLocalMatrix(in vec3 norm_world, in vec4 tan_world, in mat4 model_view) {
	// note: this is not strictly correct (should be transpose(inverse(model_view))),
	// but since we are not using non-uniform scaling it's an acceptable approximation
    mat3 normal_matrix = mat3(model_view);

	vec3 norm_view = normalize(normal_matrix * norm_world);
	vec3 tan_view = normalize(normal_matrix * tan_world.xyz);
	vec3 bin_view = normalize(cross(norm_view, tan_view)) * tan_world.w;

	return mat3(
		tan_view.x, bin_view.x, norm_view.x,
		tan_view.y, bin_view.y, norm_view.y,
		tan_view.z, bin_view.z, norm_view.z
	);
}