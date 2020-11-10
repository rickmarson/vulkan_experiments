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

vec3 calcLightIntensity(in mat4 model_view, 
						in vec3 vertex_pos, 
						in vec3 vertex_norm, 
						in vec3 light_pos, 
						in vec3 light_colour, 
						in vec3 Kd) {
	// note: this is not strictly correct (should be transpose(inverse(model_view))),
	// but since we are not using non-uniform scaling it's an acceptable approximation
	vec3 tnorm = normalize(mat3(model_view) * vertex_norm);
	vec4 vertex_view = model_view * vec4(vertex_pos, 1.0);
	vec4 light_view = model_view * vec4(light_pos, 1.0);
	vec3 s = normalize(light_view.xyz - vertex_view.xyz);

	return light_colour * Kd * max( dot(s, tnorm), 0.0 );
} 