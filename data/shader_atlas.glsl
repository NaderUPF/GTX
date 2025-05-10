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

// DEFERRED LIGHTING SHADER
deferred_lighting quad.vs deferred_lighting.fs

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

in vec2 v_uv; // Texcoord from quad.vs covering the screen

// G-Buffer Textures
uniform sampler2D u_albedo_texture;    // Albedo (RGB) and potentially alpha (A)
uniform sampler2D u_normal_material_texture; // World Normal (XYZ) and material data (W)
uniform sampler2D u_depth_texture;     // Scene depth

// Camera Uniforms for position reconstruction and lighting
uniform mat4 u_inverse_projection_matrix;
uniform mat4 u_inverse_view_matrix;
uniform vec3 u_camera_position; // Eye position for specular, etc.

// Light Uniforms (same as phong.fs)
uniform int u_num_lights;
uniform int u_light_type[10];       // 1:Point, 2:Spot, 3:Directional
uniform vec3 u_light_positions[10];
uniform vec3 u_light_colors[10];
uniform float u_light_intensity[10];
uniform vec3 u_light_direction[10]; // For directional and spotlights

// Shadow Mapping Uniforms (same as phong.fs)
uniform sampler2D u_shadow_map[10]; // Assuming max 10 shadow-casting lights
uniform mat4 u_shadow_vp[10];       // Light's ViewProjection matrix for each shadow map
uniform float u_shadow_bias;

out vec4 FragColor;

void main()
{
    // Sample G-Buffer
    vec4 albedo_gbuffer = texture(u_albedo_texture, v_uv);
    vec3 albedo_color = albedo_gbuffer.rgb;
    float alpha = albedo_gbuffer.a;

    vec4 normal_material_gbuffer = texture(u_normal_material_texture, v_uv);
    vec3 world_normal = normalize(normal_material_gbuffer.xyz);
    // float material_shininess = normal_material_gbuffer.w; // Example: if you store shininess in G-Buffer normal's W component

    float depth = texture(u_depth_texture, v_uv).r;

    // If depth is at or very near 1.0, it might be background/far plane.
    // Check for skybox normal first, as skybox can also be at far depth.
    if (length(world_normal) < 0.001) { // Our convention for skybox from skybox.fs
        FragColor = vec4(albedo_color, alpha); // Output sky color from albedo G-Buffer
        return;
    }
    if (depth >= 0.99999) { // Threshold for far plane
        // Output scene background color or a default clear color if not skybox
        FragColor = vec4(0.0, 0.0, 0.0, 1.0); // Example: Black or scene.background_color
        return;
    }

    // Reconstruct World Position from Depth
    // 1. To NDC coordinates: v_uv is [0,1] -> [-1,1], depth is [0,1] -> [-1,1]
    vec2 ndc_xy = v_uv * 2.0 - 1.0;
    float ndc_z = depth * 2.0 - 1.0;
    vec4 clip_pos = vec4(ndc_xy, ndc_z, 1.0);

    // 2. To View Space: Multiply by inverse projection
    vec4 view_pos = u_inverse_projection_matrix * clip_pos;
    view_pos /= view_pos.w; // Perspective divide

    // 3. To World Space: Multiply by inverse view
    vec3 v_world_position = (u_inverse_view_matrix * view_pos).xyz;

    // Lighting Calculations (similar to phong.fs)
    vec3 N = world_normal; // Already world space from G-Buffer and normalized
    vec3 V = normalize(u_camera_position - v_world_position); // View vector
    vec3 final_phong_color = vec3(0.0); // Start with black for lighting accumulation

    // Optional: A small global ambient term if desired, applied to albedo
    // final_phong_color += albedo_color * vec3(0.1); // Example: 10% ambient
    
    for (int i = 0; i < u_num_lights; ++i)
    {
        vec3 L; // Direction from surface point to light source
        float attenuation = 1.0;
        vec3 current_light_color = u_light_colors[i] * u_light_intensity[i];

        if (u_light_type[i] == 1) { // POINT
            L = u_light_positions[i] - v_world_position; // Vector from point to light
            float dist_sq = dot(L,L); // Use distance squared for efficiency
            L = normalize(L);
            // Example attenuation: 1 / (c1 + c2*dist + c3*dist^2)
            attenuation = 1.0 / (1.0 + 0.05*sqrt(dist_sq) + 0.01*dist_sq); 
        }
        else if (u_light_type[i] == 3) { // DIRECTIONAL
            L = normalize(u_light_direction[i]); // Direction TO the light (pre-normalized)
            attenuation = 1.0;
        }
        else if (u_light_type[i] == 2) { // SPOTLIGHT
            vec3 light_to_frag_vec = v_world_position - u_light_positions[i]; // Vector from light position to fragment
            float dist_sq = dot(light_to_frag_vec, light_to_frag_vec);
            L = normalize(-light_to_frag_vec); // Vector from fragment TO light

            vec3 spot_dir_norm = normalize(u_light_direction[i]); // Spotlight's main direction
            // cos_angle is between vector from light to fragment and spotlight direction
            float cos_angle = dot(normalize(light_to_frag_vec), spot_dir_norm); 
            
            // Use uniform angles for spot cone if available, otherwise hardcoded
            float outer_cone_cos = cos(radians(25.0)); // e.g. 25 deg outer cutoff angle
            float inner_cone_cos = cos(radians(20.0)); // e.g. 20 deg inner cutoff angle (full intensity)
            // smoothstep provides a smooth transition between inner and outer cone
            float spot_factor = smoothstep(outer_cone_cos, inner_cone_cos, cos_angle);

            attenuation = spot_factor / (1.0 + 0.05*sqrt(dist_sq) + 0.01*dist_sq);
        }
        else { continue; } // Unknown light type

        float NdotL = max(dot(N, L), 0.0);
        if (NdotL <= 0.0) {
            // If light is behind the surface point, it contributes nothing to diffuse/specular
            continue;
        }

        // Shadow Calculation
        float shadow_factor = 1.0; // Assume not in shadow initially
        // Check if this light casts shadows (you might have a flag or check if shadow_vp is valid)
        // For simplicity, assume all lights from 0 to u_num_lights (up to 10) can cast shadows if their maps are bound
        if (i < 10) { // Check against max shadow maps supported (array size)
            vec4 shadow_coord_clip = u_shadow_vp[i] * vec4(v_world_position, 1.0); // To light's clip space
            vec3 shadow_coord_ndc = shadow_coord_clip.xyz / shadow_coord_clip.w;   // To NDC [-1,1]
            vec2 shadow_uv = shadow_coord_ndc.xy * 0.5 + 0.5; // Convert NDC to Shadow Map UV [0,1]

            // Check if the fragment is within the shadow map's UV and depth range
            if (shadow_uv.x >= 0.0 && shadow_uv.x <= 1.0 &&
                shadow_uv.y >= 0.0 && shadow_uv.y <= 1.0 &&
                shadow_coord_ndc.z <= 1.0) // z<=1 in NDC means it's not clipped by far plane of light
            {
                float shadow_map_depth = texture(u_shadow_map[i], shadow_uv).r; // Depth stored in shadow map
                // current_depth_in_light_space should also be in [0,1] for comparison
                float current_depth_in_light_space = shadow_coord_ndc.z * 0.5 + 0.5; // Depth of current fragment from light's POV, in [0,1]

                if (current_depth_in_light_space > shadow_map_depth + u_shadow_bias) {
                    shadow_factor = 0.0; // Fragment is in shadow
                }
            }
            // else: Fragment is outside this light's shadow map frustum, assume not shadowed by *this specific map*
        }

        if (shadow_factor > 0.0) { // Only apply light if not fully in shadow
            // Diffuse reflection
            vec3 diffuse = albedo_color * current_light_color * NdotL;

            // Specular reflection
            vec3 R = reflect(-L, N); // Reflection vector
            float RdotV = max(dot(R, V), 0.0);
            
            // Shininess: Retrieve from G-Buffer (e.g., normal_material_gbuffer.w) or use a default
            // Assuming normal_material_gbuffer.w stores a value [0,1], map it to a specular power.
            float shininess_val = normal_material_gbuffer.w * 128.0; // Example: map [0,1] to [0,128]
            if (shininess_val < 1.0) shininess_val = 32.0; // Default shininess if zero or too small

            vec3 specular = current_light_color * pow(RdotV, shininess_val);

            final_phong_color += (diffuse + specular) * attenuation * shadow_factor;
        }
    }

    FragColor = vec4(final_phong_color, alpha);
}
