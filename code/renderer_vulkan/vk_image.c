// vk_image.c -- uploads a decoded RGBA8 buffer to a device-local VkImage via a one-shot staging buffer (one mip level, no mipmap generation).

#include <string.h>

#include "VKimpl.h"
#include "vk_instance.h"
#include "vk_image.h"

uint32_t vk_find_memory_type( uint32_t type_bits, VkMemoryPropertyFlags properties ) {
	uint32_t i;
	for ( i = 0; i < vk.devMemProperties.memoryTypeCount; i++ ) {
		if ( ( type_bits & ( 1u << i ) ) &&
			 ( vk.devMemProperties.memoryTypes[i].propertyFlags & properties ) == properties ) {
			return i;
		}
	}
	ri.Error( ERR_FATAL, "Vulkan: failed to find a suitable memory type" );
	return 0;
}

VkCommandBuffer vk_begin_one_shot_cmd( void ) {
	VkCommandBuffer cmd;

	VkCommandBufferAllocateInfo alloc_info;
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.commandPool = vk.command_pool;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = 1;
	VK_CHECK( qvkAllocateCommandBuffers( vk.device, &alloc_info, &cmd ) );

	VkCommandBufferBeginInfo begin_info;
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.pNext = NULL;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	begin_info.pInheritanceInfo = NULL;
	VK_CHECK( qvkBeginCommandBuffer( cmd, &begin_info ) );

	return cmd;
}

void vk_end_one_shot_cmd( VkCommandBuffer cmd ) {
	VK_CHECK( qvkEndCommandBuffer( cmd ) );

	VkSubmitInfo submit_info;
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.pNext = NULL;
	submit_info.waitSemaphoreCount = 0;
	submit_info.pWaitSemaphores = NULL;
	submit_info.pWaitDstStageMask = NULL;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &cmd;
	submit_info.signalSemaphoreCount = 0;
	submit_info.pSignalSemaphores = NULL;

	VK_CHECK( qvkQueueSubmit( vk.queue, 1, &submit_info, VK_NULL_HANDLE ) );
	VK_CHECK( qvkQueueWaitIdle( vk.queue ) ); // not per-frame, a wait-idle is fine

	qvkFreeCommandBuffers( vk.device, vk.command_pool, 1, &cmd );
}

static void create_staging_buffer( VkDeviceSize size, VkBuffer *pBuf, VkDeviceMemory *pMem, void **pMapped ) {
	VkBufferCreateInfo buf_desc;
	buf_desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buf_desc.pNext = NULL;
	buf_desc.flags = 0;
	buf_desc.size = size;
	buf_desc.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	buf_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	buf_desc.queueFamilyIndexCount = 0;
	buf_desc.pQueueFamilyIndices = NULL;
	VK_CHECK( qvkCreateBuffer( vk.device, &buf_desc, NULL, pBuf ) );

	VkMemoryRequirements mem_reqs;
	qvkGetBufferMemoryRequirements( vk.device, *pBuf, &mem_reqs );

	VkMemoryAllocateInfo alloc_info;
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = mem_reqs.size;
	alloc_info.memoryTypeIndex = vk_find_memory_type( mem_reqs.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, pMem ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, *pBuf, *pMem, 0 ) );

	VK_CHECK( qvkMapMemory( vk.device, *pMem, 0, size, 0, pMapped ) );
}

qboolean vk_create_texture( image_t *image, const byte *pixels, int width, int height, int flags ) {
	VkDeviceSize image_size = (VkDeviceSize)width * height * 4;

	VkBuffer staging_buf;
	VkDeviceMemory staging_mem;
	void *mapped;
	create_staging_buffer( image_size, &staging_buf, &staging_mem, &mapped );
	memcpy( mapped, pixels, (size_t)image_size );
	qvkUnmapMemory( vk.device, staging_mem );

	VkImageCreateInfo image_desc;
	image_desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_desc.pNext = NULL;
	image_desc.flags = 0;
	image_desc.imageType = VK_IMAGE_TYPE_2D;
	image_desc.format = VK_FORMAT_R8G8B8A8_UNORM;
	image_desc.extent.width = (uint32_t)width;
	image_desc.extent.height = (uint32_t)height;
	image_desc.extent.depth = 1;
	image_desc.mipLevels = 1;
	image_desc.arrayLayers = 1;
	image_desc.samples = VK_SAMPLE_COUNT_1_BIT;
	image_desc.tiling = VK_IMAGE_TILING_OPTIMAL;
	image_desc.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	image_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	image_desc.queueFamilyIndexCount = 0;
	image_desc.pQueueFamilyIndices = NULL;
	image_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VK_CHECK( qvkCreateImage( vk.device, &image_desc, NULL, &image->vk_image ) );

	VkMemoryRequirements mem_reqs;
	qvkGetImageMemoryRequirements( vk.device, image->vk_image, &mem_reqs );

	VkMemoryAllocateInfo alloc_info;
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = mem_reqs.size;
	alloc_info.memoryTypeIndex = vk_find_memory_type( mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &image->vk_memory ) );
	VK_CHECK( qvkBindImageMemory( vk.device, image->vk_image, image->vk_memory, 0 ) );

	{
		VkCommandBuffer cmd = vk_begin_one_shot_cmd();

		VkImageMemoryBarrier barrier;
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.pNext = NULL;
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = image->vk_image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
			0, 0, NULL, 0, NULL, 1, &barrier );

		VkBufferImageCopy region;
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageOffset.x = 0;
		region.imageOffset.y = 0;
		region.imageOffset.z = 0;
		region.imageExtent.width = (uint32_t)width;
		region.imageExtent.height = (uint32_t)height;
		region.imageExtent.depth = 1;
		qvkCmdCopyBufferToImage( cmd, staging_buf, image->vk_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );

		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0, 0, NULL, 0, NULL, 1, &barrier );

		vk_end_one_shot_cmd( cmd );
	}

	qvkDestroyBuffer( vk.device, staging_buf, NULL );
	qvkFreeMemory( vk.device, staging_mem, NULL );

	VkImageViewCreateInfo view_desc;
	view_desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view_desc.pNext = NULL;
	view_desc.flags = 0;
	view_desc.image = image->vk_image;
	view_desc.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view_desc.format = VK_FORMAT_R8G8B8A8_UNORM;
	view_desc.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	view_desc.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	view_desc.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	view_desc.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	view_desc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	view_desc.subresourceRange.baseMipLevel = 0;
	view_desc.subresourceRange.levelCount = 1;
	view_desc.subresourceRange.baseArrayLayer = 0;
	view_desc.subresourceRange.layerCount = 1;
	VK_CHECK( qvkCreateImageView( vk.device, &view_desc, NULL, &image->vk_view ) );

	VkSamplerAddressMode address_mode = ( flags & IMGFLAG_CLAMPTOEDGE ) ?
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE : VK_SAMPLER_ADDRESS_MODE_REPEAT;

	VkSamplerCreateInfo sampler_desc;
	sampler_desc.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampler_desc.pNext = NULL;
	sampler_desc.flags = 0;
	sampler_desc.magFilter = VK_FILTER_LINEAR;
	sampler_desc.minFilter = VK_FILTER_LINEAR;
	sampler_desc.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	sampler_desc.addressModeU = address_mode;
	sampler_desc.addressModeV = address_mode;
	sampler_desc.addressModeW = address_mode;
	sampler_desc.mipLodBias = 0.0f;
	sampler_desc.anisotropyEnable = VK_FALSE;
	sampler_desc.maxAnisotropy = 1.0f;
	sampler_desc.compareEnable = VK_FALSE;
	sampler_desc.compareOp = VK_COMPARE_OP_ALWAYS;
	sampler_desc.minLod = 0.0f;
	sampler_desc.maxLod = 0.0f;
	sampler_desc.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	sampler_desc.unnormalizedCoordinates = VK_FALSE;
	VK_CHECK( qvkCreateSampler( vk.device, &sampler_desc, NULL, &image->vk_sampler ) );

	VkDescriptorSetAllocateInfo set_alloc;
	set_alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	set_alloc.pNext = NULL;
	set_alloc.descriptorPool = vk.descriptor_pool;
	set_alloc.descriptorSetCount = 1;
	set_alloc.pSetLayouts = &vk.set_layout;
	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &set_alloc, &image->vk_descriptor ) );

	VkDescriptorImageInfo image_info;
	image_info.sampler = image->vk_sampler;
	image_info.imageView = image->vk_view;
	image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkWriteDescriptorSet write;
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.pNext = NULL;
	write.dstSet = image->vk_descriptor;
	write.dstBinding = 0;
	write.dstArrayElement = 0;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.pImageInfo = &image_info;
	write.pBufferInfo = NULL;
	write.pTexelBufferView = NULL;
	qvkUpdateDescriptorSets( vk.device, 1, &write, 0, NULL );

	return qtrue;
}

void vk_destroy_texture( image_t *image ) {
	if ( image->vk_descriptor ) {
		qvkFreeDescriptorSets( vk.device, vk.descriptor_pool, 1, &image->vk_descriptor );
	}
	if ( image->vk_sampler ) {
		qvkDestroySampler( vk.device, image->vk_sampler, NULL );
	}
	if ( image->vk_view ) {
		qvkDestroyImageView( vk.device, image->vk_view, NULL );
	}
	if ( image->vk_image ) {
		qvkDestroyImage( vk.device, image->vk_image, NULL );
	}
	if ( image->vk_memory ) {
		qvkFreeMemory( vk.device, image->vk_memory, NULL );
	}
}
