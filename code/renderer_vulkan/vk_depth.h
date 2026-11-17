// vk_depth.h -- depth-buffer image/view backing the render pass's depth attachment.

#ifndef VK_DEPTH_H_
#define VK_DEPTH_H_

void vk_createDepthResources( uint32_t width, uint32_t height );
void vk_destroyDepthResources( void );

#endif // VK_DEPTH_H_
