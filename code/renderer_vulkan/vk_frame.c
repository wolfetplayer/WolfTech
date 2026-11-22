// vk_frame.c -- per-frame acquire/render-pass/present cycle, adapted from vkQuake3 (GPLv2-or-later); the render pass owns layout transitions, so no manual barrier is needed here.

#include "VKimpl.h"
#include "vk_instance.h"
#include "vk_cmd.h"
#include "vk_swapchain.h"
#include "glConfig.h"
#include "vk_frame.h"

// Indexed by frame slot (vk_current_frame_slot) -- paired with that slot's command buffer/fence.
static VkSemaphore sema_imageAvailable[VK_FRAMES_IN_FLIGHT];
// Indexed by swapchain image, not frame slot: present/acquire order isn't guaranteed to match submission order closely enough to reuse a frame-slot-indexed semaphore here.
static VkSemaphore sema_renderFinished[MAX_SWAPCHAIN_IMAGES];
static VkFence fence_renderFinished[VK_FRAMES_IN_FLIGHT];

static uint32_t s_frameSlot;

uint32_t vk_current_frame_slot( void ) {
	return s_frameSlot;
}

void vk_create_sync_primitives( void ) {
	VkSemaphoreCreateInfo desc;
	VkFenceCreateInfo fence_desc;
	uint32_t i;

	desc.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;

	for ( i = 0; i < VK_FRAMES_IN_FLIGHT; i++ ) {
		VK_CHECK( qvkCreateSemaphore( vk.device, &desc, NULL, &sema_imageAvailable[i] ) );
	}
	for ( i = 0; i < vk.swapchain_image_count; i++ ) {
		VK_CHECK( qvkCreateSemaphore( vk.device, &desc, NULL, &sema_renderFinished[i] ) );
	}

	fence_desc.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fence_desc.pNext = NULL;
	fence_desc.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for ( i = 0; i < VK_FRAMES_IN_FLIGHT; i++ ) {
		VK_CHECK( qvkCreateFence( vk.device, &fence_desc, NULL, &fence_renderFinished[i] ) );
	}

	s_frameSlot = 0;
}

void vk_destroy_sync_primitives( void ) {
	uint32_t i;

	ri.Printf( PRINT_ALL, " Destroy sema_imageAvailable sema_renderFinished fence_renderFinished\n" );

	for ( i = 0; i < VK_FRAMES_IN_FLIGHT; i++ ) {
		qvkDestroySemaphore( vk.device, sema_imageAvailable[i], NULL );
		qvkDestroyFence( vk.device, fence_renderFinished[i], NULL );
	}
	for ( i = 0; i < vk.swapchain_image_count; i++ ) {
		qvkDestroySemaphore( vk.device, sema_renderFinished[i], NULL );
	}
}

qboolean vk_begin_frame( void ) {
	VkResult result;

	if ( vk.swapchain == VK_NULL_HANDLE ) {
		// Left this way by a previous recreate finding a zero-sized (e.g. minimized) window; retry now that a new frame is starting, without touching any sync primitives below until one exists again.
		vk_recreateSwapChain();
		if ( vk.swapchain == VK_NULL_HANDLE ) {
			return qfalse;
		}
	}

	s_frameSlot = ( s_frameSlot + 1 ) % VK_FRAMES_IN_FLIGHT;

	// Throttles the CPU to VK_FRAMES_IN_FLIGHT frames ahead of the GPU, not 1 -- only blocks once the GPU has genuinely fallen a whole cycle behind.
	VK_CHECK( qvkWaitForFences( vk.device, 1, &fence_renderFinished[s_frameSlot], VK_FALSE, 1000000000ull ) );

	result = qvkAcquireNextImageKHR( vk.device, vk.swapchain, UINT64_MAX,
		sema_imageAvailable[s_frameSlot], VK_NULL_HANDLE, &vk.idx_swapchain_image );

	if ( result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_ERROR_SURFACE_LOST_KHR ) {
		qvkDeviceWaitIdle( vk.device );
		vk_recreateSwapChain();
		return qfalse;
	}
	if ( result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR ) {
		ri.Printf( PRINT_ALL, "Vulkan: error %s returned by qvkAcquireNextImageKHR\n", cvtResToStr( result ) );
	}

	VK_CHECK( qvkResetFences( vk.device, 1, &fence_renderFinished[s_frameSlot] ) );

	vk.command_buffer = vk.command_buffers[s_frameSlot];

	VkCommandBufferBeginInfo begin_info;
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.pNext = NULL;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	begin_info.pInheritanceInfo = NULL;

	VK_CHECK( qvkBeginCommandBuffer( vk.command_buffer, &begin_info ) );

	return qtrue;
}

void vk_begin_render_pass( float r, float g, float b, float a ) {
	int width, height;
	R_GetWinResolution( &width, &height );

	VkClearValue clear_values[2];
	clear_values[0].color.float32[0] = r;
	clear_values[0].color.float32[1] = g;
	clear_values[0].color.float32[2] = b;
	clear_values[0].color.float32[3] = a;
	clear_values[1].depthStencil.depth = 1.0f;
	clear_values[1].depthStencil.stencil = 0;

	VkRenderPassBeginInfo begin_info;
	begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	begin_info.pNext = NULL;
	begin_info.renderPass = vk.render_pass;
	begin_info.framebuffer = vk.framebuffers[vk.idx_swapchain_image];
	begin_info.renderArea.offset.x = 0;
	begin_info.renderArea.offset.y = 0;
	begin_info.renderArea.extent.width = (uint32_t)width;
	begin_info.renderArea.extent.height = (uint32_t)height;
	begin_info.clearValueCount = 2;
	begin_info.pClearValues = clear_values;

	qvkCmdBeginRenderPass( vk.command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE );
}

void vk_end_render_pass( void ) {
	qvkCmdEndRenderPass( vk.command_buffer );
}

void vk_clear_depth_only( void ) {
	int width, height;
	VkClearAttachment clearAttachment;
	VkClearRect clearRect;

	R_GetWinResolution( &width, &height );

	clearAttachment.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	clearAttachment.colorAttachment = 0; // unused for a depth-only clear
	clearAttachment.clearValue.depthStencil.depth = 1.0f;
	clearAttachment.clearValue.depthStencil.stencil = 0;

	clearRect.rect.offset.x = 0;
	clearRect.rect.offset.y = 0;
	clearRect.rect.extent.width = (uint32_t)width;
	clearRect.rect.extent.height = (uint32_t)height;
	clearRect.baseArrayLayer = 0;
	clearRect.layerCount = 1;

	qvkCmdClearAttachments( vk.command_buffer, 1, &clearAttachment, 1, &clearRect );
}

void vk_end_frame( void ) {
	VK_CHECK( qvkEndCommandBuffer( vk.command_buffer ) );

	VkPipelineStageFlags wait_dst_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	VkSubmitInfo submit_info;
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.pNext = NULL;
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = &sema_imageAvailable[s_frameSlot];
	submit_info.pWaitDstStageMask = &wait_dst_stage_mask;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &vk.command_buffer;
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = &sema_renderFinished[vk.idx_swapchain_image];

	VK_CHECK( qvkQueueSubmit( vk.queue, 1, &submit_info, fence_renderFinished[s_frameSlot] ) );

	VkPresentInfoKHR present_info;
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.pNext = NULL;
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = &sema_renderFinished[vk.idx_swapchain_image];
	present_info.swapchainCount = 1;
	present_info.pSwapchains = &vk.swapchain;
	present_info.pImageIndices = &vk.idx_swapchain_image;
	present_info.pResults = NULL;

	VkResult result = qvkQueuePresentKHR( vk.queue, &present_info );
	if ( result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR ) {
		return;
	}
	if ( result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_ERROR_SURFACE_LOST_KHR ) {
		qvkDeviceWaitIdle( vk.device );
		vk_recreateSwapChain();
	}
}
