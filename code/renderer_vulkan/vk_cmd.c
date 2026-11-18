// vk_cmd.c -- adapted from vkQuake3 (GPLv2-or-later).

#include "VKimpl.h"
#include "vk_instance.h"
#include "vk_cmd.h"

void vk_create_command_pool( VkCommandPool *pPool ) {
	VkCommandPoolCreateInfo desc;
	desc.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	desc.queueFamilyIndex = vk.queue_family_index;

	VK_CHECK( qvkCreateCommandPool( vk.device, &desc, NULL, pPool ) );
}

void vk_create_command_buffer( VkCommandPool pool, VkCommandBuffer *pBuf ) {
	VkCommandBufferAllocateInfo alloc_info;
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.commandPool = pool;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = 1;
	VK_CHECK( qvkAllocateCommandBuffers( vk.device, &alloc_info, pBuf ) );
}

void vk_destroy_commands( void ) {
	ri.Printf( PRINT_ALL, " Free command buffers: vk.command_buffer. \n" );
	qvkFreeCommandBuffers( vk.device, vk.command_pool, 1, &vk.command_buffer );
	ri.Printf( PRINT_ALL, " Destroy command pool: vk.command_pool. \n" );
	qvkDestroyCommandPool( vk.device, vk.command_pool, NULL );
}

void record_image_layout_transition(
		VkCommandBuffer cmdBuf,
		VkImage image,
		VkImageAspectFlags image_aspect_flags,
		VkAccessFlags src_access_flags,
		VkImageLayout old_layout,
		VkAccessFlags dst_access_flags,
		VkImageLayout new_layout ) {

	VkImageMemoryBarrier barrier;
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.pNext = NULL;
	barrier.srcAccessMask = src_access_flags;
	barrier.dstAccessMask = dst_access_flags;
	barrier.oldLayout = old_layout;
	barrier.newLayout = new_layout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = image_aspect_flags;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

	qvkCmdPipelineBarrier( cmdBuf, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 0, NULL, 1, &barrier );
}
