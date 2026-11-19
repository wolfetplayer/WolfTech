// vk_screenshot.c -- "screenshot"/"screenshotJPEG": copies the just-presented swapchain image back via a
// one-shot staging buffer and writes TGA/JPEG. Runs after vk_end_frame() returns, not inside the frame's own
// command buffer -- vk_end_one_shot_cmd()'s queue-wait-idle also covers the frame submitted just before it,
// since both go through the same vk.queue in submission order.

#include <stdlib.h>
#include <string.h>

#include "VKimpl.h"
#include "vk_instance.h"
#include "vk_image.h"
#include "glConfig.h"
#include "vk_screenshot.h"

// Defined in tr_image_jpg.c; no shared header declares it.
void RE_SaveJPG( char *filename, int quality, int image_width, int image_height, byte *image_buffer, int padding );

static cvar_t *r_screenshotJpegQuality;

static qboolean s_pending = qfalse;
static qboolean s_pendingJpeg = qfalse;
static char s_pendingName[MAX_OSPATH];

static qboolean GetChannelOrder( qboolean *outBgr ) {
	switch ( vk.surface_format.format ) {
	case VK_FORMAT_B8G8R8A8_UNORM:
	case VK_FORMAT_B8G8R8A8_SRGB:
		*outBgr = qtrue;
		return qtrue;
	case VK_FORMAT_R8G8B8A8_UNORM:
	case VK_FORMAT_R8G8B8A8_SRGB:
		*outBgr = qfalse;
		return qtrue;
	default:
		return qfalse;
	}
}

// Swapchain memory is top-down, 4 bytes/pixel; TGA and RE_SaveJPG both expect bottom-up, 3 bytes/pixel (RGB).
static void ConvertToBottomUpRGB( const byte *src, int width, int height, qboolean bgr, byte *dst ) {
	int row, x;

	for ( row = 0; row < height; row++ ) {
		const byte *srcRow = src + (size_t)row * width * 4;
		byte *dstRow = dst + (size_t)( height - 1 - row ) * width * 3;

		for ( x = 0; x < width; x++ ) {
			byte c0 = srcRow[x * 4 + 0];
			byte c1 = srcRow[x * 4 + 1];
			byte c2 = srcRow[x * 4 + 2];

			dstRow[x * 3 + 0] = bgr ? c2 : c0;
			dstRow[x * 3 + 1] = c1;
			dstRow[x * 3 + 2] = bgr ? c0 : c2;
		}
	}
}

static void WriteTGA( const char *filename, const byte *rgbBottomUp, int width, int height ) {
	byte header[18];
	byte *fileBuf;
	size_t numPixels = (size_t)width * height;
	size_t i;

	memset( header, 0, sizeof( header ) );
	header[2] = 2; // uncompressed true-color
	header[12] = (byte)( width & 255 );
	header[13] = (byte)( width >> 8 );
	header[14] = (byte)( height & 255 );
	header[15] = (byte)( height >> 8 );
	header[16] = 24; // bits per pixel

	fileBuf = malloc( sizeof( header ) + numPixels * 3 );
	memcpy( fileBuf, header, sizeof( header ) );
	for ( i = 0; i < numPixels; i++ ) {
		fileBuf[sizeof( header ) + i * 3 + 0] = rgbBottomUp[i * 3 + 2]; // TGA stores BGR
		fileBuf[sizeof( header ) + i * 3 + 1] = rgbBottomUp[i * 3 + 1];
		fileBuf[sizeof( header ) + i * 3 + 2] = rgbBottomUp[i * 3 + 0];
	}

	ri.FS_WriteFile( filename, fileBuf, (int)( sizeof( header ) + numPixels * 3 ) );
	free( fileBuf );
}

static void PickFilename( qboolean jpeg, char *out, size_t outSize ) {
	const char *ext = jpeg ? "jpg" : "tga";
	int i;

	if ( ri.Cmd_Argc() > 1 ) {
		Com_sprintf( out, (int)outSize, "screenshots/%s.%s", ri.Cmd_Argv( 1 ), ext );
		return;
	}

	for ( i = 0; i <= 9999; i++ ) {
		Com_sprintf( out, (int)outSize, "screenshots/shot%04d.%s", i, ext );
		if ( !ri.FS_FileExists( out ) ) {
			return;
		}
	}
}

static void TakeScreenshot_f( void ) {
	s_pending = qtrue;
	s_pendingJpeg = qfalse;
	PickFilename( qfalse, s_pendingName, sizeof( s_pendingName ) );
}

static void TakeScreenshotJPEG_f( void ) {
	s_pending = qtrue;
	s_pendingJpeg = qtrue;
	PickFilename( qtrue, s_pendingName, sizeof( s_pendingName ) );
}

void vk_screenshot_init( void ) {
	r_screenshotJpegQuality = ri.Cvar_Get( "r_screenshotJpegQuality", "90", CVAR_ARCHIVE );
	ri.Cmd_AddCommand( "screenshot", TakeScreenshot_f );
	ri.Cmd_AddCommand( "screenshotJPEG", TakeScreenshotJPEG_f );
}

void vk_screenshot_shutdown( void ) {
	ri.Cmd_RemoveCommand( "screenshot" );
	ri.Cmd_RemoveCommand( "screenshotJPEG" );
}

void vk_screenshot_process_pending( void ) {
	int width, height;
	qboolean bgr;
	VkBuffer buffer;
	VkDeviceMemory memory;
	VkDeviceSize size;
	void *mapped;
	byte *rgb;

	if ( !s_pending ) {
		return;
	}
	s_pending = qfalse;

	if ( !GetChannelOrder( &bgr ) ) {
		ri.Printf( PRINT_WARNING, "screenshot: unsupported swapchain format %d\n", vk.surface_format.format );
		return;
	}

	R_GetWinResolution( &width, &height );
	size = (VkDeviceSize)width * height * 4;

	{
		VkBufferCreateInfo buf_desc;
		VkMemoryRequirements mem_reqs;
		VkMemoryAllocateInfo alloc_info;

		buf_desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buf_desc.pNext = NULL;
		buf_desc.flags = 0;
		buf_desc.size = size;
		buf_desc.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		buf_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		buf_desc.queueFamilyIndexCount = 0;
		buf_desc.pQueueFamilyIndices = NULL;
		VK_CHECK( qvkCreateBuffer( vk.device, &buf_desc, NULL, &buffer ) );

		qvkGetBufferMemoryRequirements( vk.device, buffer, &mem_reqs );

		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.pNext = NULL;
		alloc_info.allocationSize = mem_reqs.size;
		alloc_info.memoryTypeIndex = vk_find_memory_type( mem_reqs.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
		VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &memory ) );
		VK_CHECK( qvkBindBufferMemory( vk.device, buffer, memory, 0 ) );
	}

	{
		VkCommandBuffer cmd = vk_begin_one_shot_cmd();
		VkImage image = vk.swapchain_images_array[vk.idx_swapchain_image];
		VkImageMemoryBarrier barrier;
		VkBufferImageCopy region;

		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.pNext = NULL;
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
			0, 0, NULL, 0, NULL, 1, &barrier );

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
		qvkCmdCopyImageToBuffer( cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &region );

		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = 0;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		qvkCmdPipelineBarrier( cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			0, 0, NULL, 0, NULL, 1, &barrier );

		vk_end_one_shot_cmd( cmd );
	}

	VK_CHECK( qvkMapMemory( vk.device, memory, 0, size, 0, &mapped ) );

	rgb = malloc( (size_t)width * height * 3 );
	ConvertToBottomUpRGB( (const byte *)mapped, width, height, bgr, rgb );

	qvkUnmapMemory( vk.device, memory );
	qvkDestroyBuffer( vk.device, buffer, NULL );
	qvkFreeMemory( vk.device, memory, NULL );

	if ( s_pendingJpeg ) {
		RE_SaveJPG( s_pendingName, r_screenshotJpegQuality->integer, width, height, rgb, 0 );
	} else {
		WriteTGA( s_pendingName, rgb, width, height );
	}
	ri.Printf( PRINT_ALL, "Wrote %s\n", s_pendingName );

	free( rgb );
}
