#ifndef GBUFFER_H
#define GBUFFER_H

#include "../gfx/fbo.h" // For GFX::FBO

// Forward declaration if GFX types are not fully included by fbo.h
namespace GFX {
    struct FBO;
}

namespace SCN {
    class GBuffer {
    public:
        GFX::FBO gbuffer_fbo;

        // Creates the G-Buffer with specified dimensions.
        // Other parameters (num_textures, format, type, depth) are based on the assignment's requirements.
        void create(int width, int height);

        // Destroys the FBO resources
        void free();

        // Constructor and Destructor
        GBuffer();
        ~GBuffer();

        // Declaration for clearResources
        void clearResources();
    };
} // namespace SCN

#endif // GBUFFER_H 