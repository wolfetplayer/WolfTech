// vk_swapchain.h -- adapted from vkQuake3 (GPLv2-or-later).

#ifndef VK_SWAPCHAIN_H_
#define VK_SWAPCHAIN_H_

void vk_createSwapChain( VkDevice device, VkSurfaceKHR surface, VkSurfaceFormatKHR surface_format );
void vk_destroySwapChain( void );
void vk_recreateSwapChain( void );

#endif // VK_SWAPCHAIN_H_
