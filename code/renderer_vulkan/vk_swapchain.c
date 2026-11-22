// vk_swapchain.c -- adapted from vkQuake3 (GPLv2-or-later).

#include <stdlib.h>

#include "VKimpl.h"
#include "vk_instance.h"
#include "vk_frame.h"
#include "vk_depth.h"
#include "vk_pipelines.h"
#include "glConfig.h"
#include "vk_swapchain.h"

extern cvar_t *r_fullscreen;

// Rebuilds everything that depends on the surface's actual size (swapchain, its per-image semaphores, depth buffer, framebuffers) -- pipelines/shaders/descriptor pools/per-subsystem buffers don't depend on it (pipelines already use dynamic viewport/scissor state) and are left alone. Both callers already call qvkDeviceWaitIdle first, so nothing below races in-flight GPU work.
void vk_recreateSwapChain( void ) {
	if ( r_fullscreen->integer ) {
		ri.Cvar_Set( "r_fullscreen", "0" );
		r_fullscreen->modified = qtrue;
	}

	// vk.swapchain is already VK_NULL_HANDLE when a previous call bailed out below (zero-sized/minimized window) -- vk_begin_frame calls this again every frame in that state, so skip re-destroying what's already gone and just recheck the surface size.
	if ( vk.swapchain != VK_NULL_HANDLE ) {
		ri.Printf( PRINT_ALL, " Recreate swap chain \n" );
		vk_destroyFrameBuffers();
		vk_destroyDepthResources();
		vk_destroy_sync_primitives();
		vk_destroySwapChain(); // leaves vk.swapchain == VK_NULL_HANDLE
	}

	VK_CHECK( qvkGetPhysicalDeviceSurfaceCapabilitiesKHR( vk.physical_device, vk.surface, &vk.surface_caps ) );

	if ( vk.surface_caps.currentExtent.width == 0 || vk.surface_caps.currentExtent.height == 0 ) {
		return; // still zero-sized -- vk_begin_frame retries this next frame
	}

	vk_createSwapChain( vk.device, vk.surface, vk.surface_format );
	vk_create_sync_primitives(); // swapchain image count can change across a recreate; rebuild all of it to stay in sync, not just the frame-slot-indexed primitives

	R_SetWinResolution( (int)vk.swapchain_extent.width, (int)vk.swapchain_extent.height );

	vk_createDepthResources( vk.swapchain_extent.width, vk.swapchain_extent.height );
	vk_createFrameBuffers( vk.swapchain_extent.width, vk.swapchain_extent.height );
}

void vk_createSwapChain( VkDevice device, VkSurfaceKHR surface, VkSurfaceFormatKHR surface_format ) {
	VkPresentModeKHR present_mode;
	uint32_t image_count;

	{
		uint32_t nPM, i;
		qvkGetPhysicalDeviceSurfacePresentModesKHR( vk.physical_device, surface, &nPM, NULL );

		VkPresentModeKHR *pPresentModes = (VkPresentModeKHR *) malloc( nPM * sizeof( VkPresentModeKHR ) );
		qvkGetPhysicalDeviceSurfacePresentModesKHR( vk.physical_device, surface, &nPM, pPresentModes );

		VkBool32 mailbox_supported = VK_FALSE;
		VkBool32 immediate_supported = VK_FALSE;

		for ( i = 0; i < nPM; i++ ) {
			if ( pPresentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR ) {
				mailbox_supported = VK_TRUE;
			} else if ( pPresentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR ) {
				immediate_supported = VK_TRUE;
			}
		}
		free( pPresentModes );

		if ( mailbox_supported ) {
			present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
			image_count = MAX( 3u, vk.surface_caps.minImageCount );
		} else if ( immediate_supported ) {
			present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
			image_count = MAX( 2u, vk.surface_caps.minImageCount );
		} else {
			// VK_PRESENT_MODE_FIFO_KHR is guaranteed to be available.
			present_mode = VK_PRESENT_MODE_FIFO_KHR;
			image_count = MAX( 2u, vk.surface_caps.minImageCount );
		}

		if ( vk.surface_caps.maxImageCount == 0 ) {
			image_count = MAX_SWAPCHAIN_IMAGES;
		} else {
			image_count = MIN( image_count + 1, vk.surface_caps.maxImageCount );
		}

		ri.Printf( PRINT_ALL, " Swapchain present mode: %d, image count: %d (min %d, max %d)\n",
				   present_mode, image_count, vk.surface_caps.minImageCount, vk.surface_caps.maxImageCount );
	}

	{
		VkExtent2D image_extent = vk.surface_caps.currentExtent;
		if ( ( image_extent.width == 0xffffffff ) && ( image_extent.height == 0xffffffff ) ) {
			image_extent.width = MIN( vk.surface_caps.maxImageExtent.width,
									   MAX( vk.surface_caps.minImageExtent.width, 640u ) );
			image_extent.height = MIN( vk.surface_caps.maxImageExtent.height,
										MAX( vk.surface_caps.minImageExtent.height, 480u ) );
		}

		ri.Printf( PRINT_ALL, " Swapchain extent: %d x %d\n", image_extent.width, image_extent.height );
		vk.swapchain_extent = image_extent;

		VkSwapchainCreateInfoKHR desc;
		desc.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.surface = surface;
		desc.minImageCount = image_count;
		desc.imageFormat = surface_format.format;
		desc.imageColorSpace = surface_format.colorSpace;
		desc.imageExtent = image_extent;
		desc.imageArrayLayers = 1;
		// TRANSFER_SRC lets vk_screenshot.c copy the presented image back out for "screenshot"/"screenshotJPEG".
		desc.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		desc.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		desc.queueFamilyIndexCount = 0;
		desc.pQueueFamilyIndices = NULL;
		desc.preTransform = vk.surface_caps.currentTransform;
		desc.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		desc.presentMode = present_mode;
		desc.clipped = VK_TRUE;
		desc.oldSwapchain = VK_NULL_HANDLE;

		VK_CHECK( qvkCreateSwapchainKHR( device, &desc, NULL, &vk.swapchain ) );
	}

	{
		VK_CHECK( qvkGetSwapchainImagesKHR( device, vk.swapchain, &vk.swapchain_image_count, NULL ) );

		ri.Printf( PRINT_ALL, " Swapchain image count: %d\n", vk.swapchain_image_count );

		if ( vk.swapchain_image_count > MAX_SWAPCHAIN_IMAGES ) {
			vk.swapchain_image_count = MAX_SWAPCHAIN_IMAGES;
		}

		VK_CHECK( qvkGetSwapchainImagesKHR( device, vk.swapchain, &vk.swapchain_image_count, vk.swapchain_images_array ) );

		uint32_t i;
		for ( i = 0; i < vk.swapchain_image_count; i++ ) {
			VkImageViewCreateInfo desc;
			desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			desc.pNext = NULL;
			desc.flags = 0;
			desc.image = vk.swapchain_images_array[i];
			desc.viewType = VK_IMAGE_VIEW_TYPE_2D;
			desc.format = vk.surface_format.format;
			desc.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			desc.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			desc.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			desc.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			desc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			desc.subresourceRange.baseMipLevel = 0;
			desc.subresourceRange.levelCount = 1;
			desc.subresourceRange.baseArrayLayer = 0;
			desc.subresourceRange.layerCount = 1;
			VK_CHECK( qvkCreateImageView( device, &desc, NULL, &vk.swapchain_image_views[i] ) );
		}
	}
}

void vk_destroySwapChain( void ) {
	uint32_t i;
	for ( i = 0; i < vk.swapchain_image_count; i++ ) {
		qvkDestroyImageView( vk.device, vk.swapchain_image_views[i], NULL );
	}
	qvkDestroySwapchainKHR( vk.device, vk.swapchain, NULL );
	vk.swapchain = VK_NULL_HANDLE; // vk_begin_frame's "not ready yet" check while minimized -- see vk_recreateSwapChain
	vk.swapchain_image_count = 0;
}
