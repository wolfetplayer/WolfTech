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

// Entity GPU vertex-lerp (MD3/MDC); pair with single_texture_fs/multi_texture_fs, no fragment-shader change needed. See vk_getPipelineEntityLerp/vk_getPipelineEntityLerpMultiTex.
extern VkShaderModule vk_entity_lerp_vs;
extern VkShaderModule vk_entity_lerp_multitex_vs;

// Entity GPU bone-skinning (MDS/MDM); same fragment-shader reuse. See vk_getPipelineEntitySkin/vk_getPipelineEntitySkinMultiTex.
extern VkShaderModule vk_entity_skin_vs;
extern VkShaderModule vk_entity_skin_multitex_vs;

#endif // VK_SHADERS_H_
