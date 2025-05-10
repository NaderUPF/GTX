#include "shadow.h"
#include "../gfx/mesh.h"
#include "../gfx/shader.h"
#include "renderer.h" 

namespace SHADOW {
    // A3: TASK 1 - Array of shadow FBOs and textures
    GFX::FBO* shadow_fbos[MAX_SHADOW_MAPS] = { nullptr };
    GFX::Texture* shadow_textures[MAX_SHADOW_MAPS] = { nullptr };
    int active_shadow_count = 0;

    // A3: TASK 2 - Array of light cameras
    Camera light_cameras[MAX_SHADOW_MAPS];

    // A3: TASK 5 - Shadow tweak settings
    static float shadow_bias = 0.005f;
    static bool front_face_culling = false;
    bool getFrontFaceCulling() { return front_face_culling; }

    // A3: TASK 1 - Initialize specific shadow map
    void initShadowMap(int resolution, int index) {
        if (index < 0 || index >= MAX_SHADOW_MAPS) return;

        if (shadow_fbos[index]) delete shadow_fbos[index];
        shadow_fbos[index] = new GFX::FBO();
        shadow_fbos[index]->setDepthOnly(resolution, resolution);
    }

    // A3: TASK 5 - Shadow parameters
    float getShadowBias() { return shadow_bias; }
    void setShadowBias(float bias) { shadow_bias = bias; }
    void setFrontFaceCulling(bool enabled) { front_face_culling = enabled; }

    // A3: TASK 6 - Accessors with index
    GFX::Texture* getShadowMap(int index) {
        return (index >= 0 && index < MAX_SHADOW_MAPS) ? shadow_fbos[index]->depth_texture : nullptr;
    }

    Camera& getLightCamera(int index) {
        static Camera dummy;
        return (index >= 0 && index < MAX_SHADOW_MAPS) ? light_cameras[index] : dummy;
    }

    // A3: TASK 2 - Configure camera for specific light index
    void setupLightCamera(SCN::LightEntity* light, int index) {
        if (index < 0 || index >= MAX_SHADOW_MAPS) return;

        Matrix44 model = light->root.getGlobalMatrix();
        Vector3 pos = model.getTranslation();
        Vector3 dir = light->root.model.frontVector();

        light_cameras[index].lookAt(pos, model * vec3(0.0f, 0.0f, -1.0f), Vector3(0.0f, 1.0f, 0.0f));

        if (light->light_type == SCN::eLightType::SPOT) {
            float fov = light->cone_info.y * 2.0f;
            light_cameras[index].setPerspective(fov, 1.0f, light->near_distance, light->max_distance);
        }
        else if (light->light_type == SCN::eLightType::DIRECTIONAL) {
            float half_size = light->area / 2.0f;
            light_cameras[index].setOrthographic(-half_size, half_size, -half_size, half_size,
                light->near_distance, light->max_distance);
        }
    }

    // A3: TASK 3 - Render to specific shadow map
    void renderToShadowMap(SCN::Scene* scene, const std::vector<sDrawCommand>& draw_commands,
        SCN::LightEntity* light, int index) {
        if (index < 0 || index >= MAX_SHADOW_MAPS || !shadow_fbos[index]) return;

        shadow_fbos[index]->bind();
        glClear(GL_DEPTH_BUFFER_BIT);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glEnable(GL_DEPTH_TEST);

        setupLightCamera(light, index);

        glEnable(GL_CULL_FACE);
        glCullFace(front_face_culling ? GL_FRONT : GL_BACK);

        GFX::Shader* shader = GFX::Shader::Get("plain");
        shader->enable();
        shader->setUniform("u_viewprojection", light_cameras[index].viewprojection_matrix);

        for (const auto& cmd : draw_commands) {
            if (cmd.is_transparent) continue;
            shader->setUniform("u_model", cmd.model);
            cmd.mesh->render(GL_TRIANGLES);
        }

        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        shader->disable();
        shadow_fbos[index]->unbind();
        glCullFace(GL_BACK);
    }

    // A3: TASK 7 - Render shadows for all valid lights
    void renderAllShadowMaps(SCN::Scene* scene, const std::vector<sDrawCommand>& draw_calls,
        const std::vector<SCN::LightEntity*>& lights) {
        active_shadow_count = 0;

        for (SCN::LightEntity* light : lights) {
            if (active_shadow_count >= MAX_SHADOW_MAPS) break;

            if (light->light_type == SCN::eLightType::SPOT ||
                light->light_type == SCN::eLightType::DIRECTIONAL) {

                initShadowMap(1024, active_shadow_count);
                setupLightCamera(light, active_shadow_count);
                renderToShadowMap(scene, draw_calls, light, active_shadow_count);
                active_shadow_count++;
            }
        }
    }
}