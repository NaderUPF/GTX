//example of some shaders compiled
flat basic.vs flat.fs
texture basic.vs texture.fs
skybox basic.vs skybox.fs
depth quad.vs depth.fs
multi basic.vs multi.fs
compute test.cs

// ASSIGNMENT2: TASK 2 & 3
phong basic.vs phong.fs 

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

\flat.fs
#version 330 core
uniform vec4 u_color;
out vec4 FragColor;
void main() {
	FragColor = u_color;
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

//CODI PROFE
uniform int light_types[10];
uniform vec3 u_light_positions[10];
uniform vec3 u_light_colors[10];
uniform float u_light_intensity[10];

uniform vec3 u_light_direction[10];

out vec4 FragColor;

void main()
{
	vec2 uv = v_uv;
	vec4 color = u_color;
	color *= texture(u_texture, v_uv);

// CODI PROFE
	vec3 light_component = vec3(0.0);
	//ambient
	for(int i = 0; i < 3; i++) {
		vec3 L; 
			if (light_types[i] == 1) { // POINT
				L = normalize(u_light_positions[i] - v_world_position);
			} else if (light_types[i] == 3) { // dir
				L = normalize(u_light_direction[i]);
			}
	
		float l_dot_n = clamp(dot(L, normalize(v_normal)), 0.0, 1.0);
		light_component += u_light_intensity[i] * u_light_colors[i] * l_dot_n; // + specular
	}

	vec3 L = normalize(u_light_direction[3]);
	float l_dot_n = clamp(dot(L, normalize(v_normal)), 0.0, 1.0);
	light_component += u_light_intensity[3] * u_light_colors[3] * l_dot_n; // + specular

	if(color.a < u_alpha_cutoff)
		discard;

	FragColor = color * vec4(light_component, 1.0);
}

\skybox.fs
#version 330 core
in vec3 v_position;
in vec3 v_world_position;

uniform samplerCube u_texture;
uniform vec3 u_camera_position;
out vec4 FragColor;

void main()
{
	vec3 E = v_world_position - u_camera_position;
	vec4 color = texture(u_texture, E);
	FragColor = color;
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
uniform float u_shininess;

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

	// Ambient lighting
	final_color += 0.1 * color.rgb;

	for (int i = 0; i < u_num_lights; ++i)
	{
		vec3 L;
		float attenuation = 1.0;

		if (u_light_type[i] == 0) { // POINT
			L = normalize(u_light_positions[i] - v_world_position);
			float dist = length(u_light_positions[i] - v_world_position);
			attenuation = 1.0 / (1.0 + 0.1 * dist + 0.01 * dist * dist);
		}
		else if (u_light_type[i] == 1) { // DIRECTIONAL
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
			attenuation = spot_smooth / (1.0 + 0.1 * dist + 0.01 * dist * dist);
		}

		float NdotL = max(dot(N, L), 0.0);
		vec3 R = reflect(-L, N);
		float spec = pow(max(dot(R, V), 0.0), u_shininess);

		vec3 light_contrib = u_light_intensity[i] * u_light_colors[i] * (NdotL + spec);
		final_color += light_contrib * attenuation;
	}

	FragColor = vec4(final_color * color.rgb, color.a);
}