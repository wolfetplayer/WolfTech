// vk_init.c -- bring-up/teardown order for the Vulkan pieces, adapted from vkQuake3 (GPLv2-or-later); RE_Shutdown in tr_init.c must free all descriptors before this destroys their pool.

#include <string.h>

#include "VKimpl.h"
#include "vk_instance.h"
#include "vk_swapchain.h"
#include "vk_cmd.h"
#include "vk_frame.h"
#include "vk_shaders.h"
#include "vk_pipelines.h"
#include "vk_depth.h"
#include "vk_draw2d.h"
#include "vk_draw3d.h"
#include "vk_world.h"
#include "glConfig.h"
#include "vk_init.h"

void vk_initialize( void ) {
	int width, height;

	vk_createWindow();

	vk_getProcAddress();

	vk_createSwapChain( vk.device, vk.surface, vk.surface_format );

	vk_create_sync_primitives();

	ri.Printf( PRINT_ALL, " Create command pool: vk.command_pool \n" );
	vk_create_command_pool( &vk.command_pool );

	ri.Printf( PRINT_ALL, " Create command buffer: vk.command_buffer \n" );
	vk_create_command_buffer( vk.command_pool, &vk.command_buffer );

	R_GetWinResolution( &width, &height );
	vk_createDepthResources( (uint32_t)width, (uint32_t)height );

	vk_createRenderPass();

	vk_createFrameBuffers( (uint32_t)width, (uint32_t)height );

	vk_createPipelineLayout();

	vk_loadShaderModules();

	vk_create2DPipelines();
	vk_create3DPipelines();

	vk_draw2d_init();
	vk_draw3d_init();

	vk.isInitialized = VK_TRUE;
}

void vk_shutdown( void ) {
	ri.Printf( PRINT_ALL, "vk_shutdown()\n" );

	vk_world_shutdown();
	vk_draw3d_shutdown();
	vk_draw2d_shutdown();

	vk_destroy3DPipelines();
	vk_destroy2DPipelines();

	vk_destroyShaderModules();

	vk_destroyPipelineLayout(); // frees vk.descriptor_pool

	vk_destroyFrameBuffers();

	vk_destroyRenderPass();

	vk_destroyDepthResources();

	vk_destroy_sync_primitives();

	vk_destroy_commands();

	vk_destroySwapChain();

	vk_clearProcAddress();

	ri.Printf( PRINT_ALL, " clear vk struct: vk \n" );
	memset( &vk, 0, sizeof( vk ) );

	vk.isInitialized = VK_FALSE;
}
