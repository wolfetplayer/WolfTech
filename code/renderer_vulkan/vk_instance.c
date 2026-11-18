// vk_instance.c -- Vulkan instance/device/function-pointer bring-up, adapted from vkQuake3 (GPLv2-or-later); validation layer is checked before use so a missing SDK doesn't hard-fail.

#include <string.h>
#include <stdlib.h>

#include "VKimpl.h"
#include "vk_instance.h"

struct Vk_Instance vk;

// Vulkan API functions used by the renderer.
PFN_vkGetInstanceProcAddr                       qvkGetInstanceProcAddr;

PFN_vkEnumerateInstanceLayerProperties          qvkEnumerateInstanceLayerProperties;
PFN_vkCreateInstance                            qvkCreateInstance;
PFN_vkEnumerateInstanceExtensionProperties      qvkEnumerateInstanceExtensionProperties;

PFN_vkCreateDevice                              qvkCreateDevice;
PFN_vkDestroyInstance                           qvkDestroyInstance;
PFN_vkEnumerateDeviceExtensionProperties        qvkEnumerateDeviceExtensionProperties;
PFN_vkEnumeratePhysicalDevices                  qvkEnumeratePhysicalDevices;
PFN_vkGetDeviceProcAddr                         qvkGetDeviceProcAddr;
PFN_vkGetPhysicalDeviceFeatures                 qvkGetPhysicalDeviceFeatures;
PFN_vkGetPhysicalDeviceFormatProperties         qvkGetPhysicalDeviceFormatProperties;
PFN_vkGetPhysicalDeviceMemoryProperties         qvkGetPhysicalDeviceMemoryProperties;
PFN_vkGetPhysicalDeviceProperties               qvkGetPhysicalDeviceProperties;
PFN_vkGetPhysicalDeviceQueueFamilyProperties    qvkGetPhysicalDeviceQueueFamilyProperties;

PFN_vkDestroySurfaceKHR                         qvkDestroySurfaceKHR;
PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR   qvkGetPhysicalDeviceSurfaceCapabilitiesKHR;
PFN_vkGetPhysicalDeviceSurfaceFormatsKHR        qvkGetPhysicalDeviceSurfaceFormatsKHR;
PFN_vkGetPhysicalDeviceSurfacePresentModesKHR   qvkGetPhysicalDeviceSurfacePresentModesKHR;
PFN_vkGetPhysicalDeviceSurfaceSupportKHR        qvkGetPhysicalDeviceSurfaceSupportKHR;

#ifndef NDEBUG
PFN_vkCreateDebugReportCallbackEXT              qvkCreateDebugReportCallbackEXT;
PFN_vkDestroyDebugReportCallbackEXT             qvkDestroyDebugReportCallbackEXT;
#endif

PFN_vkAllocateCommandBuffers                    qvkAllocateCommandBuffers;
PFN_vkAllocateDescriptorSets                    qvkAllocateDescriptorSets;
PFN_vkAllocateMemory                            qvkAllocateMemory;
PFN_vkBeginCommandBuffer                        qvkBeginCommandBuffer;
PFN_vkBindBufferMemory                          qvkBindBufferMemory;
PFN_vkBindImageMemory                           qvkBindImageMemory;
PFN_vkCmdBeginRenderPass                        qvkCmdBeginRenderPass;
PFN_vkCmdBindDescriptorSets                     qvkCmdBindDescriptorSets;
PFN_vkCmdBindIndexBuffer                        qvkCmdBindIndexBuffer;
PFN_vkCmdBindPipeline                           qvkCmdBindPipeline;
PFN_vkCmdBindVertexBuffers                      qvkCmdBindVertexBuffers;
PFN_vkCmdBlitImage                              qvkCmdBlitImage;
PFN_vkCmdClearAttachments                       qvkCmdClearAttachments;
PFN_vkCmdClearColorImage                        qvkCmdClearColorImage;
PFN_vkCmdCopyBuffer                             qvkCmdCopyBuffer;
PFN_vkCmdCopyBufferToImage                      qvkCmdCopyBufferToImage;
PFN_vkCmdCopyImage                              qvkCmdCopyImage;
PFN_vkCmdCopyImageToBuffer                      qvkCmdCopyImageToBuffer;
PFN_vkCmdDraw                                   qvkCmdDraw;
PFN_vkCmdDrawIndexed                            qvkCmdDrawIndexed;
PFN_vkCmdEndRenderPass                          qvkCmdEndRenderPass;
PFN_vkCmdPipelineBarrier                        qvkCmdPipelineBarrier;
PFN_vkCmdPushConstants                          qvkCmdPushConstants;
PFN_vkCmdSetDepthBias                           qvkCmdSetDepthBias;
PFN_vkCmdSetScissor                             qvkCmdSetScissor;
PFN_vkCmdSetViewport                            qvkCmdSetViewport;
PFN_vkCreateBuffer                              qvkCreateBuffer;
PFN_vkCreateCommandPool                         qvkCreateCommandPool;
PFN_vkCreateDescriptorPool                      qvkCreateDescriptorPool;
PFN_vkCreateDescriptorSetLayout                 qvkCreateDescriptorSetLayout;
PFN_vkCreateFence                               qvkCreateFence;
PFN_vkCreateFramebuffer                         qvkCreateFramebuffer;
PFN_vkCreateGraphicsPipelines                   qvkCreateGraphicsPipelines;
PFN_vkCreateImage                               qvkCreateImage;
PFN_vkCreateImageView                           qvkCreateImageView;
PFN_vkCreatePipelineLayout                      qvkCreatePipelineLayout;
PFN_vkCreateRenderPass                          qvkCreateRenderPass;
PFN_vkCreateSampler                             qvkCreateSampler;
PFN_vkCreateSemaphore                           qvkCreateSemaphore;
PFN_vkCreateShaderModule                        qvkCreateShaderModule;
PFN_vkDestroyBuffer                             qvkDestroyBuffer;
PFN_vkDestroyCommandPool                        qvkDestroyCommandPool;
PFN_vkDestroyDescriptorPool                     qvkDestroyDescriptorPool;
PFN_vkDestroyDescriptorSetLayout                qvkDestroyDescriptorSetLayout;
PFN_vkDestroyDevice                             qvkDestroyDevice;
PFN_vkDestroyFence                              qvkDestroyFence;
PFN_vkDestroyFramebuffer                        qvkDestroyFramebuffer;
PFN_vkDestroyImage                              qvkDestroyImage;
PFN_vkDestroyImageView                          qvkDestroyImageView;
PFN_vkDestroyPipeline                           qvkDestroyPipeline;
PFN_vkDestroyPipelineLayout                     qvkDestroyPipelineLayout;
PFN_vkDestroyRenderPass                         qvkDestroyRenderPass;
PFN_vkDestroySampler                            qvkDestroySampler;
PFN_vkDestroySemaphore                          qvkDestroySemaphore;
PFN_vkDestroyShaderModule                       qvkDestroyShaderModule;
PFN_vkDeviceWaitIdle                            qvkDeviceWaitIdle;
PFN_vkEndCommandBuffer                          qvkEndCommandBuffer;
PFN_vkFreeCommandBuffers                        qvkFreeCommandBuffers;
PFN_vkFreeDescriptorSets                        qvkFreeDescriptorSets;
PFN_vkFreeMemory                                qvkFreeMemory;
PFN_vkGetBufferMemoryRequirements               qvkGetBufferMemoryRequirements;
PFN_vkGetDeviceQueue                            qvkGetDeviceQueue;
PFN_vkGetImageMemoryRequirements                qvkGetImageMemoryRequirements;
PFN_vkGetImageSubresourceLayout                 qvkGetImageSubresourceLayout;
PFN_vkMapMemory                                 qvkMapMemory;
PFN_vkUnmapMemory                               qvkUnmapMemory;
PFN_vkQueueSubmit                               qvkQueueSubmit;
PFN_vkQueueWaitIdle                             qvkQueueWaitIdle;
PFN_vkResetDescriptorPool                       qvkResetDescriptorPool;
PFN_vkResetFences                               qvkResetFences;
PFN_vkUpdateDescriptorSets                      qvkUpdateDescriptorSets;
PFN_vkWaitForFences                             qvkWaitForFences;
PFN_vkAcquireNextImageKHR                       qvkAcquireNextImageKHR;
PFN_vkCreateSwapchainKHR                        qvkCreateSwapchainKHR;
PFN_vkDestroySwapchainKHR                       qvkDestroySwapchainKHR;
PFN_vkGetSwapchainImagesKHR                     qvkGetSwapchainImagesKHR;
PFN_vkQueuePresentKHR                           qvkQueuePresentKHR;


#ifndef NDEBUG

static VKAPI_ATTR VkBool32 VKAPI_CALL vk_DebugCallback(
		VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT object_type,
		uint64_t object, size_t location, int32_t message_code,
		const char *layer_prefix, const char *message, void *user_data ) {
	(void)flags; (void)object_type; (void)object; (void)location;
	(void)message_code; (void)layer_prefix; (void)user_data;
	ri.Printf( PRINT_WARNING, "%s\n", message );
	return VK_FALSE;
}

static void vk_createDebugCallback( PFN_vkDebugReportCallbackEXT qvkDebugCB ) {
	VkDebugReportCallbackCreateInfoEXT desc;
	desc.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
	desc.pNext = NULL;
	desc.flags = VK_DEBUG_REPORT_WARNING_BIT_EXT |
				 VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT |
				 VK_DEBUG_REPORT_ERROR_BIT_EXT;
	desc.pfnCallback = qvkDebugCB;
	desc.pUserData = NULL;

	VK_CHECK( qvkCreateDebugReportCallbackEXT( vk.instance, &desc, NULL, &vk.h_debugCB ) );
}

// VK_LAYER_KHRONOS_validation is only present with the Vulkan SDK installed; check before requesting it.
static qboolean vk_validationLayerAvailable( const char *layer_name ) {
	uint32_t count = 0;
	uint32_t i;
	VkLayerProperties *layers;
	qboolean found = qfalse;

	qvkEnumerateInstanceLayerProperties( &count, NULL );
	if ( count == 0 ) {
		return qfalse;
	}

	layers = (VkLayerProperties *) malloc( sizeof( VkLayerProperties ) * count );
	qvkEnumerateInstanceLayerProperties( &count, layers );

	for ( i = 0; i < count; i++ ) {
		if ( !strcmp( layers[i].layerName, layer_name ) ) {
			found = qtrue;
			break;
		}
	}

	free( layers );
	return found;
}

#endif // !NDEBUG


static void vk_createInstance( void ) {
	ri.Printf( PRINT_ALL, " Creating instance: vk.instance\n" );

	VkApplicationInfo appInfo;
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pNext = NULL;
	appInfo.pApplicationName = "Wolf-Tech";
	appInfo.applicationVersion = VK_MAKE_VERSION( 1, 0, 0 );
	appInfo.pEngineName = "Wolf-Tech";
	appInfo.engineVersion = VK_MAKE_VERSION( 1, 0, 0 );
	appInfo.apiVersion = VK_MAKE_VERSION( 1, 0, 0 );

	VkInstanceCreateInfo instanceCreateInfo;
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pNext = NULL;
	instanceCreateInfo.flags = 0;
	instanceCreateInfo.pApplicationInfo = &appInfo;

	// Enable every advertised instance extension -- safe since the driver/loader advertises them.
	uint32_t nInsExt = 0;
	VK_CHECK( qvkEnumerateInstanceExtensionProperties( NULL, &nInsExt, NULL ) );

	VkExtensionProperties *pInsExt = (VkExtensionProperties *) malloc( sizeof( VkExtensionProperties ) * nInsExt );
	const char **ppInstanceExt = (const char **) malloc( sizeof( char * ) * nInsExt );

	VK_CHECK( qvkEnumerateInstanceExtensionProperties( NULL, &nInsExt, pInsExt ) );

	ri.Printf( PRINT_ALL, "--- Total %d instance extensions. ---\n", nInsExt );

	uint32_t i;
	for ( i = 0; i < nInsExt; i++ ) {
		ppInstanceExt[i] = pInsExt[i].extensionName;
	}

	instanceCreateInfo.enabledExtensionCount = nInsExt;
	instanceCreateInfo.ppEnabledExtensionNames = ppInstanceExt;

#ifndef NDEBUG
	static const char *const validation_layer_name = "VK_LAYER_KHRONOS_validation";
	if ( vk_validationLayerAvailable( validation_layer_name ) ) {
		ri.Printf( PRINT_ALL, "Using VK_LAYER_KHRONOS_validation\n" );
		instanceCreateInfo.enabledLayerCount = 1;
		instanceCreateInfo.ppEnabledLayerNames = &validation_layer_name;
	} else {
		ri.Printf( PRINT_ALL, "VK_LAYER_KHRONOS_validation not found (Vulkan SDK not installed?), skipping\n" );
		instanceCreateInfo.enabledLayerCount = 0;
		instanceCreateInfo.ppEnabledLayerNames = NULL;
	}
#else
	instanceCreateInfo.enabledLayerCount = 0;
	instanceCreateInfo.ppEnabledLayerNames = NULL;
#endif

	VkResult e = qvkCreateInstance( &instanceCreateInfo, NULL, &vk.instance );
	if ( e == VK_SUCCESS ) {
		ri.Printf( PRINT_ALL, "--- Vulkan create instance success! ---\n\n" );
	} else if ( e == VK_ERROR_INCOMPATIBLE_DRIVER ) {
		ri.Error( ERR_FATAL, "The requested version of Vulkan is not supported by the driver." );
	} else if ( e == VK_ERROR_EXTENSION_NOT_PRESENT ) {
		ri.Error( ERR_FATAL, "Cannot find a specified extension library." );
	} else {
		ri.Error( ERR_FATAL, "%d, returned by qvkCreateInstance.", e );
	}

	free( ppInstanceExt );
	free( pInsExt );
}


static void vk_loadGlobalFunctions( void ) {
	ri.Printf( PRINT_ALL, " Loading vulkan instance functions \n" );

	vk_getInstanceProcAddrImpl();

	#define INIT_INSTANCE_FUNCTION(func)                                \
	q##func = (PFN_ ## func)qvkGetInstanceProcAddr(vk.instance, #func); \
	if (q##func == NULL) {                                              \
		ri.Error(ERR_FATAL, "Failed to find entrypoint %s", #func);     \
	}

	INIT_INSTANCE_FUNCTION( vkCreateInstance )
	INIT_INSTANCE_FUNCTION( vkEnumerateInstanceExtensionProperties )
	INIT_INSTANCE_FUNCTION( vkEnumerateInstanceLayerProperties )

	// Get instance level functions.
	vk_createInstance();

	INIT_INSTANCE_FUNCTION( vkCreateDevice )
	INIT_INSTANCE_FUNCTION( vkDestroyInstance )
	INIT_INSTANCE_FUNCTION( vkDestroySurfaceKHR )
	INIT_INSTANCE_FUNCTION( vkEnumerateDeviceExtensionProperties )
	INIT_INSTANCE_FUNCTION( vkEnumeratePhysicalDevices )
	INIT_INSTANCE_FUNCTION( vkGetDeviceProcAddr )

	INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceFeatures )
	INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceProperties )
	INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceFormatProperties )
	INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceMemoryProperties )
	INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceQueueFamilyProperties )

	INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceSurfaceCapabilitiesKHR )
	INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceSurfaceFormatsKHR )
	INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceSurfacePresentModesKHR )
	INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceSurfaceSupportKHR )

#ifndef NDEBUG
	if ( vk_validationLayerAvailable( "VK_LAYER_KHRONOS_validation" ) ) {
		INIT_INSTANCE_FUNCTION( vkCreateDebugReportCallbackEXT )
		INIT_INSTANCE_FUNCTION( vkDestroyDebugReportCallbackEXT )
	}
#endif

	#undef INIT_INSTANCE_FUNCTION

	ri.Printf( PRINT_ALL, " Init global functions done. \n" );
}


static void vk_selectPhysicalDevice( void ) {
	uint32_t gpu_count = 0;
	qvkEnumeratePhysicalDevices( vk.instance, &gpu_count, NULL );

	if ( gpu_count <= 0 ) {
		ri.Error( ERR_FATAL, "Vulkan: no physical device found" );
	}

	VkPhysicalDevice *pPhyDev = (VkPhysicalDevice *) malloc( sizeof( VkPhysicalDevice ) * gpu_count );

	// TODO: multi graphics card selection support.
	VK_CHECK( qvkEnumeratePhysicalDevices( vk.instance, &gpu_count, pPhyDev ) );
	// Just grab the first physical device for now.
	vk.physical_device = pPhyDev[0];

	free( pPhyDev );

	ri.Printf( PRINT_ALL, " Total %d graphics card, the first one is chosen. \n", gpu_count );

	qvkGetPhysicalDeviceMemoryProperties( vk.physical_device, &vk.devMemProperties );
}


static void vk_selectSurfaceFormat( void ) {
	uint32_t nSurfmt;

	VK_CHECK( qvkGetPhysicalDeviceSurfaceFormatsKHR( vk.physical_device, vk.surface, &nSurfmt, NULL ) );
	if ( nSurfmt == 0 ) {
		ri.Error( ERR_FATAL, "Vulkan: no surface formats supported" );
	}

	VkSurfaceFormatKHR *pSurfFmts = (VkSurfaceFormatKHR *) malloc( nSurfmt * sizeof( VkSurfaceFormatKHR ) );
	VK_CHECK( qvkGetPhysicalDeviceSurfaceFormatsKHR( vk.physical_device, vk.surface, &nSurfmt, pSurfFmts ) );

	if ( ( nSurfmt == 1 ) && ( pSurfFmts[0].format == VK_FORMAT_UNDEFINED ) ) {
		// special case that means we can choose any format
		vk.surface_format.format = VK_FORMAT_B8G8R8A8_UNORM;
		vk.surface_format.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
	} else {
		uint32_t i;
		for ( i = 0; i < nSurfmt; i++ ) {
			if ( ( pSurfFmts[i].format == VK_FORMAT_B8G8R8A8_UNORM ) &&
				 ( pSurfFmts[i].colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR ) ) {
				vk.surface_format = pSurfFmts[i];
				break;
			}
		}
		if ( i == nSurfmt ) {
			vk.surface_format = pSurfFmts[0];
		}
	}

	ri.Printf( PRINT_ALL, " Surface format: %d, colorSpace: %d\n", vk.surface_format.format, vk.surface_format.colorSpace );

	free( pSurfFmts );

	VK_CHECK( qvkGetPhysicalDeviceSurfaceCapabilitiesKHR( vk.physical_device, vk.surface, &vk.surface_caps ) );

	if ( ( vk.surface_caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT ) == 0 ) {
		ri.Error( ERR_FATAL, "VK_IMAGE_USAGE_TRANSFER_DST_BIT is not supported by your GPU." );
	}
}


static void vk_selectQueueFamilyForPresentation( void ) {
	uint32_t nQueueFam;
	qvkGetPhysicalDeviceQueueFamilyProperties( vk.physical_device, &nQueueFam, NULL );

	if ( nQueueFam == 0 ) {
		ri.Error( ERR_FATAL, "Vulkan: no queue families found" );
	}

	VkQueueFamilyProperties *pQueueFamilies = (VkQueueFamilyProperties *) malloc( nQueueFam * sizeof( VkQueueFamilyProperties ) );
	qvkGetPhysicalDeviceQueueFamilyProperties( vk.physical_device, &nQueueFam, pQueueFamilies );

	vk.queue_family_index = (uint32_t)-1;

	uint32_t i;
	for ( i = 0; i < nQueueFam; ++i ) {
		VkBool32 presentation_supported = VK_FALSE;
		VK_CHECK( qvkGetPhysicalDeviceSurfaceSupportKHR( vk.physical_device, i, vk.surface, &presentation_supported ) );

		if ( presentation_supported && ( pQueueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT ) != 0 ) {
			vk.queue_family_index = i;
			ri.Printf( PRINT_ALL, " Queue family for presentation selected: %d\n", vk.queue_family_index );
			break;
		}
	}

	free( pQueueFamilies );

	if ( vk.queue_family_index == (uint32_t)-1 ) {
		ri.Error( ERR_FATAL, "Vulkan: failed to find queue family" );
	}
}


static void vk_createLogicalDevice( void ) {
	static const char *device_extensions[1] = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	uint32_t nDevExts = 0;
	VkBool32 swapchainExtFound = VK_FALSE;

	qvkEnumerateDeviceExtensionProperties( vk.physical_device, NULL, &nDevExts, NULL );

	VkExtensionProperties *pDeviceExt = (VkExtensionProperties *) malloc( sizeof( VkExtensionProperties ) * nDevExts );
	qvkEnumerateDeviceExtensionProperties( vk.physical_device, NULL, &nDevExts, pDeviceExt );

	uint32_t j;
	for ( j = 0; j < nDevExts; j++ ) {
		if ( !strcmp( device_extensions[0], pDeviceExt[j].extensionName ) ) {
			swapchainExtFound = VK_TRUE;
			break;
		}
	}
	free( pDeviceExt );

	if ( !swapchainExtFound ) {
		ri.Error( ERR_FATAL, "VK_KHR_SWAPCHAIN_EXTENSION_NAME is not available" );
	}

	const float priority = 1.0f;
	VkDeviceQueueCreateInfo queue_desc;
	queue_desc.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queue_desc.pNext = NULL;
	queue_desc.flags = 0;
	queue_desc.queueFamilyIndex = vk.queue_family_index;
	queue_desc.queueCount = 1;
	queue_desc.pQueuePriorities = &priority;

	qvkGetPhysicalDeviceFeatures( vk.physical_device, &vk.features );

	VkDeviceCreateInfo device_desc;
	device_desc.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	device_desc.pNext = NULL;
	device_desc.flags = 0;
	device_desc.queueCreateInfoCount = 1;
	device_desc.pQueueCreateInfos = &queue_desc;
	device_desc.enabledLayerCount = 0;
	device_desc.ppEnabledLayerNames = NULL;
	device_desc.enabledExtensionCount = 1;
	device_desc.ppEnabledExtensionNames = device_extensions;
	device_desc.pEnabledFeatures = &vk.features;

	ri.Printf( PRINT_ALL, " Create logical device: vk.device \n" );
	VK_CHECK( qvkCreateDevice( vk.physical_device, &device_desc, NULL, &vk.device ) );
}


static void vk_loadDeviceFunctions( void ) {
	ri.Printf( PRINT_ALL, " Loading device level function. \n" );

	#define INIT_DEVICE_FUNCTION(func)                              \
	q##func = (PFN_ ## func)qvkGetDeviceProcAddr(vk.device, #func); \
	if (q##func == NULL) {                                          \
		ri.Error(ERR_FATAL, "Failed to find entrypoint %s", #func); \
	}

	INIT_DEVICE_FUNCTION( vkAllocateCommandBuffers )
	INIT_DEVICE_FUNCTION( vkAllocateDescriptorSets )
	INIT_DEVICE_FUNCTION( vkAllocateMemory )
	INIT_DEVICE_FUNCTION( vkBeginCommandBuffer )
	INIT_DEVICE_FUNCTION( vkBindBufferMemory )
	INIT_DEVICE_FUNCTION( vkBindImageMemory )
	INIT_DEVICE_FUNCTION( vkCmdBeginRenderPass )
	INIT_DEVICE_FUNCTION( vkCmdBindDescriptorSets )
	INIT_DEVICE_FUNCTION( vkCmdBindIndexBuffer )
	INIT_DEVICE_FUNCTION( vkCmdBindPipeline )
	INIT_DEVICE_FUNCTION( vkCmdBindVertexBuffers )
	INIT_DEVICE_FUNCTION( vkCmdBlitImage )
	INIT_DEVICE_FUNCTION( vkCmdClearAttachments )
	INIT_DEVICE_FUNCTION( vkCmdClearColorImage )
	INIT_DEVICE_FUNCTION( vkCmdCopyBuffer )
	INIT_DEVICE_FUNCTION( vkCmdCopyBufferToImage )
	INIT_DEVICE_FUNCTION( vkCmdCopyImage )
	INIT_DEVICE_FUNCTION( vkCmdCopyImageToBuffer )
	INIT_DEVICE_FUNCTION( vkCmdDraw )
	INIT_DEVICE_FUNCTION( vkCmdDrawIndexed )
	INIT_DEVICE_FUNCTION( vkCmdEndRenderPass )
	INIT_DEVICE_FUNCTION( vkCmdPipelineBarrier )
	INIT_DEVICE_FUNCTION( vkCmdPushConstants )
	INIT_DEVICE_FUNCTION( vkCmdSetDepthBias )
	INIT_DEVICE_FUNCTION( vkCmdSetScissor )
	INIT_DEVICE_FUNCTION( vkCmdSetViewport )
	INIT_DEVICE_FUNCTION( vkCreateBuffer )
	INIT_DEVICE_FUNCTION( vkCreateCommandPool )
	INIT_DEVICE_FUNCTION( vkCreateDescriptorPool )
	INIT_DEVICE_FUNCTION( vkCreateDescriptorSetLayout )
	INIT_DEVICE_FUNCTION( vkCreateFence )
	INIT_DEVICE_FUNCTION( vkCreateFramebuffer )
	INIT_DEVICE_FUNCTION( vkCreateGraphicsPipelines )
	INIT_DEVICE_FUNCTION( vkCreateImage )
	INIT_DEVICE_FUNCTION( vkCreateImageView )
	INIT_DEVICE_FUNCTION( vkCreatePipelineLayout )
	INIT_DEVICE_FUNCTION( vkCreateRenderPass )
	INIT_DEVICE_FUNCTION( vkCreateSampler )
	INIT_DEVICE_FUNCTION( vkCreateSemaphore )
	INIT_DEVICE_FUNCTION( vkCreateShaderModule )
	INIT_DEVICE_FUNCTION( vkDestroyBuffer )
	INIT_DEVICE_FUNCTION( vkDestroyCommandPool )
	INIT_DEVICE_FUNCTION( vkDestroyDescriptorPool )
	INIT_DEVICE_FUNCTION( vkDestroyDescriptorSetLayout )
	INIT_DEVICE_FUNCTION( vkDestroyDevice )
	INIT_DEVICE_FUNCTION( vkDestroyFence )
	INIT_DEVICE_FUNCTION( vkDestroyFramebuffer )
	INIT_DEVICE_FUNCTION( vkDestroyImage )
	INIT_DEVICE_FUNCTION( vkDestroyImageView )
	INIT_DEVICE_FUNCTION( vkDestroyPipeline )
	INIT_DEVICE_FUNCTION( vkDestroyPipelineLayout )
	INIT_DEVICE_FUNCTION( vkDestroyRenderPass )
	INIT_DEVICE_FUNCTION( vkDestroySampler )
	INIT_DEVICE_FUNCTION( vkDestroySemaphore )
	INIT_DEVICE_FUNCTION( vkDestroyShaderModule )
	INIT_DEVICE_FUNCTION( vkDeviceWaitIdle )
	INIT_DEVICE_FUNCTION( vkEndCommandBuffer )
	INIT_DEVICE_FUNCTION( vkFreeCommandBuffers )
	INIT_DEVICE_FUNCTION( vkFreeDescriptorSets )
	INIT_DEVICE_FUNCTION( vkFreeMemory )
	INIT_DEVICE_FUNCTION( vkGetBufferMemoryRequirements )
	INIT_DEVICE_FUNCTION( vkGetDeviceQueue )
	INIT_DEVICE_FUNCTION( vkGetImageMemoryRequirements )
	INIT_DEVICE_FUNCTION( vkGetImageSubresourceLayout )
	INIT_DEVICE_FUNCTION( vkMapMemory )
	INIT_DEVICE_FUNCTION( vkUnmapMemory )
	INIT_DEVICE_FUNCTION( vkQueueSubmit )
	INIT_DEVICE_FUNCTION( vkQueueWaitIdle )
	INIT_DEVICE_FUNCTION( vkResetDescriptorPool )
	INIT_DEVICE_FUNCTION( vkResetFences )
	INIT_DEVICE_FUNCTION( vkUpdateDescriptorSets )
	INIT_DEVICE_FUNCTION( vkWaitForFences )

	INIT_DEVICE_FUNCTION( vkCreateSwapchainKHR )
	INIT_DEVICE_FUNCTION( vkDestroySwapchainKHR )
	INIT_DEVICE_FUNCTION( vkGetSwapchainImagesKHR )
	INIT_DEVICE_FUNCTION( vkAcquireNextImageKHR )
	INIT_DEVICE_FUNCTION( vkQueuePresentKHR )

	#undef INIT_DEVICE_FUNCTION
}


void vk_getProcAddress( void ) {
	vk_loadGlobalFunctions();

#ifndef NDEBUG
	if ( qvkCreateDebugReportCallbackEXT ) {
		vk_createDebugCallback( vk_DebugCallback );
	}
#endif

	// Surface must be created right after the instance -- it can influence presentation mode and device selection.
	vk_createSurfaceImpl();

	vk_selectPhysicalDevice();
	vk_selectSurfaceFormat();
	vk_selectQueueFamilyForPresentation();
	vk_createLogicalDevice();

	vk_loadDeviceFunctions();

	qvkGetDeviceQueue( vk.device, vk.queue_family_index, 0, &vk.queue );
}


void vk_clearProcAddress( void ) {
	ri.Printf( PRINT_ALL, " Destroy logical device: vk.device. \n" );
	qvkDestroyDevice( vk.device, NULL );

	ri.Printf( PRINT_ALL, " Destroy surface: vk.surface. \n" );
	qvkDestroySurfaceKHR( vk.instance, vk.surface, NULL );

#ifndef NDEBUG
	if ( qvkDestroyDebugReportCallbackEXT && vk.h_debugCB ) {
		qvkDestroyDebugReportCallbackEXT( vk.instance, vk.h_debugCB, NULL );
	}
#endif

	ri.Printf( PRINT_ALL, " Destroy instance: vk.instance. \n" );
	qvkDestroyInstance( vk.instance, NULL );
}


const char *cvtResToStr( VkResult result ) {
	switch ( result ) {
		case VK_SUCCESS: return "VK_SUCCESS";
		case VK_NOT_READY: return "VK_NOT_READY";
		case VK_TIMEOUT: return "VK_TIMEOUT";
		case VK_EVENT_SET: return "VK_EVENT_SET";
		case VK_EVENT_RESET: return "VK_EVENT_RESET";
		case VK_INCOMPLETE: return "VK_INCOMPLETE";
		case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
		case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
		case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
		case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
		case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
		case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
		case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
		case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
		case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
		case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
		case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
		case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
		case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
		case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
		case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
		case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
		case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
		case VK_ERROR_VALIDATION_FAILED_EXT: return "VK_ERROR_VALIDATION_FAILED_EXT";
		default: return "UNKNOWN_ERROR";
	}
}
