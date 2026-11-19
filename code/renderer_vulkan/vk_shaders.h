// vk_shaders.h -- loads precompiled SPIR-V modules from shaders/Compiled/*.c (single_texture started from vkQuake3, GPLv2-or-later, now modified; world_lightmap is ours).

#ifndef VK_SHADERS_H_
#define VK_SHADERS_H_

void vk_loadShaderModules( void );
void vk_destroyShaderModules( void );

extern VkShaderModule vk_single_texture_vs;
extern VkShaderModule vk_single_texture_fs;

extern VkShaderModule vk_world_lightmap_vs;
extern VkShaderModule vk_world_lightmap_fs;

extern VkShaderModule vk_multi_texture_vs;
extern VkShaderModule vk_multi_texture_fs;

// Hardware-gl_ClipDistance twins, used only when vk.features.shaderClipDistance is true (vk_pipelines.c).
extern VkShaderModule vk_single_texture_clip_vs;
extern VkShaderModule vk_world_lightmap_clip_vs;

#endif // VK_SHADERS_H_
