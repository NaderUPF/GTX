#include "material.h"

#include "../core/includes.h"
#include "../gfx/texture.h"
#include "../gfx/shader.h"

using namespace SCN;

std::map<std::string, Material*> Material::sMaterials;
uint32 Material::s_last_index = 0;
Material Material::default_material;

const char* SCN::texture_channel_str[] = { "ALBEDO","EMISSIVE","OPACITY","METALLIC_ROUGHNESS","OCCLUSION","NORMALMAP" };


Material* Material::Get(const char* name)
{
	assert(name);
	std::map<std::string, Material*>::iterator it = sMaterials.find(name);
	if (it != sMaterials.end())
		return it->second;
	return NULL;
}

void Material::registerMaterial(const char* name)
{
	this->name = name;
	sMaterials[name] = this;
}

Material::~Material()
{
	if (name.size())
	{
		auto it = sMaterials.find(name);
		if (it != sMaterials.end())
			sMaterials.erase(it);
	}
}

void Material::Release()
{
	std::vector<Material*>mats;

	for (auto mp : sMaterials)
	{
		Material* m = mp.second;
		mats.push_back(m);
	}

	for (Material* m : mats)
	{
		delete m;
	}
	sMaterials.clear();
}

void Material::bind(GFX::Shader* shader) {
    // Configure OpenGL state (blending, culling)
    if (alpha_mode == SCN::eAlphaMode::BLEND) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    else {
        glDisable(GL_BLEND);
    }
    if (two_sided)
        glDisable(GL_CULL_FACE);
    else
        glEnable(GL_CULL_FACE);
    assert(glGetError() == GL_NO_ERROR);

    // Bind textures with uniform checks
    GFX::Texture* albedo_tex = textures[SCN::eTextureChannel::ALBEDO].texture;
    if (!albedo_tex)
        albedo_tex = GFX::Texture::getWhiteTexture();

    if (shader->IsUniform("u_albedo_texture"))
        shader->setUniform("u_albedo_texture", albedo_tex, 0);

    GFX::Texture* normal_tex = textures[SCN::eTextureChannel::NORMALMAP].texture;
    if (normal_tex && shader->IsUniform("u_normal_material_texture"))
        shader->setUniform("u_normal_material_texture", normal_tex, 1);

    GFX::Texture* mr_tex = textures[SCN::eTextureChannel::METALLIC_ROUGHNESS].texture;
    if (mr_tex && shader->IsUniform("u_metallic_roughness_texture"))
        shader->setUniform("u_metallic_roughness_texture", mr_tex, 2);

    if (shader->IsUniform("u_roughness_factor"))
        shader->setUniform("u_roughness_factor", roughness_factor);

    if (shader->IsUniform("u_metallic_factor"))
        shader->setUniform("u_metallic_factor", metallic_factor);

    if (shader->IsUniform("u_color"))
        shader->setUniform("u_color", color);

    if (shader->IsUniform("u_shininess"))
        shader->setUniform("u_shininess", shininess);

    // Use albedo_tex as fallback texture uniform if shader expects it
    if (shader->IsUniform("u_texture"))
        shader->setUniform("u_texture", albedo_tex, 0);

    float cutoff = (alpha_mode == SCN::eAlphaMode::MASK) ? alpha_cutoff : 0.001f;
    if (shader->IsUniform("u_alpha_cutoff"))
        shader->setUniform("u_alpha_cutoff", cutoff);
}
