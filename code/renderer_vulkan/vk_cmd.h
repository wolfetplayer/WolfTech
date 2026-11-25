// vk_cmd.h -- adapted from vkQuake3 (GPLv2-or-later).

#ifndef VK_CMD_H_
#define VK_CMD_H_

void vk_create_command_pool( VkCommandPool *pPool );
void vk_create_command_buffers( VkCommandPool pool, VkCommandBuffer *pBufs, uint32_t count );
void vk_destroy_commands( void );

void vk_reset_bind_cache( void );
void vk_bind_pipeline_cached( VkPipeline pipeline );
void vk_bind_descriptor_set_cached( VkPipelineLayout layout, uint32_t firstSet, VkDescriptorSet set,
	uint32_t dynamicOffsetCount, const uint32_t *pDynamicOffsets );

void record_image_layout_transition(
	VkCommandBuffer cmdBuf,
	VkImage image,
	VkImageAspectFlags image_aspect_flags,
	VkAccessFlags src_access_flags,
	VkImageLayout old_layout,
	VkAccessFlags dst_access_flags,
	VkImageLayout new_layout );

#endif // VK_CMD_H_
