#include "renderer.h"

#include <algorithm> //sort
#include <iostream> // For std::cerr

#include "camera.h"
#include "../gfx/gfx.h"
#include "../gfx/shader.h"
#include "../gfx/mesh.h"
#include "../gfx/texture.h"
#include "../gfx/fbo.h"
#include "../pipeline/prefab.h"
#include "../pipeline/material.h"
#include "../pipeline/animation.h"
#include "../utils/utils.h"
#include "../extra/hdre.h"
#include "../core/ui.h"
#include "../core/core.h"

#include "scene.h"
#include "shadow.h" // A3: TASK - Shadow map system
#include "gbuffer.h" // Make sure gbuffer is included

// Globals as per user's provided file structure
std::vector<sDrawCommand> draw_command_list;
std::vector<SCN::LightEntity*> light_list; // A2: TASK 1 - Light List

using namespace SCN;

//some globals
GFX::Mesh sphere;
// GFX::FBO gbuffer_fbo; // This will be SCN::Renderer::gbuffer.gbuffer_fbo

Renderer::Renderer(const char* shader_atlas_filename)
{
	render_wireframe = false;
	render_boundaries = false;
	scene = nullptr;
	skybox_cubemap = nullptr;
	reflection_fbo = nullptr;
	reflection_texture = nullptr;

	if (!GFX::Shader::LoadAtlas(shader_atlas_filename)) {
		std::cerr << "Error: Failed to load shader atlas: " << shader_atlas_filename << std::endl;
		exit(1);
	}
	GFX::checkGLErrors();

	// Get screen dimensions (using defines or CORE::getWindowSize())
	// For consistency, let's assume CORE::getWindowSize() is preferred if available and working.
	// If SCREEN_WIDTH/HEIGHT macros are primary, ensure they are correctly defined.
	vec2 w_size = CORE::getWindowSize();
	int screen_w = static_cast<int>(w_size.x);
	int screen_h = static_cast<int>(w_size.y);

	if (screen_w <= 0 || screen_h <= 0) {
        // Fallback if CORE::getWindowSize() returned invalid dimensions
        #if defined(SCREEN_WIDTH) && defined(SCREEN_HEIGHT)
		    screen_w = SCREEN_WIDTH;
		    screen_h = SCREEN_HEIGHT;
            if (screen_w <=0 || screen_h <=0) { // If defines also bad
                 std::cerr << "Warning: SCREEN_WIDTH/HEIGHT are invalid. Using 800x600." << std::endl;
                 screen_w = 800; screen_h = 600;
            }
        #else
		    std::cerr << "Warning: CORE::getWindowSize() returned invalid dimensions and SCREEN_WIDTH/HEIGHT not defined. Using 800x600." << std::endl;
		    screen_w = 800;
		    screen_h = 600;
        #endif
	}
	
	gbuffer.create(screen_w, screen_h); // GBuffer with 2 color, 1 depth
	GFX::checkGLErrors();

	// Create the lighting FBO: 1 color texture (RGBA8), and a depth texture (for depth testing using G-Buffer's depth)
	// The depth texture here is primarily for the FBO structure; its content will be copied from gbuffer.
	if (!lighting_fbo.create(screen_w, screen_h, 1, GL_RGBA, GL_UNSIGNED_BYTE, true)) {
		std::cerr << "Error: Failed to create lighting_fbo." << std::endl;
		// Handle error, perhaps exit or fallback
	}
	GFX::checkGLErrors();


	sphere.createSphere(1.0f, 32, 24);
	if (sphere.getNumVertices() == 0) {
		std::cerr << "ERROR: Failed to procedurally create sphere mesh. Skybox and light volumes might not render." << std::endl;
	} else {
		// std::cout << "Log: Global sphere mesh created procedurally." << std::endl; // Less verbose
	}
	GFX::checkGLErrors(); 
}

void Renderer::setupScene()
{
	if (this->scene && this->scene->skybox_filename.size()) {
		skybox_cubemap = GFX::Texture::Get(std::string(this->scene->base_folder + "/" + this->scene->skybox_filename).c_str());
	}
	else {
		skybox_cubemap = nullptr;
	}
}

// A1: TASK 2 - Parse scene and generate render calls
void Renderer::parseNodes(SCN::Node* node, Camera* cam, std::vector<sDrawCommand>& commands_list) {
	if (!node || !node->visible) {
		return;
	}
	if (node->mesh && node->material) {
		sDrawCommand draw_com;
		draw_com.mesh = node->mesh;
		draw_com.material = node->material;
		draw_com.model = node->getGlobalMatrix();

		if (cam) {
			draw_com.camera_distance = (cam->eye - draw_com.model.getTranslation()).length();
		}
		else {
			draw_com.camera_distance = 0.0f;
		}

		draw_com.is_transparent = (node->material->alpha_mode != SCN::NO_ALPHA);
		commands_list.push_back(draw_com);
	}
	for (SCN::Node* child : node->children) {
		parseNodes(child, cam, commands_list);
	}
}

void Renderer::parseSceneEntities(SCN::Scene* scene_ptr, Camera* cam, std::vector<sDrawCommand>& commands_list, std::vector<SCN::LightEntity*>& lights_list) {
	commands_list.clear();
	lights_list.clear();

	if (!scene_ptr) {
		std::cerr << "Error: parseSceneEntities called with null scene_ptr." << std::endl;
		return;
	}
	if (!cam) {
		std::cerr << "Error: parseSceneEntities called with null camera." << std::endl;
		// Decide if you can proceed without a camera for parsing, or return
	}

	for (size_t i = 0; i < scene_ptr->entities.size(); ++i) {
		SCN::BaseEntity* entity = scene_ptr->entities[i];

		if (!entity || !entity->visible) {
			continue;
		}

		if (entity->getType() == SCN::eEntityType::PREFAB) {
			SCN::PrefabEntity* prefab_ent = static_cast<SCN::PrefabEntity*>(entity);
			if (prefab_ent) {
				parseNodes(&prefab_ent->root, cam, commands_list);
			}
		}
		else if (entity->getType() == SCN::eEntityType::LIGHT) {
			SCN::LightEntity* light_ent = static_cast<SCN::LightEntity*>(entity);
			if (light_ent) {
				lights_list.push_back(light_ent);
			}
		}
	}
}

// A2: TASK 3 - Upload light uniforms
void Renderer::uploadLights(GFX::Shader* shader, const std::vector<SCN::LightEntity*>& lights_list_ref) {
	if (!shader || !shader->IsUniform("u_num_lights")) {
		return;
	}

	int count = static_cast<int>(lights_list_ref.size());
	if (count > 10) count = 10;

	vec3 light_positions[10];
	vec3 light_colors[10];
	float light_intensity[10];
	vec3 light_direction[10];
	int light_type_arr[10];

	for (int i = 0; i < count; ++i) {
		LightEntity* light = lights_list_ref[i];
		if (!light) continue;
		light_positions[i] = light->root.getGlobalMatrix().getTranslation();
		light_colors[i] = light->color;
		light_intensity[i] = light->intensity;
		light_direction[i] = light->root.model.frontVector();
		switch (light->light_type) {
		case SCN::eLightType::POINT:    light_type_arr[i] = 1; break;
		case SCN::eLightType::SPOT:     light_type_arr[i] = 2; break;
		case SCN::eLightType::DIRECTIONAL: light_type_arr[i] = 3; break;
		default: light_type_arr[i] = 1;
		}
	}

	shader->setUniform("u_num_lights", count);
	if (count > 0) {
		shader->setUniform3Array("u_light_positions", reinterpret_cast<const float*>(light_positions), count);
		shader->setUniform3Array("u_light_colors", reinterpret_cast<const float*>(light_colors), count);
		shader->setUniform1Array("u_light_intensity", light_intensity, count);
		shader->setUniform3Array("u_light_direction", reinterpret_cast<const float*>(light_direction), count);
		shader->setUniform1Array("u_light_type", light_type_arr, count);
	}
}

// Definition of the new private helper methods:

void Renderer::renderGBufferPass(Camera* camera, const std::vector<sDrawCommand>& opaque_commands) {
	gbuffer.gbuffer_fbo.bind();
	GFX::checkGLErrors();

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f); // Clear G-Buffer to black/transparent
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	GFX::checkGLErrors();

	if (skybox_cubemap) {
		renderSkybox(skybox_cubemap); 
		GFX::checkGLErrors();
	}

	GFX::Shader* gbuffer_fill_shader = GFX::Shader::Get("gbuffer_fill");
	if (!gbuffer_fill_shader) {
		std::cerr << "GBuffer fill shader 'gbuffer_fill' not found!" << std::endl;
		gbuffer.gbuffer_fbo.unbind();
		return;
	}

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); 
    glDepthMask(GL_TRUE); // Ensure depth writing is enabled for G-Buffer

	for (const auto& command : opaque_commands) {
		renderMeshWithMaterial(command.model, command.mesh, command.material, gbuffer_fill_shader);
	}
	GFX::checkGLErrors();

	gbuffer.gbuffer_fbo.unbind();
}

// renderDeferredLightingPass now renders into lighting_fbo
void Renderer::renderDeferredLightingPass(Camera* camera, const std::vector<SCN::LightEntity*>& lights) {
	if (gbuffer.gbuffer_fbo.depth_texture && lighting_fbo.depth_texture) {
		gbuffer.gbuffer_fbo.depth_texture->copyTo(lighting_fbo.depth_texture);
        GFX::checkGLErrors();
	} else {
		std::cerr << "Error: Depth texture missing for G-Buffer to Lighting FBO copy." << std::endl;
		return;
	}

	lighting_fbo.bind();
	GFX::checkGLErrors();
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f); 
	glClear(GL_COLOR_BUFFER_BIT);        
	GFX::checkGLErrors();

    // Calculate inverse screen size for shader uniform
    vec2 window_s = CORE::getWindowSize(); // Or get from G-Buffer/Lighting FBO dimensions
    if (window_s.x <= 0 || window_s.y <= 0) { // Fallback if window size is invalid
        window_s.x = static_cast<float>(gbuffer.gbuffer_fbo.width);
        window_s.y = static_cast<float>(gbuffer.gbuffer_fbo.height);
         if (window_s.x <= 0 || window_s.y <= 0) { // Final fallback
            window_s.x = 800; window_s.y = 600;
        }
    }
    vec2 inv_screen_size(1.0f / window_s.x, 1.0f / window_s.y);

	// Backup GL State (as before)
	GLboolean prev_blend_enabled = glIsEnabled(GL_BLEND);
	GLint prev_blend_eq_rgb, prev_blend_eq_alpha;
	GLint prev_blend_src_rgb, prev_blend_dst_rgb, prev_blend_src_alpha, prev_blend_dst_alpha;
	glGetIntegerv(GL_BLEND_EQUATION_RGB, &prev_blend_eq_rgb);
	glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &prev_blend_eq_alpha);
	glGetIntegerv(GL_BLEND_SRC_RGB, &prev_blend_src_rgb);
	glGetIntegerv(GL_BLEND_DST_RGB, &prev_blend_dst_rgb);
	glGetIntegerv(GL_BLEND_SRC_ALPHA, &prev_blend_src_alpha);
	glGetIntegerv(GL_BLEND_DST_ALPHA, &prev_blend_dst_alpha);
	GLboolean prev_cull_face_enabled = glIsEnabled(GL_CULL_FACE);
	GLint prev_cull_face_mode; glGetIntegerv(GL_CULL_FACE_MODE, &prev_cull_face_mode);
	GLint prev_front_face; glGetIntegerv(GL_FRONT_FACE, &prev_front_face);
	GLboolean prev_depth_writemask; glGetBooleanv(GL_DEPTH_WRITEMASK, &prev_depth_writemask);
	GLint prev_depth_func; glGetIntegerv(GL_DEPTH_FUNC, &prev_depth_func);

	// --- POINT AND SPOT LIGHTS (Light Volumes) ---
	GFX::Shader* light_volume_shader = GFX::Shader::Get("light_volume_deferred"); // Use new shader
	if (!light_volume_shader) {
		std::cerr << "Light volume shader 'light_volume_deferred' not found!" << std::endl;
	} else {
		light_volume_shader->enable();

		// Set G-Buffer common textures (source for lighting calc)
		if (gbuffer.gbuffer_fbo.num_color_textures > 0 && gbuffer.gbuffer_fbo.color_textures[0]) {
			light_volume_shader->setUniform("u_albedo_texture", gbuffer.gbuffer_fbo.color_textures[0], 0);
		}
		if (gbuffer.gbuffer_fbo.num_color_textures > 1 && gbuffer.gbuffer_fbo.color_textures[1]) {
			light_volume_shader->setUniform("u_normal_material_texture", gbuffer.gbuffer_fbo.color_textures[1], 1);
		}
		if (gbuffer.gbuffer_fbo.depth_texture) {
			light_volume_shader->setUniform("u_depth_texture", gbuffer.gbuffer_fbo.depth_texture, 2);
		}
		GFX::checkGLErrors();

		// Camera Uniforms for position reconstruction & lighting
		Matrix44 inv_proj = camera->projection_matrix; inv_proj.inverse();
		Matrix44 inv_view = camera->view_matrix;   inv_view.inverse();
		light_volume_shader->setUniform("u_inverse_projection_matrix", inv_proj);
		light_volume_shader->setUniform("u_inverse_view_matrix", inv_view);
		light_volume_shader->setUniform3("u_camera_position", camera->eye.x, camera->eye.y, camera->eye.z);
        light_volume_shader->setUniform2("u_inv_screen_size", inv_screen_size.x, inv_screen_size.y); // Set inv screen size

		glEnable(GL_BLEND);
		glBlendEquation(GL_FUNC_ADD);
		glBlendFunc(GL_ONE, GL_ONE);

		glEnable(GL_CULL_FACE);
		glFrontFace(GL_CW);      
		glCullFace(GL_FRONT);    

		glEnable(GL_DEPTH_TEST); 
		glDepthMask(GL_FALSE);     
		glDepthFunc(GL_GREATER); // CHANGED from GL_GEQUAL

		GFX::Mesh* light_volume_mesh_ptr = &sphere; 

		for (SCN::LightEntity* light : lights) {
			if (light->light_type == SCN::eLightType::POINT || light->light_type == SCN::eLightType::SPOT) {
				if (light->max_distance <= 0.0f) continue;

				Matrix44 light_sphere_model_matrix;
				light_sphere_model_matrix.setTranslation(light->root.getGlobalMatrix().getTranslation().x,
					light->root.getGlobalMatrix().getTranslation().y,
					light->root.getGlobalMatrix().getTranslation().z);
				light_sphere_model_matrix.scale(light->max_distance, light->max_distance, light->max_distance);
				
                // Set uniforms for basic.vs (transformation of the sphere)
				light_volume_shader->setUniform("u_model", light_sphere_model_matrix);
				light_volume_shader->setUniform("u_viewprojection", camera->viewprojection_matrix);

                // Set uniforms for deferred_lighting.fs (light properties)
				vec3 single_light_pos = light->root.getGlobalMatrix().getTranslation();
				vec3 single_light_dir = light->root.model.frontVector();
				int single_light_type_int_val = (light->light_type == SCN::eLightType::POINT) ? 1 : 2;

				light_volume_shader->setUniform("u_num_lights", 1); // Shader expects array of 1
				light_volume_shader->setUniform3("u_light_positions[0]", single_light_pos.x, single_light_pos.y, single_light_pos.z);
				light_volume_shader->setUniform3("u_light_colors[0]", light->color.x, light->color.y, light->color.z);
				light_volume_shader->setUniform1("u_light_intensity[0]", light->intensity);
				light_volume_shader->setUniform3("u_light_direction[0]", single_light_dir.x, single_light_dir.y, single_light_dir.z);
				light_volume_shader->setUniform1("u_light_type[0]", single_light_type_int_val);
				
                // Simplified shadow part for light volumes
                bool lv_shadow_params_set = false;
                if (light->cast_shadows && light_volume_shader->IsUniform("u_shadow_map[0]")) {
                    for (int k = 0; k < SHADOW::active_shadow_count; ++k) {
                        if (SHADOW::getShadowMap(k) != nullptr) { 
                            light_volume_shader->setUniform("u_shadow_map[0]", SHADOW::getShadowMap(k), 3); // Ensure tex unit is fine 
                            light_volume_shader->setUniform("u_shadow_vp[0]", SHADOW::getLightCamera(k).viewprojection_matrix);
                            light_volume_shader->setUniform("u_shadow_bias", SHADOW::getShadowBias());
                            lv_shadow_params_set = true;
                            break; 
                        }
                    }
                     if (!lv_shadow_params_set) { // Ensure shader doesn't use stale shadow map if none for this light
                        // Option: bind a dummy 1x1 white texture, or ensure shader handles no shadow map
                    }
                }


				if (light_volume_mesh_ptr->getNumVertices() > 0) {
					light_volume_mesh_ptr->render(GL_TRIANGLES);
				}
			}
		}
        light_volume_shader->disable(); // Disable after point/spot lights
	}
	GFX::checkGLErrors();

	// --- DIRECTIONAL LIGHTS (Full-screen Quad) ---
	GFX::Shader* deferred_dir_shader = GFX::Shader::Get("deferred_lighting"); // This is quad.vs + deferred_lighting.fs
    if (!deferred_dir_shader) {
        std::cerr << "Deferred directional lighting shader 'deferred_lighting' not found!" << std::endl;
    } else {
        deferred_dir_shader->enable();

        // Set G-Buffer common textures
        if (gbuffer.gbuffer_fbo.num_color_textures > 0 && gbuffer.gbuffer_fbo.color_textures[0]) {
            deferred_dir_shader->setUniform("u_albedo_texture", gbuffer.gbuffer_fbo.color_textures[0], 0);
        }
        if (gbuffer.gbuffer_fbo.num_color_textures > 1 && gbuffer.gbuffer_fbo.color_textures[1]) {
            deferred_dir_shader->setUniform("u_normal_material_texture", gbuffer.gbuffer_fbo.color_textures[1], 1);
        }
        if (gbuffer.gbuffer_fbo.depth_texture) {
            deferred_dir_shader->setUniform("u_depth_texture", gbuffer.gbuffer_fbo.depth_texture, 2);
        }
        
        // Camera Uniforms for position reconstruction & lighting
        Matrix44 inv_proj_dir = camera->projection_matrix; inv_proj_dir.inverse();
        Matrix44 inv_view_dir = camera->view_matrix;   inv_view_dir.inverse();
        deferred_dir_shader->setUniform("u_inverse_projection_matrix", inv_proj_dir);
        deferred_dir_shader->setUniform("u_inverse_view_matrix", inv_view_dir);
        deferred_dir_shader->setUniform3("u_camera_position", camera->eye.x, camera->eye.y, camera->eye.z);
        deferred_dir_shader->setUniform2("u_inv_screen_size", inv_screen_size.x, inv_screen_size.y); // Set inv screen size

        // For directional lights, depth test should compare against far plane or be effectively off
        // if the quad is to cover everything. Using GL_ALWAYS. Blend is already set.
        glCullFace(GL_BACK);    
        glFrontFace(GL_CCW);   
        glDepthFunc(GL_ALWAYS); // Directional light affects all pixels regardless of depth of the quad

        GFX::Mesh* quad_mesh_ptr = GFX::Mesh::getQuad();
        if (quad_mesh_ptr) {
            for (SCN::LightEntity* light : lights) {
                if (light->light_type == SCN::eLightType::DIRECTIONAL) {
                    vec3 single_light_dir_val = light->root.model.frontVector();
                    
                    deferred_dir_shader->setUniform("u_num_lights", 1); // Shader expects array of 1
                    deferred_dir_shader->setUniform3("u_light_positions[0]", 0.0f,0.0f,0.0f); // Not used by directional in shader's L_dir_calc
                    deferred_dir_shader->setUniform3("u_light_colors[0]", light->color.x, light->color.y, light->color.z);
                    deferred_dir_shader->setUniform1("u_light_intensity[0]", light->intensity);
                    deferred_dir_shader->setUniform3("u_light_direction[0]", single_light_dir_val.x, single_light_dir_val.y, single_light_dir_val.z);
                    deferred_dir_shader->setUniform1("u_light_type[0]", 3); 

                    // Simplified shadow part for directional lights
                    bool dir_s_params_set = false;
                    if (light->cast_shadows && deferred_dir_shader->IsUniform("u_shadow_map[0]")) {
                        for (int k = 0; k < SHADOW::active_shadow_count; ++k) {
                            if (SHADOW::getShadowMap(k) != nullptr) { 
                                deferred_dir_shader->setUniform("u_shadow_map[0]", SHADOW::getShadowMap(k), 3); 
                                deferred_dir_shader->setUniform("u_shadow_vp[0]", SHADOW::getLightCamera(k).viewprojection_matrix);
                                deferred_dir_shader->setUniform("u_shadow_bias", SHADOW::getShadowBias());
                                dir_s_params_set = true;
                                break; 
                            }
                        }
                         if (!dir_s_params_set) {
                            // Option: bind a dummy 1x1 white texture
                        }
                    }
                    quad_mesh_ptr->render(GL_TRIANGLES);
                }
            }
        }
        deferred_dir_shader->disable(); // Disable after directional lights
    }
	GFX::checkGLErrors();

	lighting_fbo.unbind(); 

	// Restore GL States (as before)
	if (prev_blend_enabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
	glBlendEquationSeparate(prev_blend_eq_rgb, prev_blend_eq_alpha);
	glBlendFuncSeparate(prev_blend_src_rgb, prev_blend_dst_rgb, prev_blend_src_alpha, prev_blend_dst_alpha);
	if (prev_cull_face_enabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
	glCullFace(prev_cull_face_mode);
	glFrontFace(prev_front_face);
	glDepthMask(prev_depth_writemask);
	glDepthFunc(prev_depth_func); 
	GFX::checkGLErrors();
}

void Renderer::compositeLightingToScreen() {
    glViewport(0, 0, static_cast<GLsizei>(CORE::getWindowSize().x), static_cast<GLsizei>(CORE::getWindowSize().y));
    // Main framebuffer is already bound (or should be by default after FBO unbind)
    
    glDisable(GL_DEPTH_TEST); // No depth test for full-screen blit
    glDisable(GL_BLEND);      // Direct copy, no blending (or use GL_ONE, GL_ZERO if needed)

    GFX::Shader* tex_shader = GFX::Shader::Get("texture");
    if (!tex_shader) {
        std::cerr << "Texture shader for composition not found!" << std::endl;
        return;
    }
    if (lighting_fbo.num_color_textures == 0 || lighting_fbo.color_textures[0] == nullptr) {
        std::cerr << "Lighting FBO color texture is missing for composition!" << std::endl;
        return;
    }

    tex_shader->enable();
    tex_shader->setUniform("u_color", Vector4f(1,1,1,1)); // Ensure no tint
    tex_shader->setUniform("u_texture", lighting_fbo.color_textures[0], 0);
    
    GFX::Mesh* quad = GFX::Mesh::getQuad();
    if(quad) quad->render(GL_TRIANGLES);
    
    tex_shader->disable();
    GFX::checkGLErrors();
}

void Renderer::renderTransparentPass(Camera* camera, const std::vector<sDrawCommand>& transparent_commands, const std::vector<SCN::LightEntity*>& lights) {
	// ... (transparent pass as before, ensuring it renders to the main framebuffer) ...
    // Important: ensure depth test is re-enabled and blend func is for transparency
	GFX::Shader* phong_shader = GFX::Shader::Get("phong"); 
	if (!phong_shader) {
        std::cerr << "Phong shader for transparents not found!" << std::endl;
        return;
    }

    phong_shader->enable(); 

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); 
	
    glEnable(GL_DEPTH_TEST); // Re-enable depth test
	glDepthMask(GL_TRUE);    // Allow writing to depth buffer for transparents (can be tricky)
    glDepthFunc(GL_LESS);    // Standard depth test

	uploadLights(phong_shader, lights); 

	for (const auto& command : transparent_commands) {
        phong_shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
        phong_shader->setUniform3("u_camera_position", camera->eye.x, camera->eye.y, camera->eye.z);
		renderMeshWithMaterial(command.model, command.mesh, command.material, phong_shader);
	}
    phong_shader->disable(); 
	glDisable(GL_BLEND);
	GFX::checkGLErrors();
}

// Updated renderScene
void Renderer::renderScene(SCN::Scene* scene_ptr, Camera* camera)
{
	if (!scene_ptr || !camera) return; 

	this->scene = scene_ptr; 
	setupScene(); 

	std::vector<sDrawCommand> local_draw_commands;
	std::vector<SCN::LightEntity*> local_light_list;
	parseSceneEntities(scene_ptr, camera, local_draw_commands, local_light_list);

	std::vector<sDrawCommand> opaque_commands, transparent_commands;
	for (const auto& cmd : local_draw_commands) {
		if (cmd.is_transparent)
			transparent_commands.push_back(cmd);
		else
			opaque_commands.push_back(cmd);
	}
	std::sort(opaque_commands.begin(), opaque_commands.end(), [](const sDrawCommand& a, const sDrawCommand& b) {
		return a.camera_distance < b.camera_distance;
	});
	std::sort(transparent_commands.begin(), transparent_commands.end(), [](const sDrawCommand& a, const sDrawCommand& b) {
		return a.camera_distance > b.camera_distance; 
	});

	SHADOW::renderAllShadowMaps(scene_ptr, opaque_commands, local_light_list);
	GFX::checkGLErrors();

	renderGBufferPass(camera, opaque_commands);
	GFX::checkGLErrors();

	// Main framebuffer is implicitly active after gbuffer.unbind()
	// Clear main framebuffer (color will be overwritten by composite, depth might be used by transparents)
	glViewport(0, 0, static_cast<GLsizei>(CORE::getWindowSize().x), static_cast<GLsizei>(CORE::getWindowSize().y));
	glClearColor(scene_ptr->background_color.x, scene_ptr->background_color.y, scene_ptr->background_color.z, 1.0f); // Clear to background for areas not covered by lighting pass result.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clear both color and depth
	GFX::checkGLErrors();
	
	renderDeferredLightingPass(camera, local_light_list); // Renders into lighting_fbo
	GFX::checkGLErrors();
	
	compositeLightingToScreen(); // Blits lighting_fbo result to main screen
	GFX::checkGLErrors();
	
	renderTransparentPass(camera, transparent_commands, local_light_list); // Renders to main screen
	GFX::checkGLErrors();
}

void Renderer::renderSkybox(GFX::Texture* cubemap)
{
	Camera* camera = Camera::current;
	if (!camera || !cubemap) return;

	// GL states for skybox rendering (targeting G-Buffer)
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST); 
	glDepthMask(GL_TRUE);    
	glDisable(GL_CULL_FACE); 

	if (render_wireframe)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	GFX::Shader* shader = GFX::Shader::Get("skybox");
	if (!shader) {
		std::cerr << "Skybox shader (for G-Buffer) not found!" << std::endl;
		if (render_wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		return;
	}
	shader->enable(); // Single enable before setting any uniforms for this shader

	Matrix44 m;
	m.setTranslation(camera->eye.x, camera->eye.y, camera->eye.z);
	m.scale(camera->far_plane * 0.99f, camera->far_plane * 0.99f, camera->far_plane * 0.99f);
	
	shader->setUniform("u_model", m); 
	shader->setUniform("u_viewprojection", camera->viewprojection_matrix); 
	shader->setUniform3("u_camera_position", camera->eye.x, camera->eye.y, camera->eye.z);

	// This is the call that currently triggers the assertion
	shader->setUniform("u_texture", cubemap, 0); 

	if (sphere.getNumVertices() > 0) sphere.render(GL_TRIANGLES); 

	shader->disable();

	if (render_wireframe)
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

// Renders a mesh given its transform and material, optionally with a specific shader
void Renderer::renderMeshWithMaterial(const Matrix44 model, GFX::Mesh* mesh, SCN::Material* material, GFX::Shader* override_shader)
{
	if (!mesh || !mesh->getNumVertices() || !material || !Camera::current) return;
	assert(glGetError() == GL_NO_ERROR);

	GFX::Shader* shader_to_use = override_shader;
	Camera* camera = Camera::current;

	if (!shader_to_use) { // If no override, use default logic (e.g., phong, or material specified shader if any)
		// This part might need more sophisticated logic if materials can specify their own shaders.
		// For now, defaulting to "phong" if no override.
		shader_to_use = GFX::Shader::Get("phong");
	}

	if (!shader_to_use) {
		std::cerr << "No shader available for renderMeshWithMaterial (model: " << (mesh->name.empty() ? "unnamed" : mesh->name.c_str()) << ")" << std::endl;
		return;
	}
	shader_to_use->enable();

	// material->bind() should handle:
	// - Alpha mode (blending setup)
	// - Two-sided (culling setup)
	// - Binding textures (u_texture, etc.)
	// - Setting material-specific uniforms (u_color, u_alpha_cutoff, etc.)
	material->bind(shader_to_use);
	GFX::checkGLErrors();


	// Upload common uniforms required by most shaders (like basic.vs)
	shader_to_use->setUniform("u_model", model);
	shader_to_use->setUniform("u_viewprojection", camera->viewprojection_matrix);
	shader_to_use->setUniform3("u_camera_position", camera->eye.x, camera->eye.y, camera->eye.z);
	shader_to_use->setUniform1("u_time", static_cast<float>(getTime()));

	// Conditional uploads: Only for relevant shaders (e.g., "phong" or lighting shaders)
	// The G-Buffer fill shader ("gbuffer_fill") does not use lights or shadow maps directly.
	GFX::Shader* phong_shader_ptr = GFX::Shader::Get("phong");
	if (shader_to_use == phong_shader_ptr) {
		// Light uniforms are uploaded by renderScene before rendering transparents.
		// Shadow uniforms are uploaded here if the shader is phong (which uses them).
		if (shader_to_use->IsUniform("u_shadow_map[0]")) {
			int shadow_maps_to_bind = std::min<int>(SHADOW::active_shadow_count, 10); // Assuming max 10 shadow maps for phong
			for (int i = 0; i < shadow_maps_to_bind; ++i) {
				std::string shadow_map_name = "u_shadow_map[" + std::to_string(i) + "]";
				std::string shadow_vp_name = "u_shadow_vp[" + std::to_string(i) + "]";
				if (SHADOW::getShadowMap(i)) {
					shader_to_use->setUniform(shadow_map_name.c_str(), SHADOW::getShadowMap(i), 8 + i); // Ensure texture units don't clash
				}
				shader_to_use->setUniform(shadow_vp_name.c_str(), SHADOW::getLightCamera(i).viewprojection_matrix);
			}
			shader_to_use->setUniform("u_shadow_bias", SHADOW::getShadowBias());
		}
	}
	GFX::checkGLErrors();

	if (render_wireframe)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	mesh->render(GL_TRIANGLES);
	GFX::checkGLErrors();

	shader_to_use->disable();

	if (render_wireframe)
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	// Material should ideally unbind its specific states (like blending) if it enabled them.
	// Or reset common states here if material->bind() is not perfectly symmetrical.
	// glDisable(GL_BLEND); // If material enabled it and didn't disable.
	// glEnable(GL_CULL_FACE); // If material disabled it and it should be on.
	// glCullFace(GL_BACK);
}

#ifndef SKIP_IMGUI

void Renderer::showUI()
{
	ImGui::Checkbox("Wireframe", &render_wireframe);
	ImGui::Checkbox("Boundaries", &render_boundaries);

	ImGui::SliderFloat("Shininess", &SCN::Material::default_material.shininess, 1.0f, 100.0f, "Shininess = %.1f");

	float current_bias = SHADOW::getShadowBias();
	if (ImGui::SliderFloat("Shadow Bias", &current_bias, 0.0f, 0.05f, "Bias = %.5f")) {
		SHADOW::setShadowBias(current_bias);
	}

	bool current_cull = SHADOW::getFrontFaceCulling();
	if (ImGui::Checkbox("Front Face Culling (Shadows)", &current_cull)) {
		SHADOW::setFrontFaceCulling(current_cull);
	}

	// The G-Buffer Contents section has been removed.
}

#else
void Renderer::showUI() {}
#endif
