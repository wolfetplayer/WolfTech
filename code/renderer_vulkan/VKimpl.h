// VKimpl.h -- platform window/surface entry points, adapted from vkQuake3 (GPLv2-or-later); vendored vulkan/*.h is Khronos Vulkan-Headers (Apache 2.0).

#ifndef VKIMPL_H_
#define VKIMPL_H_

#define VK_NO_PROTOTYPES
#include "vulkan/vulkan.h"

void vk_createWindow( void );
void vk_destroyWindow( void );

void vk_getInstanceProcAddrImpl( void );

void vk_createSurfaceImpl( void );

void vk_minimizeWindow( void );

#endif // VKIMPL_H_
