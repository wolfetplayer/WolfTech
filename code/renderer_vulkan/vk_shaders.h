// vk_shaders.h -- loads precompiled SPIR-V modules from shaders/Compiled/*.c (single_texture is vkQuake3, GPLv2-or-later; world_lightmap is ours).

#ifndef VK_SHADERS_H_
#define VK_SHADERS_H_

void vk_loadShaderModules( void );
void vk_destroyShaderModules( void );

extern VkShaderModule vk_single_texture_vs;
extern VkShaderModule vk_single_texture_fs;

extern VkShaderModule vk_world_lightmap_vs;
extern VkShaderModule vk_world_lightmap_fs;

#endif // VK_SHADERS_H_
