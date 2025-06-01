#pragma once
#include "scene.h"
#include "prefab.h"
#include "light.h"
#include "gbuffer.h" // Assuming SCN::GBuffer is defined here

//forward declarations
class Camera;
class Skeleton; // If still used, otherwise can be removed if not part of Renderer's direct interface/members

enum class ShaderType {
	Unknown,
	GBufferFill,
	Phong,
	Other
};

namespace GFX {
	class Shader;
	class Mesh;
	class FBO;
	class Texture; // Added GFX::Texture forward declaration as skybox_cubemap is GFX::Texture*
}

// A1: TASK 1 - Render Call struct (or sDrawCommand if already named that way)
struct sDrawCommand { // Ensure this matches existing struct if any
	GFX::Mesh* mesh;
	SCN::Material* material;
	Matrix44 model;
	float camera_distance;
	bool is_transparent;
};


namespace SCN {

	class Prefab; // Already forward declared by prefab.h but good practice if used directly
	class Material; // Already forward declared by prefab.h (as Material is part of Prefab)
	class Scene;    // Already forward declared by scene.h
	class Node;     // Part of SCN::BaseEntity in scene.h, usually not needed as separate forward here
	class LightEntity; // Already forward declared by light.h

	// This class is in charge of rendering anything in our system.
	// Separating the render from anything else makes the code cleaner
	class Renderer
	{
	public:
		bool render_wireframe;
		bool render_boundaries;

		GFX::Texture* skybox_cubemap; // Make sure GFX::Texture is known (forward declared or Texture.h included)

		SCN::Scene* scene; // Current scene pointer

		// Members for G-Buffer
		SCN::GBuffer gbuffer;

		// Potentially for a future assignment - ensure they are initialized
		GFX::FBO* reflection_fbo;       // Example member
		GFX::Texture* reflection_texture; // Example member

		// New FBO for light accumulation
		GFX::FBO lighting_fbo;

		//constructor
		Renderer(const char* shaders_atlas_filename);

		//setup scene properties
		void setupScene();

		// Shader uniform uploading
		void uploadLights(GFX::Shader* shader, const std::vector<SCN::LightEntity*>& lights);

		// Scene parsing
		void parseSceneEntities(SCN::Scene* scene_ptr, Camera* cam, std::vector<sDrawCommand>& commands_list, std::vector<SCN::LightEntity*>& lights_list);
		void parseNodes(SCN::Node* node, Camera* cam, std::vector<sDrawCommand>& commands_list); // Helper for parseSceneEntities

		// Main rendering loop
		void renderScene(SCN::Scene* scene, Camera* camera);

		// Renders a single mesh with a material (and optional override shader)
		void renderMeshWithMaterial(const Matrix44 model, GFX::Mesh* mesh, SCN::Material* material, GFX::Shader* override_shader = nullptr, ShaderType shader_type = ShaderType::Unknown);

		// For UI
		void showUI();

	private:
		// Helper for skybox rendering, called during G-Buffer pass
		void renderSkybox(GFX::Texture* cubemap);

		// New private helper methods for pipeline stages
		void renderGBufferPass(Camera* camera, const std::vector<sDrawCommand>& opaque_commands);
		void renderDeferredLightingPass(Camera* camera);
		void renderTransparentPass(Camera* camera, const std::vector<sDrawCommand>& transparent_commands, const std::vector<SCN::LightEntity*>& lights);
		void compositeLightingToScreen();
	};

};

// Screen dimension macros - consider moving to a config.h or constants file if used widely
// #define SCREEN_WIDTH  640 // Example values, ensure these are defined appropriately if used
// #define SCREEN_HEIGHT 480 // in gbuffer.create() or elsewhere without direct CORE::getWindowSize()