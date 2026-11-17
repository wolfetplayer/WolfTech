// vk_frame.c -- per-frame acquire/render-pass/present cycle, adapted from vkQuake3 (GPLv2-or-later); the render pass owns layout transitions, so no manual barrier is needed here.

#include "VKimpl.h"
#include "vk_instance.h"
#include "vk_cmd.h"
#include "vk_swapchain.h"
#include "glConfig.h"
#include "vk_frame.h"

static VkSemaphore sema_imageAvailable;
static VkSemaphore sema_renderFinished;
static VkFence fence_renderFinished;

void vk_create_sync_primitives( void ) {
	VkSemaphoreCreateInfo desc;
	desc.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;

	VK_CHECK( qvkCreateSemaphore( vk.device, &desc, NULL, &sema_imageAvailable ) );
	VK_CHECK( qvkCreateSemaphore( vk.device, &desc, NULL, &sema_renderFinished ) );

	VkFenceCreateInfo fence_desc;
	fence_desc.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fence_desc.pNext = NULL;
	fence_desc.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	VK_CHECK( qvkCreateFence( vk.device, &fence_desc, NULL, &fence_renderFinished ) );
}

void vk_destroy_sync_primitives( void ) {
	ri.Printf( PRINT_ALL, " Destroy sema_imageAvailable sema_renderFinished fence_renderFinished\n" );

	qvkDestroySemaphore( vk.device, sema_imageAvailable, NULL );
	qvkDestroySemaphore( vk.device, sema_renderFinished, NULL );
	qvkDestroyFence( vk.device, fence_renderFinished, NULL );
}

qboolean vk_begin_frame( void ) {
	VkResult result = qvkAcquireNextImageKHR( vk.device, vk.swapchain, UINT64_MAX,
		sema_imageAvailable, VK_NULL_HANDLE, &vk.idx_swapchain_image );

	if ( result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_ERROR_SURFACE_LOST_KHR ) {
		qvkDeviceWaitIdle( vk.device );
		vk_recreateSwapChain();
		return qfalse;
	}
	if ( result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR ) {
		ri.Printf( PRINT_ALL, "Vulkan: error %s returned by qvkAcquireNextImageKHR\n", cvtResToStr( result ) );
	}

	VK_CHECK( qvkWaitForFences( vk.device, 1, &fence_renderFinished, VK_FALSE, 1000000000ull ) );
	VK_CHECK( qvkResetFences( vk.device, 1, &fence_renderFinished ) );

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

void vk_end_frame( void ) {
	VK_CHECK( qvkEndCommandBuffer( vk.command_buffer ) );

	VkPipelineStageFlags wait_dst_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	VkSubmitInfo submit_info;
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.pNext = NULL;
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = &sema_imageAvailable;
	submit_info.pWaitDstStageMask = &wait_dst_stage_mask;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &vk.command_buffer;
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = &sema_renderFinished;

	VK_CHECK( qvkQueueSubmit( vk.queue, 1, &submit_info, fence_renderFinished ) );

	VkPresentInfoKHR present_info;
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.pNext = NULL;
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = &sema_renderFinished;
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
