const float PI = 3.14159265359;

vec3 schlickFresnel(in float l_dot_h, in float metal, in vec3 diffuse_colour) {
	vec3 f0 = vec3(0.04) * (1.0 - metal) + diffuse_colour * metal;
	return f0 + (1.0 - f0) * pow(1.0 - l_dot_h, 5.0);
}

float geomSmith(in float n_dot_l, in float roughness) {
	float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
	float d = n_dot_l * (1 - k) + k;
	return 1.0 / d;
}

float ggxDistribution(in float n_dot_h, in float roughness) {
	float alpha2 = roughness * roughness * roughness * roughness;
	float d = (n_dot_h * n_dot_h) * (alpha2 - 1.0) + 1.0;
	return alpha2 / (PI * d * d);
}

vec3 brdf(in vec3 pos, 
		  in vec3 norm, 
		  in vec3 diffuse_colour, 
		  in float metalness,
		  in float roughness, 
		  in vec3 light_pos, 
		  in vec3 light_intensity) {	
	vec3 l = light_pos - pos;
	float dist = length(l);
	l = normalize(l);
	light_intensity /= (dist * dist);
	
	vec3 v = normalize(-pos);
	vec3 h = normalize(v + l);
	float n_dot_h = dot(norm, h);
	float l_dot_h = dot(l, h);
	float n_dot_l = max(dot(norm, l), 0.0);
	float n_dot_v = dot(norm, v);

	vec3 spec_brdf = 0.25 * ggxDistribution(n_dot_h, roughness) * 
		schlickFresnel(l_dot_h, metalness, diffuse_colour) * 
		geomSmith(n_dot_l, roughness) * 
		geomSmith(n_dot_v, roughness);

	vec3 diffuse_brdf = diffuse_colour * (1.0 - metalness);

	return (diffuse_brdf + PI * spec_brdf) * light_intensity * n_dot_l;
}
