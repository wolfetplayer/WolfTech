// vk_image.c -- uploads a decoded RGBA8 buffer to a device-local VkImage via a one-shot staging buffer, generating a full mip chain with vkCmdBlitImage unless IMGFLAG_NOMIPMAP is set.

#include <string.h>
#include <math.h>

#include "VKimpl.h"
#include "vk_instance.h"
#include "vk_image.h"

// Baked into every texture at upload time: there's no hardware gamma ramp or postprocess pass yet
// (see docs/vulkan-renderer-roadmap.md), so a changed r_gamma only affects textures loaded afterward.
static cvar_t *r_gamma;
static byte s_gammaTable[256];
static float s_gammaTableBuiltFor = -1.0f;

static void R_BuildGammaTableIfNeeded( void ) {
	int i;

	if ( !r_gamma ) {
		r_gamma = ri.Cvar_Get( "r_gamma", "1.4", CVAR_ARCHIVE | CVAR_LATCH );
	}

	if ( r_gamma->value == s_gammaTableBuiltFor ) {
		return;
	}

	{
		float g = r_gamma->value;
		if ( g < 0.5f ) g = 0.5f;
		if ( g > 3.0f ) g = 3.0f;

		for ( i = 0; i < 256; i++ ) {
			float corrected = 255.0f * powf( i / 255.0f, 1.0f / g ) + 0.5f;
			if ( corrected < 0.0f ) corrected = 0.0f;
			if ( corrected > 255.0f ) corrected = 255.0f;
			s_gammaTable[i] = (byte)corrected;
		}
		s_gammaTableBuiltFor = r_gamma->value;
	}
}

// dst is the staging buffer's mapped pointer -- likely uncached/write-combined GPU-visible memory, so this reads only from src (normal RAM) and writes only to dst; reading dst back is drastically slow (~50ms/texture) and isn't needed here.
static void R_CopyWithGammaCorrection( byte *dst, const byte *src, int width, int height ) {
	int i, count;

	R_BuildGammaTableIfNeeded();

	count = width * height;
	for ( i = 0; i < count; i++ ) {
		dst[i * 4 + 0] = s_gammaTable[src[i * 4 + 0]];
		dst[i * 4 + 1] = s_gammaTable[src[i * 4 + 1]];
		dst[i * 4 + 2] = s_gammaTable[src[i * 4 + 2]];
		dst[i * 4 + 3] = src[i * 4 + 3]; // alpha is coverage/opacity, not luminance -- never gamma-corrected
	}
}

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

// Bulk loading (a map's textures/lightmaps, a model's skins, a menu's UI images) can mean hundreds to
// thousands of vk_create_texture_image calls back to back. A vkQueueWaitIdle() per texture -- cheap when
// each one is just a trivial copy -- becomes the dominant cost once mip-chain blits make each submission
// bigger, so uploads share one command buffer and one wait across up to MAX_PENDING_UPLOADS textures
// instead. vk_flush_pending_uploads() must run before anything uploaded this way gets sampled.
#define MAX_PENDING_UPLOADS 32

typedef struct {
	VkBuffer buffer;
	VkDeviceMemory memory;
} pendingStaging_t;

static VkCommandBuffer s_uploadCmd = VK_NULL_HANDLE;
static pendingStaging_t s_pendingStaging[MAX_PENDING_UPLOADS];
static int s_numPendingStaging = 0;

static VkCommandBuffer vk_get_upload_cmd( void ) {
	if ( s_uploadCmd == VK_NULL_HANDLE ) {
		s_uploadCmd = vk_begin_one_shot_cmd();
	}
	return s_uploadCmd;
}

void vk_flush_pending_uploads( void ) {
	int i;

	if ( s_uploadCmd == VK_NULL_HANDLE ) {
		return;
	}

	vk_end_one_shot_cmd( s_uploadCmd );
	s_uploadCmd = VK_NULL_HANDLE;

	for ( i = 0; i < s_numPendingStaging; i++ ) {
		qvkDestroyBuffer( vk.device, s_pendingStaging[i].buffer, NULL );
		qvkFreeMemory( vk.device, s_pendingStaging[i].memory, NULL );
	}

	s_numPendingStaging = 0;
}

static void vk_track_pending_staging( VkBuffer buffer, VkDeviceMemory memory ) {
	s_pendingStaging[s_numPendingStaging].buffer = buffer;
	s_pendingStaging[s_numPendingStaging].memory = memory;
	s_numPendingStaging++;
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

// One-time upload of static data (e.g. world geometry) to a device-local buffer via a staging copy.
qboolean vk_create_device_local_buffer( const void *data, VkDeviceSize size, VkBufferUsageFlags usage,
										  VkBuffer *outBuffer, VkDeviceMemory *outMemory ) {
	VkBuffer staging_buf;
	VkDeviceMemory staging_mem;
	void *mapped;
	create_staging_buffer( size, &staging_buf, &staging_mem, &mapped );
	memcpy( mapped, data, (size_t)size );
	qvkUnmapMemory( vk.device, staging_mem );

	VkBufferCreateInfo buf_desc;
	buf_desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buf_desc.pNext = NULL;
	buf_desc.flags = 0;
	buf_desc.size = size;
	buf_desc.usage = usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	buf_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	buf_desc.queueFamilyIndexCount = 0;
	buf_desc.pQueueFamilyIndices = NULL;
	VK_CHECK( qvkCreateBuffer( vk.device, &buf_desc, NULL, outBuffer ) );

	VkMemoryRequirements mem_reqs;
	qvkGetBufferMemoryRequirements( vk.device, *outBuffer, &mem_reqs );

	VkMemoryAllocateInfo alloc_info;
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = mem_reqs.size;
	alloc_info.memoryTypeIndex = vk_find_memory_type( mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, outMemory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, *outBuffer, *outMemory, 0 ) );

	{
		VkCommandBuffer cmd = vk_begin_one_shot_cmd();
		VkBufferCopy region;
		region.srcOffset = 0;
		region.dstOffset = 0;
		region.size = size;
		qvkCmdCopyBuffer( cmd, staging_buf, *outBuffer, 1, &region );
		vk_end_one_shot_cmd( cmd );
	}

	qvkDestroyBuffer( vk.device, staging_buf, NULL );
	qvkFreeMemory( vk.device, staging_mem, NULL );

	return qtrue;
}

static uint32_t calc_mip_levels( int width, int height ) {
	uint32_t levels = 1;
	int dim = ( width > height ) ? width : height;
	while ( dim > 1 ) {
		dim >>= 1;
		levels++;
	}
	return levels;
}

qboolean vk_create_texture_image( const byte *pixels, int width, int height, int flags,
									VkImage *outImage, VkDeviceMemory *outMemory, VkImageView *outView, VkSampler *outSampler ) {
	VkDeviceSize image_size = (VkDeviceSize)width * height * 4;
	uint32_t mipLevels = ( flags & IMGFLAG_NOMIPMAP ) ? 1 : calc_mip_levels( width, height );

	VkBuffer staging_buf;
	VkDeviceMemory staging_mem;
	void *mapped;
	create_staging_buffer( image_size, &staging_buf, &staging_mem, &mapped );
	R_CopyWithGammaCorrection( (byte *)mapped, pixels, width, height );
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
	image_desc.mipLevels = mipLevels;
	image_desc.arrayLayers = 1;
	image_desc.samples = VK_SAMPLE_COUNT_1_BIT;
	image_desc.tiling = VK_IMAGE_TILING_OPTIMAL;
	image_desc.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	image_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	image_desc.queueFamilyIndexCount = 0;
	image_desc.pQueueFamilyIndices = NULL;
	image_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VK_CHECK( qvkCreateImage( vk.device, &image_desc, NULL, outImage ) );

	VkMemoryRequirements mem_reqs;
	qvkGetImageMemoryRequirements( vk.device, *outImage, &mem_reqs );

	VkMemoryAllocateInfo alloc_info;
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = mem_reqs.size;
	alloc_info.memoryTypeIndex = vk_find_memory_type( mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, outMemory ) );
	VK_CHECK( qvkBindImageMemory( vk.device, *outImage, *outMemory, 0 ) );

	if ( s_numPendingStaging >= MAX_PENDING_UPLOADS ) {
		vk_flush_pending_uploads();
	}

	{
		VkCommandBuffer cmd = vk_get_upload_cmd();

		VkImageMemoryBarrier barrier;
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.pNext = NULL;
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = *outImage;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = mipLevels; // every level starts out undefined, not just level 0
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
		qvkCmdCopyBufferToImage( cmd, staging_buf, *outImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );

		if ( mipLevels > 1 ) {
			int srcWidth = width, srcHeight = height;
			uint32_t level;

			barrier.subresourceRange.levelCount = 1;

			for ( level = 1; level < mipLevels; level++ ) {
				int dstWidth = ( srcWidth > 1 ) ? srcWidth / 2 : 1;
				int dstHeight = ( srcHeight > 1 ) ? srcHeight / 2 : 1;
				VkImageBlit blit;

				barrier.subresourceRange.baseMipLevel = level - 1;
				barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
					0, 0, NULL, 0, NULL, 1, &barrier );

				blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				blit.srcSubresource.mipLevel = level - 1;
				blit.srcSubresource.baseArrayLayer = 0;
				blit.srcSubresource.layerCount = 1;
				blit.srcOffsets[0].x = 0; blit.srcOffsets[0].y = 0; blit.srcOffsets[0].z = 0;
				blit.srcOffsets[1].x = srcWidth; blit.srcOffsets[1].y = srcHeight; blit.srcOffsets[1].z = 1;
				blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				blit.dstSubresource.mipLevel = level;
				blit.dstSubresource.baseArrayLayer = 0;
				blit.dstSubresource.layerCount = 1;
				blit.dstOffsets[0].x = 0; blit.dstOffsets[0].y = 0; blit.dstOffsets[0].z = 0;
				blit.dstOffsets[1].x = dstWidth; blit.dstOffsets[1].y = dstHeight; blit.dstOffsets[1].z = 1;
				qvkCmdBlitImage( cmd, *outImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					*outImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR );

				srcWidth = dstWidth;
				srcHeight = dstHeight;
			}

			// levels [0, mipLevels-2] are now TRANSFER_SRC_OPTIMAL from the loop above; the last level is still TRANSFER_DST_OPTIMAL.
			barrier.subresourceRange.baseMipLevel = 0;
			barrier.subresourceRange.levelCount = mipLevels - 1;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0, 0, NULL, 0, NULL, 1, &barrier );

			barrier.subresourceRange.baseMipLevel = mipLevels - 1;
			barrier.subresourceRange.levelCount = 1;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0, 0, NULL, 0, NULL, 1, &barrier );
		} else {
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0, 0, NULL, 0, NULL, 1, &barrier );
		}

	}

	vk_track_pending_staging( staging_buf, staging_mem );

	VkImageViewCreateInfo view_desc;
	view_desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view_desc.pNext = NULL;
	view_desc.flags = 0;
	view_desc.image = *outImage;
	view_desc.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view_desc.format = VK_FORMAT_R8G8B8A8_UNORM;
	view_desc.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	view_desc.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	view_desc.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	view_desc.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	view_desc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	view_desc.subresourceRange.baseMipLevel = 0;
	view_desc.subresourceRange.levelCount = mipLevels;
	view_desc.subresourceRange.baseArrayLayer = 0;
	view_desc.subresourceRange.layerCount = 1;
	VK_CHECK( qvkCreateImageView( vk.device, &view_desc, NULL, outView ) );

	VkSamplerAddressMode address_mode = ( flags & IMGFLAG_CLAMPTOEDGE ) ?
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE : VK_SAMPLER_ADDRESS_MODE_REPEAT;

	VkSamplerCreateInfo sampler_desc;
	sampler_desc.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampler_desc.pNext = NULL;
	sampler_desc.flags = 0;
	sampler_desc.magFilter = VK_FILTER_LINEAR;
	sampler_desc.minFilter = VK_FILTER_LINEAR;
	sampler_desc.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler_desc.addressModeU = address_mode;
	sampler_desc.addressModeV = address_mode;
	sampler_desc.addressModeW = address_mode;
	sampler_desc.mipLodBias = 0.0f;
	sampler_desc.anisotropyEnable = vk.features.samplerAnisotropy;
	sampler_desc.maxAnisotropy = vk.features.samplerAnisotropy ? vk.deviceProperties.limits.maxSamplerAnisotropy : 1.0f;
	sampler_desc.compareEnable = VK_FALSE;
	sampler_desc.compareOp = VK_COMPARE_OP_ALWAYS;
	sampler_desc.minLod = 0.0f;
	sampler_desc.maxLod = (float)( mipLevels - 1 );
	sampler_desc.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	sampler_desc.unnormalizedCoordinates = VK_FALSE;
	VK_CHECK( qvkCreateSampler( vk.device, &sampler_desc, NULL, outSampler ) );

	return qtrue;
}

void vk_destroy_texture_image( VkImage image, VkDeviceMemory memory, VkImageView view, VkSampler sampler ) {
	if ( sampler ) {
		qvkDestroySampler( vk.device, sampler, NULL );
	}
	if ( view ) {
		qvkDestroyImageView( vk.device, view, NULL );
	}
	if ( image ) {
		qvkDestroyImage( vk.device, image, NULL );
	}
	if ( memory ) {
		qvkFreeMemory( vk.device, memory, NULL );
	}
}

qboolean vk_create_texture( image_t *image, const byte *pixels, int width, int height, int flags ) {
	if ( !vk_create_texture_image( pixels, width, height, flags,
			&image->vk_image, &image->vk_memory, &image->vk_view, &image->vk_sampler ) ) {
		return qfalse;
	}

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
	vk_destroy_texture_image( image->vk_image, image->vk_memory, image->vk_view, image->vk_sampler );
}
