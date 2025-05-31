// shader_atlas.glsl

// ---------------------------------------------
// Vertex Shaders
// ---------------------------------------------

// basic.vs
// Used in multiple passes: G-Buffer fill, phong forward, light volumes
// A1: Used for rendering geometry with proper model/viewprojection transformations
// Example compiled shaders list (for reference)
flat basic.vs flat.fs
texture basic.vs texture.fs
skybox basic.vs skybox.fs
depth quad.vs depth.fs
multi basic.vs multi.fs
compute test.cs

// ASSIGNMENT2: TASK 2 & 3
phong basic.vs phong.fs

// ASSIGNMENT3: TASK 3.1
plain basic.vs plain.fs

// G-BUFFER FILL SHADER
gbuffer_fill basic.vs gbuffer_fill.fs

// DEFERRED LIGHTING SHADER (used for directional lights with quad.vs, and point/spot with basic.vs)
deferred_lighting quad.vs deferred_lighting.fs

// NEW SHADER PROGRAM FOR POINT/SPOT LIGHT VOLUMES
light_volume_deferred basic.vs deferred_lighting.fs

// A5: TASK 2.2 - Add PBR G-Buffer and deferred lighting shaders
pbr_gbuffer_fill basic.vs pbr_gbuffer_fill.fs
pbr_deferred_lighting quad.vs pbr_deferred_lighting.fs

\test.cs
#version 430 core
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() 
{
	vec4 i = vec4(0.0);
}

\basic.vs
#version 330 core
in vec3 a_vertex;
in vec3 a_normal;
in vec2 a_coord;
in vec4 a_color;

uniform vec3 u_camera_pos;

uniform mat4 u_model;
uniform mat4 u_viewprojection;

out vec3 v_position;
out vec3 v_world_position;
out vec3 v_normal;
out vec2 v_uv;
out vec4 v_color;

uniform float u_time;

void main()
{	
	v_normal = (u_model * vec4(a_normal, 0.0)).xyz;
	v_position = a_vertex;
	v_world_position = (u_model * vec4(v_position, 1.0)).xyz;
	v_color = a_color;
	v_uv = a_coord;
	gl_Position = u_viewprojection * vec4(v_world_position, 1.0);
}

\quad.vs
#version 330 core
in vec3 a_vertex;
in vec2 a_coord;
out vec2 v_uv;

void main()
{	
	v_uv = a_coord;
	gl_Position = vec4(a_vertex, 1.0);
}

\texture.fs
#version 330 core
in vec3 v_position;
in vec3 v_world_position;
in vec3 v_normal;
in vec2 v_uv;
in vec4 v_color;

uniform vec4 u_color;
uniform sampler2D u_texture;
uniform float u_time;
uniform float u_alpha_cutoff;

out vec4 FragColor;

void main()
{
	vec4 color = u_color * texture(u_texture, v_uv);
	if(color.a < u_alpha_cutoff)
		discard;
	FragColor = color;
}

\flat.fs
#version 330 core
uniform vec4 u_color;
out vec4 FragColor;

void main() {
	FragColor = u_color;
}

\plain.fs
#version 330 core
out vec4 FragColor;

void main()
{
	FragColor = vec4(0.0, 0.0, 0.0, 1.0);
}

\skybox.fs
#version 330 core
in vec3 v_world_position;

uniform samplerCube u_texture;
uniform vec3 u_camera_position;

layout(location = 0) out vec4 gbuffer_albedo;
layout(location = 1) out vec4 gbuffer_normal;

void main()
{
	vec3 E = v_world_position - u_camera_position;
	vec4 sky_color = texture(u_texture, E);
	gbuffer_albedo = sky_color;
	gbuffer_normal = vec4(0.0, 0.0, 0.0, 0.0); 
}

\multi.fs
#version 330 core
in vec3 v_position;
in vec3 v_world_position;
in vec3 v_normal;
in vec2 v_uv;

uniform vec4 u_color;
uniform sampler2D u_texture;
uniform float u_time;
uniform float u_alpha_cutoff;

layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec4 NormalColor;

void main()
{
	vec4 color = u_color * texture(u_texture, v_uv);
	if(color.a < u_alpha_cutoff)
		discard;
	vec3 N = normalize(v_normal);

	FragColor = color;
	NormalColor = vec4(N, 1.0);
}

\depth.fs
#version 330 core
uniform vec2 u_camera_nearfar;
uniform sampler2D u_texture;
in vec2 v_uv;
out vec4 FragColor;

void main()
{
	float n = u_camera_nearfar.x;
	float f = u_camera_nearfar.y;
	float z = texture(u_texture, v_uv).x;
	if(n == 0.0 && f == 1.0)
		FragColor = vec4(z);
	else
		FragColor = vec4(n * (z + 1.0) / (f + n - z * (f - n)));
}


\instanced.vs
#version 330 core
in vec3 a_vertex;
in vec3 a_normal;
in vec2 a_coord;
in mat4 u_model;

uniform vec3 u_camera_pos;
uniform mat4 u_viewprojection;

out vec3 v_position;
out vec3 v_world_position;
out vec3 v_normal;
out vec2 v_uv;

void main()
{	
	v_normal = (u_model * vec4(a_normal, 0.0)).xyz;
	v_position = a_vertex;
	v_world_position = (u_model * vec4(a_vertex, 1.0)).xyz;
	v_uv = a_coord;
	gl_Position = u_viewprojection * vec4(v_world_position, 1.0);
}


\phong.fs
#version 330 core

in vec3 v_position;
in vec3 v_world_position;
in vec3 v_normal;
in vec2 v_uv;
in vec4 v_color;

uniform vec4 u_color;
uniform sampler2D u_texture;
uniform float u_time;
uniform float u_alpha_cutoff;

uniform int u_num_lights;
uniform int u_light_type[10];
uniform vec3 u_light_positions[10];
uniform vec3 u_light_colors[10];
uniform float u_light_intensity[10];
uniform vec3 u_light_direction[10];

uniform vec3 u_camera_position;

// Shadow map uniforms
uniform sampler2D u_shadow_map[10];
uniform mat4 u_shadow_vp[10];
uniform float u_shadow_bias;

out vec4 FragColor;

void main()
{
	vec4 tex_color = texture(u_texture, v_uv);
	vec4 color = u_color * tex_color;

	if (color.a < u_alpha_cutoff)
		discard;

	vec3 N = normalize(v_normal);
	vec3 V = normalize(u_camera_position - v_world_position);

	vec3 final_color = vec3(0.0);

	for (int i = 0; i < u_num_lights; ++i)
	{
		vec3 L;
		float attenuation = 1.0;

		if (u_light_type[i] == 1) { // POINT
			L = normalize(u_light_positions[i] - v_world_position);
			float dist = length(u_light_positions[i] - v_world_position);
			attenuation = 1.0 / (dist*dist);
		}
		else if (u_light_type[i] == 3) { // DIRECTIONAL
			L = normalize(u_light_direction[i]);
			attenuation = 1.0;
		}
		else if (u_light_type[i] == 2) { // SPOTLIGHT
			vec3 light_to_frag = normalize(v_world_position - u_light_positions[i]);
			float cos_angle = dot(-light_to_frag, normalize(u_light_direction[i]));
			float inner = 0.9;
			float outer = 0.7;
			float spot_smooth = clamp((cos_angle - outer) / (inner - outer), 0.0, 1.0);

			L = normalize(u_light_positions[i] - v_world_position);
			float dist = length(u_light_positions[i] - v_world_position);
			attenuation = spot_smooth / (dist*dist);
		}

		float NdotL = max(dot(N, L), 0.0);

		vec4 shadow_coord = u_shadow_vp[i] * vec4(v_world_position, 1.0);
		shadow_coord.xyz /= shadow_coord.w;
		shadow_coord.xyz = shadow_coord.xyz * 0.5 + 0.5;

		bool in_shadow = false;

		if (shadow_coord.x >= 0.0 && shadow_coord.x <= 1.0 &&
			shadow_coord.y >= 0.0 && shadow_coord.y <= 1.0)
		{
			float shadow_map_depth = texture(u_shadow_map[i], shadow_coord.xy).r;
			float current_depth = shadow_coord.z - u_shadow_bias;
			in_shadow = current_depth > shadow_map_depth;
		}

		if (!in_shadow) {
			vec3 light_contrib = u_light_intensity[i] * u_light_colors[i] * NdotL;
			final_color += light_contrib * attenuation;
		}
	}
	FragColor = vec4(final_color * color.rgb, color.a);
}


\gbuffer_fill.fs
#version 330 core

in vec3 v_world_position;
in vec3 v_normal;
in vec2 v_uv;

uniform vec4 u_color;
uniform sampler2D u_texture;
uniform float u_alpha_cutoff;

layout(location = 0) out vec4 out_gbuffer_albedo;
layout(location = 1) out vec4 out_gbuffer_normal;

void main()
{
	vec4 tex_color = texture(u_texture, v_uv);
	vec4 diffuse_albedo = u_color * tex_color;

	if (diffuse_albedo.a < u_alpha_cutoff)
		discard;

	out_gbuffer_albedo = diffuse_albedo;
	out_gbuffer_normal = vec4(normalize(v_normal), 1.0);
}

\pbr_gbuffer_fill.fs
#version 330 core

in vec3 v_world_position;
in vec3 v_normal;
in vec2 v_uv;

uniform sampler2D u_albedo_texture;
uniform sampler2D u_metallic_roughness_texture;

uniform vec4 u_color;
uniform float u_alpha_cutoff;

layout(location = 0) out vec4 out_gbuffer_albedo;
layout(location = 1) out vec4 out_gbuffer_normal;

void main()
{
	vec4 albedo_sample = texture(u_albedo_texture, v_uv);
	vec4 mr_sample = texture(u_metallic_roughness_texture, v_uv);

	vec4 final_color = u_color * albedo_sample;

	if(final_color.a < u_alpha_cutoff)
		discard;

	out_gbuffer_albedo = vec4(final_color.rgb, mr_sample.r);
	out_gbuffer_normal = vec4(normalize(v_normal), 1.0);

	// Note: Roughness, metallic, and AO can be packed in other targets or alpha channels if needed
}

\deferred_lighting.fs
#version 330 core

// Input G-Buffer textures
uniform sampler2D u_albedo_texture;
uniform sampler2D u_normal_material_texture;
uniform sampler2D u_depth_texture;

// Camera & screen info
uniform mat4 u_inverse_projection_matrix;
uniform mat4 u_inverse_view_matrix;
uniform vec3 u_camera_position;
uniform vec2 u_inv_screen_size;

// Lights
uniform int u_num_lights;
uniform int u_light_type[10];            // 1 = point, 2 = spot, 3 = directional
uniform vec3 u_light_positions[10];
uniform vec3 u_light_colors[10];
uniform float u_light_intensity[10];
uniform vec3 u_light_direction[10];

// Shadow maps and matrices
uniform sampler2D u_shadow_map[10];
uniform mat4 u_shadow_vp[10];
uniform float u_shadow_bias;

out vec4 FragColor;

const float PI = 3.14159265359;

// Simple Lambertian diffuse term
float computeDiffuse(vec3 N, vec3 L) {
    return max(dot(N, L), 0.0);
}

void main()
{
    // Compute UV coordinates from fragment coords
    vec2 uv = gl_FragCoord.xy * u_inv_screen_size;

    // Sample G-Buffer
    vec4 albedo = texture(u_albedo_texture, uv);
    vec4 normal_material = texture(u_normal_material_texture, uv);
    float depth = texture(u_depth_texture, uv).r;

    // Discard if no geometry (depth at far plane)
    if(depth >= 0.9999)
        discard;

    // Reconstruct world position from depth
    float z = depth * 2.0 - 1.0; // Convert depth to NDC space [-1,1]
    vec2 xy = uv * 2.0 - 1.0;    // NDC XY coords
    vec4 clip_space_pos = vec4(xy, z, 1.0);
    vec4 view_space_pos = u_inverse_projection_matrix * clip_space_pos;
    view_space_pos /= view_space_pos.w;
    vec4 world_space_pos = u_inverse_view_matrix * view_space_pos;
    vec3 pos = world_space_pos.xyz;

    // Get normal and material data
    vec3 N = normalize(normal_material.xyz);
    float roughness = clamp(normal_material.g, 0.05, 1.0);
    float metallic = clamp(normal_material.b, 0.0, 1.0);
    float ao = normal_material.r;

    // View direction
    vec3 V = normalize(u_camera_position - pos);

    // Base reflectance for non-metal
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo.rgb, metallic);

    vec3 Lo = vec3(0.0); // Accumulate lighting

    // Loop over lights
    for(int i = 0; i < u_num_lights; i++) {
        vec3 L;
        float attenuation = 1.0;

        if(u_light_type[i] == 1) { // Point light
            L = normalize(u_light_positions[i] - pos);
            float dist = length(u_light_positions[i] - pos);
            attenuation = 1.0 / (dist * dist);
        }
        else if(u_light_type[i] == 3) { // Directional light
            L = normalize(u_light_direction[i]);
            attenuation = 1.0;
        }
        else if(u_light_type[i] == 2) { // Spot light
            vec3 light_to_frag = normalize(pos - u_light_positions[i]);
            float cos_angle = dot(-light_to_frag, normalize(u_light_direction[i]));
            float inner = 0.9;
            float outer = 0.7;
            float spot_smooth = clamp((cos_angle - outer) / (inner - outer), 0.0, 1.0);
            L = normalize(u_light_positions[i] - pos);
            float dist = length(u_light_positions[i] - pos);
            attenuation = spot_smooth / (dist * dist);
        }

        float NdotL = computeDiffuse(N, L);
        if(NdotL <= 0.0)
            continue;

        // Shadow calculation
        vec4 shadowCoord = u_shadow_vp[i] * vec4(pos, 1.0);
        shadowCoord.xyz /= shadowCoord.w;
        shadowCoord.xyz = shadowCoord.xyz * 0.5 + 0.5;
        float shadow = 1.0;

        bool in_shadow = false;
        if(shadowCoord.x >= 0.0 && shadowCoord.x <= 1.0 &&
           shadowCoord.y >= 0.0 && shadowCoord.y <= 1.0) {
            float closestDepth = texture(u_shadow_map[i], shadowCoord.xy).r;
            float currentDepth = shadowCoord.z - u_shadow_bias;
            if(currentDepth > closestDepth)
                in_shadow = true;
        }
        if(in_shadow)
            shadow = 0.0;

        vec3 radiance = u_light_colors[i] * u_light_intensity[i] * attenuation;

        // Simple diffuse lighting (Lambertian)
        Lo += radiance * albedo.rgb * NdotL * shadow * ao;
    }

    // Ambient term
    vec3 ambient = vec3(0.03) * albedo.rgb * ao;

    vec3 color = ambient + Lo;

    // Gamma correction
    color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, albedo.a);
}


\pbr_deferred_lighting.fs
#version 330 core

uniform sampler2D u_albedo_texture;
uniform sampler2D u_normal_material_texture;
uniform sampler2D u_metallic_roughness_texture;
uniform sampler2D u_depth_texture;

uniform mat4 u_inverse_projection_matrix;
uniform mat4 u_inverse_view_matrix;
uniform vec3 u_camera_position;
uniform vec2 u_inv_screen_size;

uniform int u_num_lights;
uniform int u_light_type[10];
uniform vec3 u_light_positions[10];
uniform vec3 u_light_colors[10];
uniform float u_light_intensity[10];
uniform vec3 u_light_direction[10];

uniform sampler2D u_shadow_map[10];
uniform mat4 u_shadow_vp[10];
uniform float u_shadow_bias;

out vec4 FragColor;

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH = max(dot(N, H), 0.0);
	float NdotH2 = NdotH * NdotH;
	float denom = (NdotH2 * (a2 - 1.0) + 1.0);
	denom = PI * denom * denom;
	return a2 / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
	float r = (roughness + 1.0);
	float k = (r * r) / 8.0;
	float denom = NdotV * (1.0 - k) + k;
	return NdotV / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
	float NdotV = max(dot(N, V), 0.0);
	float NdotL = max(dot(N, L), 0.0);
	float ggx1 = GeometrySchlickGGX(NdotV, roughness);
	float ggx2 = GeometrySchlickGGX(NdotL, roughness);
	return ggx1 * ggx2;
}

vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
	return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

void main()
{
	vec2 uv = gl_FragCoord.xy * u_inv_screen_size;

	vec4 albedo_sample = texture(u_albedo_texture, uv);
	vec4 normal_sample = texture(u_normal_material_texture, uv);
	vec4 mr_sample = texture(u_metallic_roughness_texture, uv);

	vec3 N = normalize(normal_sample.xyz);
	float roughness = clamp(mr_sample.g, 0.05, 1.0);
	float metallic = clamp(mr_sample.b, 0.0, 1.0);
	float ao = mr_sample.r;

	float depth = texture(u_depth_texture, uv).r;
	if (length(N) < 0.001 || depth >= 0.9999)
		discard;

	float depth_clip = depth * 2.0 - 1.0;
	vec2 uv_clip = uv * 2.0 - 1.0;
	vec4 clip_pos = vec4(uv_clip, depth_clip, 1.0);

	vec4 view_pos = u_inverse_projection_matrix * clip_pos;
	view_pos /= view_pos.w;
	vec4 world_pos = u_inverse_view_matrix * view_pos;
	vec3 pos = world_pos.xyz;

	vec3 V = normalize(u_camera_position - pos);

	vec3 F0 = vec3(0.04);
	F0 = mix(F0, albedo_sample.rgb, metallic);

	vec3 Lo = vec3(0.0);

	for (int i = 0; i < u_num_lights; ++i)
	{
		vec3 L;
		float attenuation = 1.0;

		if (u_light_type[i] == 1)
		{
			L = normalize(u_light_positions[i] - pos);
			float dist = length(u_light_positions[i] - pos);
			attenuation = 1.0 / (dist * dist);
		}
		else if (u_light_type[i] == 3)
		{
			L = normalize(u_light_direction[i]);
			attenuation = 1.0;
		}
		else if (u_light_type[i] == 2)
		{
			vec3 light_to_frag = normalize(pos - u_light_positions[i]);
			float cos_angle = dot(-light_to_frag, normalize(u_light_direction[i]));
			float inner = 0.9;
			float outer = 0.7;
			float spot_smooth = clamp((cos_angle - outer) / (inner - outer), 0.0, 1.0);

			L = normalize(u_light_positions[i] - pos);
			float dist = length(u_light_positions[i] - pos);
			attenuation = spot_smooth / (dist * dist);
		}

		vec3 H = normalize(V + L);
		float NdotL = max(dot(N, L), 0.0);
		float NdotV = max(dot(N, V), 0.0);
		float NdotH = max(dot(N, H), 0.0);
		float VdotH = max(dot(V, H), 0.0);

		float D = DistributionGGX(N, H, roughness);
		float G = GeometrySmith(N, V, L, roughness);
		vec3 F = FresnelSchlick(VdotH, F0);

		vec3 numerator = D * G * F;
		float denominator = 4.0 * NdotV * NdotL + 0.001;
		vec3 specular = numerator / denominator;

		vec3 kS = F;
		vec3 kD = vec3(1.0) - kS;
		kD *= (1.0 - metallic);

		vec3 diffuse = kD * albedo_sample.rgb / PI;

		float shadow = 1.0;
		vec4 shadowCoord = u_shadow_vp[i] * vec4(pos, 1.0);
		shadowCoord.xyz /= shadowCoord.w;
		shadowCoord.xyz = shadowCoord.xyz * 0.5 + 0.5;

		if (shadowCoord.x >= 0.0 && shadowCoord.x <= 1.0 &&
			shadowCoord.y >= 0.0 && shadowCoord.y <= 1.0 &&
			shadowCoord.z >= 0.0 && shadowCoord.z <= 1.0)
		{
			float closestDepth = texture(u_shadow_map[i], shadowCoord.xy).r;
			float currentDepth = shadowCoord.z - u_shadow_bias;
			if (currentDepth > closestDepth)
				shadow = 0.0;
		}

		vec3 radiance = u_light_colors[i] * u_light_intensity[i] * attenuation;

		Lo += (diffuse + specular) * radiance * NdotL * shadow * ao;
	}

	vec3 ambient = vec3(0.03) * albedo_sample.rgb * ao;

	vec3 color = ambient + Lo;

	// Tone mapping and gamma correction
	color = color / (color + vec3(1.0));
	color = pow(color, vec3(1.0 / 2.2));

	FragColor = vec4(color, albedo_sample.a);
}