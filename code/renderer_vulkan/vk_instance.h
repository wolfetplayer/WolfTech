// vk_instance.h -- Vulkan instance/device/swapchain state and function-pointer table, adapted from vkQuake3 (GPLv2-or-later).

#ifndef VK_INSTANCE_H_
#define VK_INSTANCE_H_

#include "VKimpl.h"
#include "tr_local.h"

extern PFN_vkGetInstanceProcAddr                       qvkGetInstanceProcAddr;

extern PFN_vkEnumerateInstanceLayerProperties          qvkEnumerateInstanceLayerProperties;
extern PFN_vkCreateInstance                            qvkCreateInstance;
extern PFN_vkEnumerateInstanceExtensionProperties      qvkEnumerateInstanceExtensionProperties;

extern PFN_vkCreateDevice                              qvkCreateDevice;
extern PFN_vkDestroyInstance                           qvkDestroyInstance;
extern PFN_vkEnumerateDeviceExtensionProperties        qvkEnumerateDeviceExtensionProperties;
extern PFN_vkEnumeratePhysicalDevices                  qvkEnumeratePhysicalDevices;
extern PFN_vkGetDeviceProcAddr                          qvkGetDeviceProcAddr;
extern PFN_vkGetPhysicalDeviceFeatures                 qvkGetPhysicalDeviceFeatures;
extern PFN_vkGetPhysicalDeviceFormatProperties         qvkGetPhysicalDeviceFormatProperties;
extern PFN_vkGetPhysicalDeviceMemoryProperties         qvkGetPhysicalDeviceMemoryProperties;
extern PFN_vkGetPhysicalDeviceProperties               qvkGetPhysicalDeviceProperties;
extern PFN_vkGetPhysicalDeviceQueueFamilyProperties    qvkGetPhysicalDeviceQueueFamilyProperties;
extern PFN_vkDestroySurfaceKHR                         qvkDestroySurfaceKHR;
extern PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR   qvkGetPhysicalDeviceSurfaceCapabilitiesKHR;
extern PFN_vkGetPhysicalDeviceSurfaceFormatsKHR        qvkGetPhysicalDeviceSurfaceFormatsKHR;
extern PFN_vkGetPhysicalDeviceSurfacePresentModesKHR   qvkGetPhysicalDeviceSurfacePresentModesKHR;
extern PFN_vkGetPhysicalDeviceSurfaceSupportKHR        qvkGetPhysicalDeviceSurfaceSupportKHR;

#ifndef NDEBUG
extern PFN_vkCreateDebugReportCallbackEXT              qvkCreateDebugReportCallbackEXT;
extern PFN_vkDestroyDebugReportCallbackEXT             qvkDestroyDebugReportCallbackEXT;
#endif

extern PFN_vkAllocateCommandBuffers                    qvkAllocateCommandBuffers;
extern PFN_vkAllocateDescriptorSets                    qvkAllocateDescriptorSets;
extern PFN_vkAllocateMemory                            qvkAllocateMemory;
extern PFN_vkBeginCommandBuffer                        qvkBeginCommandBuffer;
extern PFN_vkBindBufferMemory                          qvkBindBufferMemory;
extern PFN_vkBindImageMemory                           qvkBindImageMemory;
extern PFN_vkCmdBeginRenderPass                        qvkCmdBeginRenderPass;
extern PFN_vkCmdBindDescriptorSets                     qvkCmdBindDescriptorSets;
extern PFN_vkCmdBindIndexBuffer                        qvkCmdBindIndexBuffer;
extern PFN_vkCmdBindPipeline                           qvkCmdBindPipeline;
extern PFN_vkCmdBindVertexBuffers                      qvkCmdBindVertexBuffers;
extern PFN_vkCmdBeginQuery                             qvkCmdBeginQuery;
extern PFN_vkCmdEndQuery                               qvkCmdEndQuery;
extern PFN_vkCmdResetQueryPool                         qvkCmdResetQueryPool;
extern PFN_vkCmdBlitImage                              qvkCmdBlitImage;
extern PFN_vkCmdClearAttachments                       qvkCmdClearAttachments;
extern PFN_vkCmdClearColorImage                        qvkCmdClearColorImage;
extern PFN_vkCmdCopyBuffer                             qvkCmdCopyBuffer;
extern PFN_vkCmdCopyBufferToImage                      qvkCmdCopyBufferToImage;
extern PFN_vkCmdCopyImage                              qvkCmdCopyImage;
extern PFN_vkCmdCopyImageToBuffer                      qvkCmdCopyImageToBuffer;
extern PFN_vkCmdDraw                                   qvkCmdDraw;
extern PFN_vkCmdDrawIndexed                            qvkCmdDrawIndexed;
extern PFN_vkCmdEndRenderPass                          qvkCmdEndRenderPass;
extern PFN_vkCmdPipelineBarrier                        qvkCmdPipelineBarrier;
extern PFN_vkCmdPushConstants                          qvkCmdPushConstants;
extern PFN_vkCmdSetDepthBias                           qvkCmdSetDepthBias;
extern PFN_vkCmdSetScissor                             qvkCmdSetScissor;
extern PFN_vkCmdSetViewport                            qvkCmdSetViewport;
extern PFN_vkCreateBuffer                              qvkCreateBuffer;
extern PFN_vkCreateCommandPool                         qvkCreateCommandPool;
extern PFN_vkCreateDescriptorPool                      qvkCreateDescriptorPool;
extern PFN_vkCreateDescriptorSetLayout                 qvkCreateDescriptorSetLayout;
extern PFN_vkCreateFence                               qvkCreateFence;
extern PFN_vkCreateFramebuffer                         qvkCreateFramebuffer;
extern PFN_vkCreateGraphicsPipelines                   qvkCreateGraphicsPipelines;
extern PFN_vkCreateImage                               qvkCreateImage;
extern PFN_vkCreateImageView                           qvkCreateImageView;
extern PFN_vkCreatePipelineLayout                      qvkCreatePipelineLayout;
extern PFN_vkCreateRenderPass                          qvkCreateRenderPass;
extern PFN_vkCreateSampler                             qvkCreateSampler;
extern PFN_vkCreateSemaphore                           qvkCreateSemaphore;
extern PFN_vkCreateShaderModule                        qvkCreateShaderModule;
extern PFN_vkCreateQueryPool                            qvkCreateQueryPool;
extern PFN_vkDestroyQueryPool                           qvkDestroyQueryPool;
extern PFN_vkGetQueryPoolResults                        qvkGetQueryPoolResults;
extern PFN_vkDestroyBuffer                             qvkDestroyBuffer;
extern PFN_vkDestroyCommandPool                        qvkDestroyCommandPool;
extern PFN_vkDestroyDescriptorPool                     qvkDestroyDescriptorPool;
extern PFN_vkDestroyDescriptorSetLayout                qvkDestroyDescriptorSetLayout;
extern PFN_vkDestroyDevice                             qvkDestroyDevice;
extern PFN_vkDestroyFence                              qvkDestroyFence;
extern PFN_vkDestroyFramebuffer                        qvkDestroyFramebuffer;
extern PFN_vkDestroyImage                              qvkDestroyImage;
extern PFN_vkDestroyImageView                          qvkDestroyImageView;
extern PFN_vkDestroyPipeline                           qvkDestroyPipeline;
extern PFN_vkDestroyPipelineLayout                     qvkDestroyPipelineLayout;
extern PFN_vkDestroyRenderPass                         qvkDestroyRenderPass;
extern PFN_vkDestroySampler                            qvkDestroySampler;
extern PFN_vkDestroySemaphore                          qvkDestroySemaphore;
extern PFN_vkDestroyShaderModule                       qvkDestroyShaderModule;
extern PFN_vkDeviceWaitIdle                            qvkDeviceWaitIdle;
extern PFN_vkEndCommandBuffer                          qvkEndCommandBuffer;
extern PFN_vkFreeCommandBuffers                        qvkFreeCommandBuffers;
extern PFN_vkFreeDescriptorSets                        qvkFreeDescriptorSets;
extern PFN_vkFreeMemory                                qvkFreeMemory;
extern PFN_vkGetBufferMemoryRequirements               qvkGetBufferMemoryRequirements;
extern PFN_vkGetDeviceQueue                            qvkGetDeviceQueue;
extern PFN_vkGetImageMemoryRequirements                qvkGetImageMemoryRequirements;
extern PFN_vkGetImageSubresourceLayout                 qvkGetImageSubresourceLayout;
extern PFN_vkMapMemory                                 qvkMapMemory;
extern PFN_vkUnmapMemory                               qvkUnmapMemory;
extern PFN_vkQueueSubmit                               qvkQueueSubmit;
extern PFN_vkQueueWaitIdle                              qvkQueueWaitIdle;
extern PFN_vkResetDescriptorPool                       qvkResetDescriptorPool;
extern PFN_vkResetFences                               qvkResetFences;
extern PFN_vkUpdateDescriptorSets                      qvkUpdateDescriptorSets;
extern PFN_vkWaitForFences                             qvkWaitForFences;
extern PFN_vkAcquireNextImageKHR                       qvkAcquireNextImageKHR;
extern PFN_vkCreateSwapchainKHR                        qvkCreateSwapchainKHR;
extern PFN_vkDestroySwapchainKHR                       qvkDestroySwapchainKHR;
extern PFN_vkGetSwapchainImagesKHR                     qvkGetSwapchainImagesKHR;
extern PFN_vkQueuePresentKHR                           qvkQueuePresentKHR;

// Initializes/releases the Vk_Instance structure and its function pointer table.
void vk_getProcAddress( void );
void vk_clearProcAddress( void );

const char *cvtResToStr( VkResult result );

#ifndef NDEBUG
#define VK_CHECK(function_call) { \
	VkResult result = function_call; \
	if (result != VK_SUCCESS) \
		ri.Printf(PRINT_ALL, \
		"Vulkan: error %s returned by %s \n", cvtResToStr(result), #function_call); \
}
#else
#define VK_CHECK(function_call) \
	function_call;
#endif

#define MAX_SWAPCHAIN_IMAGES    8

// How many frames' worth of command buffers/fences/semaphores (and, per subsystem, dynamic-buffer regions) overlap in flight; see vk_frame.c.
#define VK_FRAMES_IN_FLIGHT     2

// Engine-wide Vulkan state, set up/torn down by vk_initialize/vk_shutdown.
struct Vk_Instance {
	VkInstance instance;
	VkPhysicalDevice physical_device;
	VkPhysicalDeviceFeatures features;
	VkPhysicalDeviceProperties deviceProperties;

	VkSurfaceKHR surface;
	VkSurfaceFormatKHR surface_format;
	VkSurfaceCapabilitiesKHR surface_caps;

	VkPhysicalDeviceMemoryProperties devMemProperties;

	uint32_t queue_family_index;
	VkDevice device;
	VkQueue queue;

	VkSwapchainKHR swapchain;
	VkExtent2D swapchain_extent; // the actual extent vk_createSwapChain picked -- see vk_recreateSwapChain
	uint32_t swapchain_image_count;
	VkImage swapchain_images_array[MAX_SWAPCHAIN_IMAGES];
	VkImageView swapchain_image_views[MAX_SWAPCHAIN_IMAGES];
	uint32_t idx_swapchain_image;

	VkCommandPool command_pool;
	VkCommandBuffer command_buffers[VK_FRAMES_IN_FLIGHT]; // allocated once; vk_begin_frame selects one into command_buffer
	VkCommandBuffer command_buffer; // the slot vk_current_frame_slot() owns this frame -- everything else in the renderer just uses this

	VkFormat depth_format;
	VkImage depth_image;
	VkDeviceMemory depth_memory;
	VkImageView depth_view;

	VkRenderPass render_pass;
	VkFramebuffer framebuffers[MAX_SWAPCHAIN_IMAGES];

	VkDescriptorPool descriptor_pool;
	VkDescriptorSetLayout set_layout;
	VkPipelineLayout pipeline_layout;

	// 2 bindings (diffuse + lightmap) for the world's lightmapped draw path; sets come from vk_world.c's own pool.
	VkDescriptorSetLayout lightmap_set_layout;
	VkPipelineLayout lightmap_pipeline_layout;

	VkPipeline pipeline_2d_opaque;
	VkPipeline pipeline_2d_blend;
	VkPipeline pipeline_3d_opaque; // fallback only; see vk_getPipeline3D
	VkPipeline pipeline_3d_blend;  // ditto, blended twin
	VkPipeline pipeline_3d_lightmap[3]; // indexed by cullType_t
	VkPipeline pipeline_3d_lightmap_portal[3]; // ditto, world-space clip plane active, no cull flip -- see tr_portal.h
	VkPipeline pipeline_3d_lightmap_mirror[3]; // ditto, clip plane + cull flip (proper reflection)
	VkPipeline pipeline_sky; // single_texture shader, 2D vertex layout, depth test/write off, never fogged
	VkPipeline pipeline_world_wireframe; // r_showtris overlay; VK_NULL_HANDLE if the device lacks fillModeNonSolid
	VkPipeline pipeline_occlusion_probe; // coronas' visibility test; see vk_flares.c
	VkPipeline pipeline_shadow_volume_incr; // stencil shadow volumes' back-face stamp; see vk_shadows.c
	VkPipeline pipeline_shadow_volume_decr; // ditto, front-face stamp
	VkPipeline pipeline_shadow_darken; // ditto, full-screen finish pass

	VkBool32 isInitialized;

#ifndef NDEBUG
	VkDebugReportCallbackEXT h_debugCB;
#endif
};

extern struct Vk_Instance vk;

#endif // VK_INSTANCE_H_
