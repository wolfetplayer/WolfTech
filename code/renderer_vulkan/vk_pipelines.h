// vk_pipelines.h -- render pass, framebuffers, pipeline layout, and the 2D/3D pipeline variants.

#ifndef VK_PIPELINES_H_
#define VK_PIPELINES_H_

#include "tr_shader.h" // cullType_t

// Single interleaved binding: position, RGBA8 color (byte[4] to dodge endianness), UV.
typedef struct {
	float pos[3];
	byte color[4];
	float uv[2];
} vk2d_vertex_t;

// World geometry: same layout as vk2d_vertex_t plus a second (lightmap) UV set.
typedef struct {
	float pos[3];
	byte color[4];
	float uv[2];
	float lmUv[2];
} vkWorldVertex_t;

void vk_createRenderPass( void );
void vk_destroyRenderPass( void );

void vk_createFrameBuffers( uint32_t width, uint32_t height );
void vk_destroyFrameBuffers( void );

void vk_createPipelineLayout( void );
void vk_destroyPipelineLayout( void );

void vk_create2DPipelines( void );
void vk_destroy2DPipelines( void );

void vk_create3DPipelines( void );
void vk_destroy3DPipelines( void );

// cullType_t -> the matching Vulkan cull mode (see vk_pipelines.c).
VkCullModeFlags vk_cullModeForShader( cullType_t cullType );

// Lazily creates/caches a single_texture pipeline for this exact state (depth test always on).
VkPipeline vk_getPipeline3D( VkBool32 blendEnable, VkBlendFactor srcBlend, VkBlendFactor dstBlend,
							  VkCullModeFlags cullMode, int32_t alphaTestFunc );

#endif // VK_PIPELINES_H_
