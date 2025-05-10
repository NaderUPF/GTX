#pragma once
#include "scene.h"
#include "prefab.h"

#include "light.h"

//forward declarations
class Camera;
class Skeleton;

namespace GFX {
	class Shader;
	class Mesh;
	class FBO;
}

// A1: TASK 1 - Render Call struct
struct sDrawCommand {
	GFX::Mesh* mesh;
	SCN::Material* material;
	Matrix44 model;
	float camera_distance;
	bool is_transparent;
};

namespace SCN {

	class Prefab;
	class Material;
	class Scene;
	class Node;
	class LightEntity;

	// This class is in charge of rendering anything in our system.
	// Separating the render from anything else makes the code cleaner
	class Renderer
	{
	public:
		bool render_wireframe;
		bool render_boundaries;

		GFX::Texture* skybox_cubemap;

		SCN::Scene* scene;

		// Members for G-Buffer
		SCN::GBuffer gbuffer;

		// Potentially for a future assignment (A4) - ensure they are initialized
		GFX::FBO* reflection_fbo; 
		GFX::Texture* reflection_texture;

		//updated every frame
		Renderer(const char* shaders_atlas_filename );

		//just to be sure we have everything ready for the rendering
		void setupScene();

		//add here your functions
		//...
		void uploadLights(GFX::Shader* shader, const std::vector<SCN::LightEntity*>& lights_list_ref);

		void parseSceneEntities(SCN::Scene* scene_ptr, Camera* cam, std::vector<sDrawCommand>& commands_list, std::vector<SCN::LightEntity*>& lights_list);
		void parseNodes(SCN::Node* node, Camera* cam, std::vector<sDrawCommand>& commands_list);

		//renders several elements of the scene
		void renderScene(SCN::Scene* scene, Camera* camera);

		//render the skybox
		void renderSkybox(GFX::Texture* cubemap);

		//to render one mesh given its material and transformation matrix
		void renderMeshWithMaterial(const Matrix44 model, GFX::Mesh* mesh, SCN::Material* material, GFX::Shader* shader_override);

		void showUI();
	};

};

#define SCREEN_WIDTH  640
#define SCREEN_HEIGHT 480