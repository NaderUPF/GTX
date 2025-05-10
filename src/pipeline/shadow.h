#pragma once

#include "../gfx/fbo.h"     
#include "../gfx/texture.h" 
#include "camera.h"         
#include "light.h"         
#include "scene.h"          
#include "renderer.h"

namespace SHADOW {
    // A3: TASK 1 - Array of shadow framebuffers and textures
    const int MAX_SHADOW_MAPS = 4;  // Maximum supported shadow-casting lights
    extern GFX::FBO* shadow_fbos[MAX_SHADOW_MAPS];
    extern GFX::Texture* shadow_textures[MAX_SHADOW_MAPS];

    // A3: TASK 2 - Array of light cameras
    extern Camera light_cameras[MAX_SHADOW_MAPS];
    extern int active_shadow_count;  // Number of actually used shadow maps

    // A3: TASK 5 - Shadow parameters (configurable via UI)
    float getShadowBias();
    void setShadowBias(float bias);
    void setFrontFaceCulling(bool enabled);
    bool getFrontFaceCulling();

    // A3: TASK 1 - Init FBO and depth texture for specific index
    void initShadowMap(int resolution, int index);

    // A3: TASK 2 - Setup camera for specific light index
    void setupLightCamera(SCN::LightEntity* light, int index);

    // A3: TASK 3 - Render scene to specific shadow map
    void renderToShadowMap(SCN::Scene* scene,
        const std::vector<sDrawCommand>& draw_commands,
        SCN::LightEntity* light,
        int index);

    // A3: TASK 6 - Access to pass to shaders
    GFX::Texture* getShadowMap(int index);
    Camera& getLightCamera(int index);

    // A3: TASK 7 - Multi-shadow support
    void renderAllShadowMaps(SCN::Scene* scene,
        const std::vector<sDrawCommand>& draw_calls,
        const std::vector<SCN::LightEntity*>& lights);
}