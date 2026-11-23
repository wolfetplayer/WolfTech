// vk_depth.c -- depth-buffer image/view backing the render pass's depth attachment.

#include "VKimpl.h"
#include "vk_instance.h"
#include "vk_image.h"
#include "vk_depth.h"

// D32_SFLOAT has no stencil bits, so it's deliberately left out -- stencil shadows (vk_shadows.c) need one.
static VkFormat find_depth_format( void ) {
	static const VkFormat candidates[] = {
		VK_FORMAT_D24_UNORM_S8_UINT,
		VK_FORMAT_D32_SFLOAT_S8_UINT,
	};
	int i;

	for ( i = 0; i < (int)( sizeof( candidates ) / sizeof( candidates[0] ) ); i++ ) {
		VkFormatProperties props;
		qvkGetPhysicalDeviceFormatProperties( vk.physical_device, candidates[i], &props );
		if ( props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT ) {
			ri.Printf( PRINT_ALL, "Vulkan: depth format %s\n",
				( candidates[i] == VK_FORMAT_D24_UNORM_S8_UINT ) ? "D24_UNORM_S8_UINT" : "D32_SFLOAT_S8_UINT (fallback)" );
			return candidates[i];
		}
	}

	ri.Error( ERR_FATAL, "Vulkan: no supported depth format" );
	return VK_FORMAT_D32_SFLOAT;
}

void vk_createDepthResources( uint32_t width, uint32_t height ) {
	vk.depth_format = find_depth_format();

	VkImageCreateInfo image_desc;
	image_desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_desc.pNext = NULL;
	image_desc.flags = 0;
	image_desc.imageType = VK_IMAGE_TYPE_2D;
	image_desc.format = vk.depth_format;
	image_desc.extent.width = width;
	image_desc.extent.height = height;
	image_desc.extent.depth = 1;
	image_desc.mipLevels = 1;
	image_desc.arrayLayers = 1;
	image_desc.samples = VK_SAMPLE_COUNT_1_BIT;
	image_desc.tiling = VK_IMAGE_TILING_OPTIMAL;
	image_desc.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	image_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	image_desc.queueFamilyIndexCount = 0;
	image_desc.pQueueFamilyIndices = NULL;
	image_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VK_CHECK( qvkCreateImage( vk.device, &image_desc, NULL, &vk.depth_image ) );

	VkMemoryRequirements mem_reqs;
	qvkGetImageMemoryRequirements( vk.device, vk.depth_image, &mem_reqs );

	VkMemoryAllocateInfo alloc_info;
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = mem_reqs.size;
	alloc_info.memoryTypeIndex = vk_find_memory_type( mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &vk.depth_memory ) );
	VK_CHECK( qvkBindImageMemory( vk.device, vk.depth_image, vk.depth_memory, 0 ) );

	VkImageViewCreateInfo view_desc;
	view_desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view_desc.pNext = NULL;
	view_desc.flags = 0;
	view_desc.image = vk.depth_image;
	view_desc.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view_desc.format = vk.depth_format;
	view_desc.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	view_desc.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	view_desc.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	view_desc.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	view_desc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	view_desc.subresourceRange.baseMipLevel = 0;
	view_desc.subresourceRange.levelCount = 1;
	view_desc.subresourceRange.baseArrayLayer = 0;
	view_desc.subresourceRange.layerCount = 1;
	VK_CHECK( qvkCreateImageView( vk.device, &view_desc, NULL, &vk.depth_view ) );
}

void vk_destroyDepthResources( void ) {
	qvkDestroyImageView( vk.device, vk.depth_view, NULL );
	qvkDestroyImage( vk.device, vk.depth_image, NULL );
	qvkFreeMemory( vk.device, vk.depth_memory, NULL );
}
