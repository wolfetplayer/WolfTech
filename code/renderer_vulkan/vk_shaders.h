// vk_shaders.h -- loads precompiled single-texture SPIR-V modules (shaders/Compiled/*.c, vkQuake3 byte arrays, GPLv2-or-later).

#ifndef VK_SHADERS_H_
#define VK_SHADERS_H_

void vk_loadShaderModules( void );
void vk_destroyShaderModules( void );

extern VkShaderModule vk_single_texture_vs;
extern VkShaderModule vk_single_texture_fs;

#endif // VK_SHADERS_H_
