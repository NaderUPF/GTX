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

	#if defined(SCREEN_WIDTH) && defined(SCREEN_HEIGHT)
		if (SCREEN_WIDTH > 0 && SCREEN_HEIGHT > 0) {
			gbuffer.create(SCREEN_WIDTH, SCREEN_HEIGHT);
		} else {
			std::cerr << "Error: SCREEN_WIDTH or SCREEN_HEIGHT defined as zero or negative. Using fallback 800x600 for G-Buffer." << std::endl;
			gbuffer.create(800, 600);
		}
	#else
		std::cerr << "Error: SCREEN_WIDTH and SCREEN_HEIGHT are not defined. Using fallback 800x600 for G-Buffer." << std::endl;
		gbuffer.create(800, 600); 
	#endif
	
	GFX::checkGLErrors();
}

void Renderer::setupScene()
{
	if (this->scene && this->scene->skybox_filename.size()) {
		skybox_cubemap = GFX::Texture::Get(std::string(this->scene->base_folder + "/" + this->scene->skybox_filename).c_str());
	} else {
		skybox_cubemap = nullptr;
	}
}

// A1: TASK 2 - Parse scene and generate render calls
void parseNodes(SCN::Node* node, Camera* cam, std::vector<sDrawCommand>& commands_list) {
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
		} else {
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
		} else if (entity->getType() == SCN::eEntityType::LIGHT) {
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
		shader->setUniform3Array("u_light_colors",    reinterpret_cast<const float*>(light_colors), count);
		shader->setUniform1Array("u_light_intensity", light_intensity, count);
		shader->setUniform3Array("u_light_direction", reinterpret_cast<const float*>(light_direction), count);
		shader->setUniform1Array("u_light_type",      light_type_arr, count);
	}
}

void Renderer::renderScene(SCN::Scene* scene_ptr, Camera* camera)
{
	if (!scene_ptr || !camera) return; // Safety checks

	this->scene = scene_ptr; 
	setupScene(); // Skybox texture, reflection FBOs, potentially G-Buffer resize

	std::vector<sDrawCommand> local_draw_commands; 
	std::vector<SCN::LightEntity*> local_light_list;
	parseSceneEntities(scene_ptr, camera, local_draw_commands, local_light_list);

	// A3: TASK 7 - Render shadow maps for all shadowable lights (Done before G-Buffer)
	SHADOW::renderAllShadowMaps(scene_ptr, local_draw_commands, local_light_list);
	GFX::checkGLErrors();

	// START G-BUFFER PASS
	gbuffer.gbuffer_fbo.bind();
	GFX::checkGLErrors();

	// Clear the G-Buffer (color and depth)
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f); // Clear to black, alpha 0 for albedo, normal can be anything
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	GFX::checkGLErrors();

	// The skybox shader ("skybox.fs") was modified to output to gbuffer_albedo and gbuffer_normal.
	// It uses glDisable(GL_CULL_FACE) and enables depth test.
	if (skybox_cubemap) {
		renderSkybox(skybox_cubemap); // This uses the "skybox" shader from atlas
		GFX::checkGLErrors();
	}

	// Get the G-Buffer filling shader
	GFX::Shader* gbuffer_fill_shader = GFX::Shader::Get("gbuffer_fill");
	if (!gbuffer_fill_shader) {
		std::cerr << "GBuffer fill shader 'gbuffer_fill' not found!" << std::endl;
		gbuffer.gbuffer_fbo.unbind();
		// Render something to main framebuffer to indicate error or exit
		glClearColor(1.0f, 0.0f, 1.0f, 1.0); // Magenta error
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		return; 
	}
	
	// Separate opaque and transparent draw calls
	std::vector<sDrawCommand> opaque_commands, transparent_commands;
	for (const auto& cmd : local_draw_commands) {
		if (cmd.is_transparent)
			transparent_commands.push_back(cmd);
		else
			opaque_commands.push_back(cmd);
	}

	// Opaque: sort near to far for G-Buffer pass (helps with early-Z)
	std::sort(opaque_commands.begin(), opaque_commands.end(), [](const sDrawCommand& a, const sDrawCommand& b) {
		return a.camera_distance < b.camera_distance;
	});

	// Transparent: sort far to near (for later forward rendering)
	std::sort(transparent_commands.begin(), transparent_commands.end(), [](const sDrawCommand& a, const sDrawCommand& b) {
		return a.camera_distance > b.camera_distance;
	});
	
	// RENDER OPAQUE OBJECTS TO G-BUFFER
	// Depth test is already enabled from skybox or default state, and should be GL_LESS
	// Culling will be handled by material->bind() in renderMeshWithMaterial
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	for (const auto& command : opaque_commands) {
		// Use the gbuffer_fill_shader for opaque objects
		renderMeshWithMaterial(command.model, command.mesh, command.material, gbuffer_fill_shader);
	}
	GFX::checkGLErrors();

	gbuffer.gbuffer_fbo.unbind();
	// END G-BUFFER PASS

	// --- Main Framebuffer Rendering ---
	// Set the clear color for the main framebuffer (can be scene background or black if fully overwritten)
	glClearColor(scene_ptr->background_color.x, scene_ptr->background_color.y, scene_ptr->background_color.z, 1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clear main framebuffer
	GFX::checkGLErrors();

	// DEBUG: Render one of the G-Buffer textures to the screen to verify
	// This is a common way to test. You will replace this with the deferred lighting pass.
	if (gbuffer.gbuffer_fbo.color_textures[0] != nullptr && gbuffer.gbuffer_fbo.color_textures[0]->texture_id != 0) { // Check albedo texture
		GFX::Shader* quad_shader = GFX::Shader::Get("texture"); // Use a simple texture shader
		if (quad_shader) {
			glDisable(GL_DEPTH_TEST); // No depth test for full-screen quad
			quad_shader->enable();
			// Check if your toViewport takes a shader, or handles it internally.
			// If it handles internally: gbuffer.gbuffer_fbo.color_textures[0]->toViewport();
			// If it needs a shader:
			quad_shader->setUniform("u_color", vec4(1,1,1,1)); // Reset color modulation
			quad_shader->setUniform("u_texture", gbuffer.gbuffer_fbo.color_textures[0], 0);
			GFX::Mesh::getQuad()->render(GL_TRIANGLES); // Render a full-screen quad
			quad_shader->disable();
			glEnable(GL_DEPTH_TEST); // Re-enable for subsequent rendering
		} else {
			std::cerr << "Debug shader 'texture' not found for G-Buffer preview." << std::endl;
			// Alternative: gbuffer.gbuffer_fbo.color_textures[0]->toViewport(); if it sets up its own shader
		}
		GFX::checkGLErrors();
	}
	// Or render the second texture (Normals):
	// if (gbuffer.gbuffer_fbo.color_textures[1]) { ... similar logic for normals ...}


	// Render Transparent objects using Forward Rendering (after G-Buffer and lighting pass in full deferred pipeline)
	GFX::Shader* phong_shader = GFX::Shader::Get("phong"); 
	if (phong_shader) {
		glEnable(GL_BLEND); // Enable blending for transparents
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // Standard alpha blending
		glDepthMask(GL_FALSE); // Don't write to depth buffer for transparents if sorted far to near

		// For transparent objects using phong, lights should be uploaded once before this loop
		uploadLights(phong_shader, local_light_list);

		for (const auto& command : transparent_commands) {
			renderMeshWithMaterial(command.model, command.mesh, command.material, phong_shader);
		}
		glDepthMask(GL_TRUE); // Re-enable depth writes
		glDisable(GL_BLEND);  // Disable blending
	}
	GFX::checkGLErrors();
}


void Renderer::renderSkybox(GFX::Texture* cubemap)
{
	Camera* camera = Camera::current;
	if(!camera || !cubemap) return;

	// GL states for skybox rendering (targeting G-Buffer)
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST); // Skybox should test against existing depth
	glDepthMask(GL_TRUE);    // Skybox writes its own depth (important if it's not infinitely far)
	                         // If skybox is always at far plane, could be GL_LEQUAL and depth mask false after opaque.
							 // But here it's part of the G-Buffer geometry pass.
	glDisable(GL_CULL_FACE); // Render the inside of the sphere/cube

	if (render_wireframe)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	// The "skybox" shader in shader_atlas.glsl is now set up to output to
	// layout(location=0) gbuffer_albedo and layout(location=1) gbuffer_normal
	GFX::Shader* shader = GFX::Shader::Get("skybox"); 
	if (!shader) {
		std::cerr << "Skybox shader (for G-Buffer) not found!" << std::endl;
		if (render_wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		return;
	}
	shader->enable();

	Matrix44 m;
	m.setTranslation(camera->eye.x, camera->eye.y, camera->eye.z);
	// Scale must be large enough to contain the scene, but smaller than camera far_plane for depth test.
	// Using a fixed large scale. If camera->far_plane is smaller, this might get clipped.
	m.scale(camera->far_plane * 0.99f, camera->far_plane * 0.99f, camera->far_plane * 0.99f); 
	shader->setUniform("u_model", m); // basic.vs (used by skybox shader) uses u_model

	shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
	shader->setUniform("u_camera_position", camera->eye); // Used by skybox.fs

	shader->setUniform("u_texture", cubemap, 0); // Cubemap texture

	if(sphere.getNumVertices() > 0) sphere.render(GL_TRIANGLES); else std::cerr << "Error: sphere mesh not ready for skybox" << std::endl;

	shader->disable();

	// Restore default GL states
	if (render_wireframe)
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	// glEnable(GL_CULL_FACE); // Re-enable culling if it's default for other objects.
	                         // Material::bind() should handle culling per object.
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
	shader_to_use->setUniform("u_camera_position", camera->eye);
	shader_to_use->setUniform("u_time", getTime());

	// Conditional uploads: Only for relevant shaders (e.g., "phong" or lighting shaders)
	// The G-Buffer fill shader ("gbuffer_fill") does not use lights or shadow maps directly.
	GFX::Shader* phong_shader_ptr = GFX::Shader::Get("phong");
	if (shader_to_use == phong_shader_ptr) { 
		// Light uniforms are uploaded by renderScene before rendering transparents.
		// Shadow uniforms are uploaded here if the shader is phong (which uses them).
		if (shader_to_use->IsUniform("u_shadow_map[0]")) { 
			for (int i = 0; i < SHADOW::active_shadow_count; ++i) {
				std::string shadow_map_name = "u_shadow_map[" + std::to_string(i) + "]";
				std::string shadow_vp_name = "u_shadow_vp[" + std::to_string(i) + "]";
				shader_to_use->setUniform(shadow_map_name.c_str(), SHADOW::getShadowMap(i), 8 + i); 
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

	if (ImGui::CollapsingHeader("G-Buffer Contents")) {
        if (gbuffer.gbuffer_fbo.color_textures[0] != nullptr && gbuffer.gbuffer_fbo.color_textures[0]->texture_id != 0) {
            ImGui::Text("Albedo");
            ImGui::Image(reinterpret_cast<void*>(static_cast<intptr_t>(gbuffer.gbuffer_fbo.color_textures[0]->texture_id)), ImVec2(256, 256), ImVec2(0,1), ImVec2(1,0));
        }
        if (gbuffer.gbuffer_fbo.color_textures[1] != nullptr && gbuffer.gbuffer_fbo.color_textures[1]->texture_id != 0) {
            ImGui::Text("Normals");
            ImGui::Image(reinterpret_cast<void*>(static_cast<intptr_t>(gbuffer.gbuffer_fbo.color_textures[1]->texture_id)), ImVec2(256, 256), ImVec2(0,1), ImVec2(1,0));
        }
        if (gbuffer.gbuffer_fbo.depth_texture != nullptr && gbuffer.gbuffer_fbo.depth_texture->texture_id != 0) {
             ImGui::Text("Depth Texture");
             ImGui::Image(reinterpret_cast<void*>(static_cast<intptr_t>(gbuffer.gbuffer_fbo.depth_texture->texture_id)), ImVec2(256, 256), ImVec2(0,1), ImVec2(1,0));
        }
    }
}

#else
void Renderer::showUI() {}
#endif
