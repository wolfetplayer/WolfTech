// vk_image.h -- upload a decoded RGBA8 buffer to a device-local VkImage.

#ifndef VK_IMAGE_H_
#define VK_IMAGE_H_

#include "tr_image.h"

uint32_t vk_find_memory_type( uint32_t type_bits, VkMemoryPropertyFlags properties );

VkCommandBuffer vk_begin_one_shot_cmd( void );
void vk_end_one_shot_cmd( VkCommandBuffer cmd );

qboolean vk_create_device_local_buffer( const void *data, VkDeviceSize size, VkBufferUsageFlags usage,
										  VkBuffer *outBuffer, VkDeviceMemory *outMemory );

// Fills in image's vk_* fields from an RGBA8 pixel buffer.
qboolean vk_create_texture( image_t *image, const byte *pixels, int width, int height, int flags );
void vk_destroy_texture( image_t *image );

// Image+view+sampler only, no descriptor set -- for textures bound into a caller-owned descriptor set (e.g. lightmaps).
qboolean vk_create_texture_image( const byte *pixels, int width, int height, int flags,
									VkImage *outImage, VkDeviceMemory *outMemory, VkImageView *outView, VkSampler *outSampler );
void vk_destroy_texture_image( VkImage image, VkDeviceMemory memory, VkImageView view, VkSampler sampler );

#endif // VK_IMAGE_H_
