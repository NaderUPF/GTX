//example of some shaders compiled
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
	v_normal = (u_model * vec4( a_normal, 0.0) ).xyz;
	v_position = a_vertex;
	v_world_position = (u_model * vec4( v_position, 1.0) ).xyz;
	v_color = a_color;
	v_uv = a_coord;
	gl_Position = u_viewprojection * vec4( v_world_position, 1.0 );
}


\quad.vs
#version 330 core
in vec3 a_vertex;
in vec2 a_coord;
out vec2 v_uv;
void main()
{	
	v_uv = a_coord;
	gl_Position = vec4( a_vertex, 1.0 );
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
	vec2 uv = v_uv;
	vec4 color = u_color;
	color *= texture( u_texture, v_uv );

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

// Outputs to G-Buffer
layout(location = 0) out vec4 gbuffer_albedo;
layout(location = 1) out vec4 gbuffer_normal;

void main()
{
	vec3 E = v_world_position - u_camera_position;
	vec4 sky_color = texture(u_texture, E);
	
	gbuffer_albedo = sky_color;
	// For skybox, normals are not standard. Output a placeholder or a view-dependent value if needed.
	// Here, outputting (0,0,0,0) as a convention for "no real normal data / sky".
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
	vec2 uv = v_uv;
	vec4 color = u_color;
	color *= texture(u_texture, uv);

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
	float z = texture2D(u_texture, v_uv).x;
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

// A3: TASK 3.3 - Shadow map uniforms
uniform sampler2D u_shadow_map[10];  // A3: TASK 7 - Multi-shadow map support
uniform mat4 u_shadow_vp[10];        // A3: TASK 7 - Light ViewProjection matrix per light
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

	// Ambient lighting
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

		// A3: TASK 3.3 - Shadow coordinate computation
		vec4 shadow_coord = u_shadow_vp[i] * vec4(v_world_position, 1.0);
		shadow_coord.xyz /= shadow_coord.w;
		shadow_coord.xyz = shadow_coord.xyz * 0.5 + 0.5; // from NDC [-1,1] to [0,1]

		bool in_shadow = false;

		// A3: TASK 3.4.1 - Depth comparison with shadow map
		if (shadow_coord.x >= 0.0 && shadow_coord.x <= 1.0 &&
			shadow_coord.y >= 0.0 && shadow_coord.y <= 1.0)
		{
			float shadow_map_depth = texture(u_shadow_map[i], shadow_coord.xy).r;
			float current_depth = shadow_coord.z - u_shadow_bias;
			in_shadow = current_depth > shadow_map_depth;
		}

		// A3: TASK 3.3 - Apply light contribution only if not in shadow
		if (!in_shadow) {
			vec3 light_contrib = u_light_intensity[i] * u_light_colors[i] * NdotL;
			final_color += light_contrib * attenuation;
		}
	}
	FragColor = vec4(final_color * color.rgb, color.a);
}

// NEW SHADER FOR G-BUFFER FILLING
\gbuffer_fill.fs
#version 330 core

in vec3 v_world_position; // From basic.vs
in vec3 v_normal;         // From basic.vs (world space normal)
in vec2 v_uv;             // From basic.vs
// in vec4 v_color;       // Vertex color, usually not used if material u_color is present

uniform vec4 u_color;     // Material color
uniform sampler2D u_texture;
uniform float u_alpha_cutoff;

// Outputs to G-Buffer
layout(location = 0) out vec4 out_gbuffer_albedo;  // Renamed to avoid conflict with potential future 'gbuffer_albedo' uniform
layout(location = 1) out vec4 out_gbuffer_normal;  // Renamed for clarity

void main()
{
    vec4 tex_color = texture(u_texture, v_uv);
    // Standard approach: modulate material color with texture color
    vec4 diffuse_albedo = u_color * tex_color;

    if (diffuse_albedo.a < u_alpha_cutoff)
        discard;

    out_gbuffer_albedo = diffuse_albedo;
    
    // Store world normal. The w component can be used for other material properties later (e.g., shininess, material ID)
    // For now, just storing normalized normal and 1.0 for w.
    out_gbuffer_normal = vec4(normalize(v_normal), 1.0); 
}

\deferred_lighting.fs
#version 330 core

// G-Buffer Textures
uniform sampler2D u_albedo_texture;    
uniform sampler2D u_normal_material_texture; 
uniform sampler2D u_depth_texture;     

// Camera Uniforms
uniform mat4 u_inverse_projection_matrix;
uniform mat4 u_inverse_view_matrix;
uniform vec3 u_camera_position; 
uniform vec2 u_inv_screen_size; // ADDED: e.g. (1.0/width, 1.0/height)

// Light Uniforms (EXPECTING u_num_lights = 1 for each pass type)
uniform int u_num_lights; 
uniform int u_light_type[1];      
uniform vec3 u_light_positions[1];
uniform vec3 u_light_colors[1];
uniform float u_light_intensity[1];
uniform vec3 u_light_direction[1]; 

// Shadow Mapping Uniforms (for the single point/spot/directional light)
uniform sampler2D u_shadow_map[1]; 
uniform mat4 u_shadow_vp[1];       
uniform float u_shadow_bias;

out vec4 FragColor; // This will be additively blended

void main()
{
    vec2 screen_uv = gl_FragCoord.xy * u_inv_screen_size; // ADDED

    // Sample G-Buffer using screen_uv
    vec4 albedo_gbuffer = texture(u_albedo_texture, screen_uv);
    vec3 albedo_color = albedo_gbuffer.rgb;

    vec4 normal_material_gbuffer = texture(u_normal_material_texture, screen_uv);
    vec3 world_normal = normalize(normal_material_gbuffer.xyz);
    // float material_shininess_factor = normal_material_gbuffer.w; 

    float depth = texture(u_depth_texture, screen_uv).r; // Get depth using screen_uv

    // IMPORTANT: Check for skybox pixels and discard if so, to avoid lighting the sky
    // (Skybox might write 0 to normal, or depth might be max)
    if (length(world_normal) < 0.001 || depth >= 0.99999) { // Used 'depth' variable
        discard; // Do not light skybox or far plane background
    }
    
    // Reconstruct World Position from Depth
    float depth_clip = depth * 2.0 - 1.0;
    vec2 uv_clip = screen_uv * 2.0 - 1.0; 
    vec4 clip_coords = vec4(uv_clip.x, uv_clip.y, depth_clip, 1.0);

    vec4 view_space_pos_h = u_inverse_projection_matrix * clip_coords;
    vec4 world_pos_h = u_inverse_view_matrix * view_space_pos_h;
    vec3 v_world_position = world_pos_h.xyz / world_pos_h.w; // UNCOMMENTED AND CORRECTED

    // Lighting Calculation for ONE light (u_num_lights will be 1, loop effectively runs once for i=0)
    vec3 N = world_normal; 
    vec3 V = normalize(u_camera_position - v_world_position); 
    vec3 light_contribution = vec3(0.0); 
    
    // Simplified lighting logic for one light (index 0)
    // This part needs to be robust for point/spot/directional based on u_light_type[0]
    // and include shadow calculations if applicable.
    // Assuming light properties are already in u_light_...[0]

    vec3 L_dir_calc;
    float attenuation = 1.0;
    bool is_directional = (u_light_type[0] == 3);

    if(is_directional) { // Directional
        L_dir_calc = normalize(u_light_direction[0]);
    } else { // Point or Spot
        vec3 light_to_frag_world = v_world_position - u_light_positions[0]; // vector from light to fragment
        float dist_sq = dot(light_to_frag_world, light_to_frag_world);
        L_dir_calc = -normalize(light_to_frag_world); // vector from fragment to light

        // Example attenuation for point/spot
        attenuation = 1.0 / (1.0 + 0.01*sqrt(dist_sq) + 0.001*dist_sq); 

        if (u_light_type[0] == 2) { // Spot
            float cos_angle = dot(normalize(light_to_frag_world), normalize(u_light_direction[0]));
            // Assuming u_light_direction stores spot direction, and you have cone angles
            // float inner_cone_cos = cos(radians(u_light_cone_info[0].x)); 
            // float outer_cone_cos = cos(radians(u_light_cone_info[0].y));
            float inner_cone_cos = 0.9; // example
            float outer_cone_cos = 0.7; // example
            float spot_factor = smoothstep(outer_cone_cos, inner_cone_cos, cos_angle);
            attenuation *= spot_factor;
        }
    }

    float NdotL = max(dot(N, L_dir_calc), 0.0);
    vec3 diffuse = albedo_color * u_light_colors[0] * u_light_intensity[0] * NdotL;

    vec3 R = reflect(-L_dir_calc, N);
    float RdotV = max(dot(R, V), 0.0);
    float shininess = 32.0; // Placeholder for material shininess
    vec3 specular = u_light_colors[0] * u_light_intensity[0] * pow(RdotV, shininess);
    
    light_contribution = (diffuse + specular) * attenuation; 

    // Shadow factor (simplified, needs u_shadow_vp, u_shadow_map, u_shadow_bias for the current light)
    float shadow_factor = 1.0;
    if (u_light_type[0] != 0 && textureSize(u_shadow_map[0],0).x > 1) { // Check if shadow map is valid (simplistic check)
        vec4 shadow_coord_clip = u_shadow_vp[0] * vec4(v_world_position, 1.0);
        vec3 shadow_coord_proj = shadow_coord_clip.xyz / shadow_coord_clip.w;
        shadow_coord_proj = shadow_coord_proj * 0.5 + 0.5; // to [0,1] range

        if (shadow_coord_proj.z < 1.0) { // inside shadow map frustum (z is depth)
            float depth_from_light = shadow_coord_proj.z - u_shadow_bias;
            float shadow_map_stored_depth = texture(u_shadow_map[0], shadow_coord_proj.xy).r;
            if (depth_from_light > shadow_map_stored_depth) {
                shadow_factor = 0.0; // In shadow
            }
        }
    }
    light_contribution *= shadow_factor;


    FragColor = vec4(light_contribution, 1.0); // Output light, alpha 1 for additive blend
}

// A5: TASK 2.2 - PBR GBuffer fill fragment shader

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

    out_gbuffer_albedo = final_color;
    out_gbuffer_normal = vec4(normalize(v_normal), 1.0);

    // Roughness, metallic and AO could be packed in another render target or alpha channel if needed
}

// A5: TASK 3.1 - Cook-Torrance PBR deferred lighting shader

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
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return a2 / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;
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

        if (u_light_type[i] == 1) // Point light
        {
            L = normalize(u_light_positions[i] - pos);
            float dist = length(u_light_positions[i] - pos);
            attenuation = 1.0 / (dist * dist);
        }
        else if (u_light_type[i] == 3) // Directional
        {
            L = normalize(u_light_direction[i]);
            attenuation = 1.0;
        }
        else if (u_light_type[i] == 2) // Spot
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
    color = pow(color, vec3(1.0/2.2));

    FragColor = vec4(color, albedo_sample.a);
}