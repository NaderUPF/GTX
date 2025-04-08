#include "renderer.h"

#include <algorithm> //sort

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

#include "scene.h"

// A1: TASK 1 - Render Call struct
struct sDrawCommand {
	GFX::Mesh* mesh;
	SCN::Material* material;
	Matrix44 model;
	float camera_distance;
	bool is_transparent;
};

std::vector<sDrawCommand> draw_command_list;
std::vector<SCN::LightEntity*> light_list; // A2: TASK 1 - Light List

using namespace SCN;

//some globals
GFX::Mesh sphere;

Renderer::Renderer(const char* shader_atlas_filename)
{
	render_wireframe = false;
	render_boundaries = false;
	scene = nullptr;
	skybox_cubemap = nullptr;

	if (!GFX::Shader::LoadAtlas(shader_atlas_filename))
		exit(1);
	GFX::checkGLErrors();

	sphere.createSphere(1.0f);
	sphere.uploadToVRAM();
}

void Renderer::setupScene()
{
	if (scene->skybox_filename.size())
		skybox_cubemap = GFX::Texture::Get(std::string(scene->base_folder + "/" + scene->skybox_filename).c_str());
	else
		skybox_cubemap = nullptr;
}

// A1: TASK 2 - Parse scene and generate render calls
////CODI PROFE
void parseNodes(SCN::Node* node, Camera* cam) {
	if (!node) {
		return;
	}
	if (node->mesh) {
		sDrawCommand draw_com;
		draw_com.mesh = node->mesh;
		draw_com.material = node->material;
		draw_com.model = node->getGlobalMatrix();

		Vector3 pos = draw_com.model.getTranslation();
		draw_com.camera_distance = (cam->eye - pos).length();

		draw_com.is_transparent = (node->material && node->material->alpha_mode != SCN::NO_ALPHA);

		draw_command_list.push_back(draw_com);
	}

	for (SCN::Node* child : node->children) {
		parseNodes(child, cam);
	}
}

////CODI PROFE
void parseNodes(SCN::Node* node, Camera* cam) {
	if (!node) {
		return;
	}
	if (node->mesh) {
		sDrawCommand draw_com;
		draw_com.mesh = node->mesh;
		draw_com.material = node->material;
		draw_com.model = node->getGlobalMatrix();

		draw_command_list.push_back(draw_com);
	}

	//for(SCN::Mo)
	//codi chat
	for (SCN::Node* child : node->children) {
		parseNodes(child, cam);
	}
}

void Renderer::parseSceneEntities(SCN::Scene* scene, Camera* cam) {
	// HERE =====================
	// TODO: GENERATE RENDERABLES
	// ==========================

	// A1: TASK 2 - GENERATE RENDERABLES
	draw_command_list.clear(); // Avoid accumulation across frames
	light_list.clear(); // A2: TASK 1

	for (int i = 0; i < scene->entities.size(); i++) {
		BaseEntity* entity = scene->entities[i];

		if (!entity->visible) {
			continue;
		}

		// Store Prefab Entitys
		// ...
		//		Store Children Prefab Entities

		// Store Lights
		// ...

		////CODI PROFE
		if (entity->getType() == eEntityType::PREFAB) {
			PrefabEntity* prefab_ent = (PrefabEntity*)entity;
			parseNodes(&prefab_ent->root, cam);
		}
		else if (entity->getType() == eEntityType::LIGHT) {
			light_list.push_back((LightEntity*)entity);
		}
	}
	
}

// A2: TASK 3 - Upload light uniforms
void Renderer::uploadLights(GFX::Shader* shader) {
	int count = (int)light_list.size();
	if (count > 10) count = 10;

	vec3 light_positions[10];
	vec3 light_colors[10];
	float light_intensity[10];
	vec3 light_direction[10];
	int light_type[10];

	for (int i = 0; i < count; ++i) {
		LightEntity* light = light_list[i];
		light_positions[i] = light->root.getGlobalMatrix().getTranslation();
		light_colors[i] = light->color;
		light_intensity[i] = light->intensity;
		light_direction[i] = light->root.model.frontVector();
		light_type[i] = (int)light->light_type;
	}

	shader->setUniform("u_num_lights", count);
	shader->setUniform3Array("u_light_positions", (float*)light_positions, count);
	shader->setUniform3Array("u_light_colors", (float*)light_colors, count);
	shader->setUniform1Array("u_light_intensity", (float*)light_intensity, count);
	shader->setUniform3Array("u_light_direction", (float*)light_direction, count);
	shader->setUniform1Array("u_light_type", light_type, count);
}

void Renderer::renderScene(SCN::Scene* scene, Camera* camera)
{
	this->scene = scene;
	setupScene();

	parseSceneEntities(scene, camera);

	//set the clear color (the background color)
	glClearColor(scene->background_color.x, scene->background_color.y, scene->background_color.z, 1.0);

	// Clear the color and the depth buffer
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	GFX::checkGLErrors();

	//render skybox
	if (skybox_cubemap)
		renderSkybox(skybox_cubemap);

	// HERE =====================
	// TODO: RENDER RENDERABLES
	// ==========================

	// A1: TASK 4 - ORDERING RENDER CALLS

	std::vector<sDrawCommand> opaque, transparent;
	for (auto& cmd : draw_command_list) {
		if (cmd.is_transparent)
			transparent.push_back(cmd);
		else
			opaque.push_back(cmd);
	}

	// Opaque: sort near to far
	std::sort(opaque.begin(), opaque.end(), [](const sDrawCommand& a, const sDrawCommand& b) {
		return a.camera_distance < b.camera_distance;
		});

	// Transparent: sort far to near
	std::sort(transparent.begin(), transparent.end(), [](const sDrawCommand& a, const sDrawCommand& b) {
		return a.camera_distance > b.camera_distance;
		});

	// A1: TASK 3 - RENDER RENDERABLES
	////CODI PROFE
	for (auto& command : opaque) {
		renderMeshWithMaterial(command.model, command.mesh, command.material);
	}
	for (auto& command : transparent) {
		renderMeshWithMaterial(command.model, command.mesh, command.material);
	}
}


void Renderer::renderSkybox(GFX::Texture* cubemap)
{
	Camera* camera = Camera::current;

	// Apply skybox necesarry config:
	// No blending, no dpeth test, we are always rendering the skybox
	// Set the culling aproppiately, since we just want the back faces
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

	if (render_wireframe)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	GFX::Shader* shader = GFX::Shader::Get("skybox");
	if (!shader)
		return;
	shader->enable();

	// Center the skybox at the camera, with a big sphere
	Matrix44 m;
	m.setTranslation(camera->eye.x, camera->eye.y, camera->eye.z);
	m.scale(10, 10, 10);
	shader->setUniform("u_model", m);

	// Upload camera uniforms
	shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
	shader->setUniform("u_camera_position", camera->eye);

	shader->setUniform("u_texture", cubemap, 0);

	sphere.render(GL_TRIANGLES);

	shader->disable();

	// Return opengl state to default
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glEnable(GL_DEPTH_TEST);
}

// Renders a mesh given its transform and material
void Renderer::renderMeshWithMaterial(const Matrix44 model, GFX::Mesh* mesh, SCN::Material* material)
{
	//in case there is nothing to do
	if (!mesh || !mesh->getNumVertices() || !material)
		return;
	assert(glGetError() == GL_NO_ERROR);

	//define locals to simplify coding
	//GFX::Shader* shader = NULL; //comment
	GFX::Shader* shader = GFX::Shader::Get("phong"); // A2: TASK 2 & 3

	Camera* camera = Camera::current;

	glEnable(GL_DEPTH_TEST);

	//chose a shader
	shader = GFX::Shader::Get("texture"); //comment
	//GFX::Shader* shader = GFX::Shader::Get("phong"); // A2: TASK 2 & 3

	assert(glGetError() == GL_NO_ERROR);

	//no shader? then nothing to render
	if (!shader)
		return;
	shader->enable();

	uploadLights(shader);

	material->bind(shader);

	////CODI PROFE
	//sending the lights
	vec3* light_positions = new vec3[light_list.size()];
	vec3* light_colors = new vec3[light_list.size()];
	float* light_intensity = new float[light_list.size()];
	vec3* light_direction = new vec3[light_list.size()];

	// ...
	int i = 0;
	for (LightEntity* light : light_list) {
		light_positions[i] = light->root.getGlobalMatrix().getTranslation();
		light_colors[i] = light->color;
		light_intensity[i] = light->intensity;
		light_direction[i] = light->root.model.frontVector();
		i++;
	}

	shader->setUniform3Array("u_light_positions", (float*)light_positions, min(light_list.size(), 10));
	shader->setUniform3Array("u_light_colors", (float*)light_colors, min(light_list.size(), 10));
	shader->setUniform1Array("u_light_intensity", (float*)light_intensity, min(light_list.size(), 10));
	shader->setUniform3Array("u_light_direction", (float*)light_direction, min(light_list.size(), 10));

	delete[] light_positions;
	delete[] light_colors;
	delete[] light_intensity;
	delete[] light_direction;

	//upload uniforms
	shader->setUniform("u_model", model);

	// Upload camera uniforms
	shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
	shader->setUniform("u_camera_position", camera->eye);

	// Upload time, for cool shader effects
	float t = getTime();
	shader->setUniform("u_time", t);

	// Render just the verticies as a wireframe
	if (render_wireframe)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	//do the draw call that renders the mesh into the screen
	mesh->render(GL_TRIANGLES);

	//disable shader
	shader->disable();

	//set the render state as it was before to avoid problems with future renders
	glDisable(GL_BLEND);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

#ifndef SKIP_IMGUI

void Renderer::showUI()
{

	ImGui::Checkbox("Wireframe", &render_wireframe);
	ImGui::Checkbox("Boundaries", &render_boundaries);

	//add here your stuff
	//...
	//float& shininess = SCN::Material::default_material.shininess;
	//ImGui::SliderFloat("Shininess", &shininess, 1.0f, 100.0f, "Shininess = %.1f");
	//shine???
	ImGui::SliderFloat("Shininess", &SCN::Material::default_material.shininess, 1.0f, 100.0f, "Shininess = %.1f");
}

#else
void Renderer::showUI() {}
#endif
