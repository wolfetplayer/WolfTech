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

void vk_create_command_buffers( VkCommandPool pool, VkCommandBuffer *pBufs, uint32_t count ) {
	VkCommandBufferAllocateInfo alloc_info;
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.commandPool = pool;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = count;
	VK_CHECK( qvkAllocateCommandBuffers( vk.device, &alloc_info, pBufs ) );
}

void vk_destroy_commands( void ) {
	ri.Printf( PRINT_ALL, " Free command buffers: vk.command_buffers. \n" );
	qvkFreeCommandBuffers( vk.device, vk.command_pool, VK_FRAMES_IN_FLIGHT, vk.command_buffers );
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

#define VK_MAX_BOUND_SET_SLOTS 2
#define VK_MAX_BOUND_SET_DYNAMIC_OFFSETS 2

static VkPipeline s_boundPipeline;

typedef struct {
	qboolean valid;
	VkPipelineLayout layout;
	VkDescriptorSet set;
	uint32_t dynamicOffsetCount;
	uint32_t dynamicOffsets[VK_MAX_BOUND_SET_DYNAMIC_OFFSETS];
} vk_boundSetSlot_t;

static vk_boundSetSlot_t s_boundSetSlots[VK_MAX_BOUND_SET_SLOTS];

void vk_reset_bind_cache( void ) {
	int i;
	s_boundPipeline = VK_NULL_HANDLE;
	for ( i = 0; i < VK_MAX_BOUND_SET_SLOTS; i++ ) {
		s_boundSetSlots[i].valid = qfalse;
	}
}

void vk_bind_pipeline_cached( VkPipeline pipeline ) {
	if ( pipeline != s_boundPipeline ) {
		qvkCmdBindPipeline( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline );
		s_boundPipeline = pipeline;
	}
}

void vk_bind_descriptor_set_cached( VkPipelineLayout layout, uint32_t firstSet, VkDescriptorSet set,
		uint32_t dynamicOffsetCount, const uint32_t *pDynamicOffsets ) {
	vk_boundSetSlot_t *slot;
	uint32_t i;

	// Not a real call site today (every caller binds exactly 1 set with <=1 dynamic offset) --
	// fall back to an always-real bind and mark the slot unknown rather than risk a false cache hit.
	if ( firstSet >= VK_MAX_BOUND_SET_SLOTS || dynamicOffsetCount > VK_MAX_BOUND_SET_DYNAMIC_OFFSETS ) {
		qvkCmdBindDescriptorSets( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, firstSet, 1, &set,
			dynamicOffsetCount, pDynamicOffsets );
		if ( firstSet < VK_MAX_BOUND_SET_SLOTS ) {
			s_boundSetSlots[firstSet].valid = qfalse;
		}
		return;
	}

	slot = &s_boundSetSlots[firstSet];

	if ( slot->valid && slot->layout == layout && slot->set == set && slot->dynamicOffsetCount == dynamicOffsetCount ) {
		qboolean same = qtrue;
		for ( i = 0; i < dynamicOffsetCount; i++ ) {
			if ( slot->dynamicOffsets[i] != pDynamicOffsets[i] ) {
				same = qfalse;
				break;
			}
		}
		if ( same ) {
			return;
		}
	}

	qvkCmdBindDescriptorSets( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, firstSet, 1, &set,
		dynamicOffsetCount, pDynamicOffsets );

	slot->valid = qtrue;
	slot->layout = layout;
	slot->set = set;
	slot->dynamicOffsetCount = dynamicOffsetCount;
	for ( i = 0; i < dynamicOffsetCount; i++ ) {
		slot->dynamicOffsets[i] = pDynamicOffsets[i];
	}
}
