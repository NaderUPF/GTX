#include "gbuffer.h"
#include "../gfx/gfx.h" 
#include "../core/includes.h" 

namespace SCN {

GBuffer::GBuffer() {
    // gbuffer_fbo is default constructed
}

GBuffer::~GBuffer() {
    // gbuffer_fbo destructor will handle its cleanup
}

void GBuffer::create(int width, int height) {
    // If the FBO was previously created, free its textures to prevent leaks
    if (gbuffer_fbo.fbo_id != 0) { 
        gbuffer_fbo.freeTextures(); 
    }

    // Create the FBO with 2 RGBA8 color textures and a depth texture
    bool success = gbuffer_fbo.create(width,
                       height,
                       2,                // num_textures: Albedo and Normal_Material
                       GL_RGBA,          
                       GL_UNSIGNED_BYTE, 
                       true);            // use_depth_texture
    
    if (!success) {
        // Optionally log an error here if creation failed
    }
    
    GFX::checkGLErrors(); 
}

// Optional: if you need an explicit way to release resources before GBuffer destructor
void GBuffer::clearResources() {
    gbuffer_fbo.freeTextures();
    gbuffer_fbo.fbo_id = 0; 
}

} // namespace SCN 