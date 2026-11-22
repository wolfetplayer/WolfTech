// vk_shaders.c -- creates VkShaderModules from shaders/Compiled/*.c.

#include "VKimpl.h"
#include "vk_instance.h"
#include "vk_shaders.h"

VkShaderModule vk_single_texture_vs;
VkShaderModule vk_single_texture_fs;

VkShaderModule vk_world_lightmap_vs;
VkShaderModule vk_world_lightmap_fs;

VkShaderModule vk_multi_texture_vs;
VkShaderModule vk_multi_texture_fs;

VkShaderModule vk_single_texture_clip_vs;
VkShaderModule vk_world_lightmap_clip_vs;

VkShaderModule vk_entity_lerp_vs;
VkShaderModule vk_entity_lerp_multitex_vs;

VkShaderModule vk_entity_skin_vs;
VkShaderModule vk_entity_skin_multitex_vs;

static void create_shader_module( const unsigned char *bytes, int count, VkShaderModule *pModule ) {
	VkShaderModuleCreateInfo desc;
	desc.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.codeSize = (size_t)count;
	desc.pCode = (const uint32_t *)bytes;
	VK_CHECK( qvkCreateShaderModule( vk.device, &desc, NULL, pModule ) );
}

void vk_loadShaderModules( void ) {
	extern unsigned char single_texture_vert_spv[];
	extern int single_texture_vert_spv_size;
	extern unsigned char single_texture_frag_spv[];
	extern int single_texture_frag_spv_size;
	extern unsigned char world_lightmap_vert_spv[];
	extern int world_lightmap_vert_spv_size;
	extern unsigned char world_lightmap_frag_spv[];
	extern int world_lightmap_frag_spv_size;
	extern unsigned char multi_texture_vert_spv[];
	extern int multi_texture_vert_spv_size;
	extern unsigned char multi_texture_frag_spv[];
	extern int multi_texture_frag_spv_size;
	extern unsigned char single_texture_clip_vert_spv[];
	extern int single_texture_clip_vert_spv_size;
	extern unsigned char world_lightmap_clip_vert_spv[];
	extern int world_lightmap_clip_vert_spv_size;
	extern unsigned char entity_lerp_vert_spv[];
	extern int entity_lerp_vert_spv_size;
	extern unsigned char entity_lerp_multitex_vert_spv[];
	extern int entity_lerp_multitex_vert_spv_size;
	extern unsigned char entity_skin_vert_spv[];
	extern int entity_skin_vert_spv_size;
	extern unsigned char entity_skin_multitex_vert_spv[];
	extern int entity_skin_multitex_vert_spv_size;

	create_shader_module( single_texture_vert_spv, single_texture_vert_spv_size, &vk_single_texture_vs );
	create_shader_module( single_texture_frag_spv, single_texture_frag_spv_size, &vk_single_texture_fs );
	create_shader_module( world_lightmap_vert_spv, world_lightmap_vert_spv_size, &vk_world_lightmap_vs );
	create_shader_module( world_lightmap_frag_spv, world_lightmap_frag_spv_size, &vk_world_lightmap_fs );
	create_shader_module( multi_texture_vert_spv, multi_texture_vert_spv_size, &vk_multi_texture_vs );
	create_shader_module( multi_texture_frag_spv, multi_texture_frag_spv_size, &vk_multi_texture_fs );
	create_shader_module( single_texture_clip_vert_spv, single_texture_clip_vert_spv_size, &vk_single_texture_clip_vs );
	create_shader_module( world_lightmap_clip_vert_spv, world_lightmap_clip_vert_spv_size, &vk_world_lightmap_clip_vs );
	create_shader_module( entity_lerp_vert_spv, entity_lerp_vert_spv_size, &vk_entity_lerp_vs );
	create_shader_module( entity_lerp_multitex_vert_spv, entity_lerp_multitex_vert_spv_size, &vk_entity_lerp_multitex_vs );
	create_shader_module( entity_skin_vert_spv, entity_skin_vert_spv_size, &vk_entity_skin_vs );
	create_shader_module( entity_skin_multitex_vert_spv, entity_skin_multitex_vert_spv_size, &vk_entity_skin_multitex_vs );
}

void vk_destroyShaderModules( void ) {
	qvkDestroyShaderModule( vk.device, vk_single_texture_vs, NULL );
	qvkDestroyShaderModule( vk.device, vk_single_texture_fs, NULL );
	qvkDestroyShaderModule( vk.device, vk_world_lightmap_vs, NULL );
	qvkDestroyShaderModule( vk.device, vk_world_lightmap_fs, NULL );
	qvkDestroyShaderModule( vk.device, vk_multi_texture_vs, NULL );
	qvkDestroyShaderModule( vk.device, vk_multi_texture_fs, NULL );
	qvkDestroyShaderModule( vk.device, vk_single_texture_clip_vs, NULL );
	qvkDestroyShaderModule( vk.device, vk_world_lightmap_clip_vs, NULL );
	qvkDestroyShaderModule( vk.device, vk_entity_lerp_vs, NULL );
	qvkDestroyShaderModule( vk.device, vk_entity_lerp_multitex_vs, NULL );
	qvkDestroyShaderModule( vk.device, vk_entity_skin_vs, NULL );
	qvkDestroyShaderModule( vk.device, vk_entity_skin_multitex_vs, NULL );
}
