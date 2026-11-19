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

// Two independent UV sets, one per collapsed stage; see vk_getPipelineMultiTexture.
typedef struct {
	float pos[3];
	byte color[4];
	float uv0[2];
	float uv1[2];
} vkMultiTexVertex_t;

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

// Lazily creates/caches a single_texture pipeline for this state; vertexStride retargets the binding to vk2d_vertex_t-compatible layouts like vkWorldVertex_t.
// clipPlane/mirror activate tr_portal.h's clip plane (and, for mirror, a flipped cull mode); pass VK_FALSE/VK_FALSE outside a world mirror/portal pass.
VkPipeline vk_getPipeline3D( VkDeviceSize vertexStride, VkBool32 blendEnable, VkBlendFactor srcBlend, VkBlendFactor dstBlend,
							  VkCullModeFlags cullMode, int32_t alphaTestFunc, VkBool32 clipPlane, VkBool32 mirror );

// vk.pipeline_3d_lightmap[cullType], auto-swapped for the clip-plane/mirror variant while tr_portal.c is recursing.
VkPipeline vk_getWorldLightmapPipeline( cullType_t cullType );

// Lazily creates/caches a multi_texture pipeline (two samplers combined in one pass, always opaque); combineMode: 0 = modulate, 1 = add.
VkPipeline vk_getPipelineMultiTexture( int combineMode, VkCullModeFlags cullMode, int32_t alphaTestFunc );

// True if shader's exactly-two stages are an opaque stage 0 plus a trivially-tinted modulate/add stage 1, i.e. collapsible into one vk_getPipelineMultiTexture draw; sets *outCombineMode when true.
qboolean vk_canCollapseMultiTex( const shader_t *shader, int *outCombineMode );

// Lazily creates/caches a 2-binding (img0, img1) descriptor set for vk_getPipelineMultiTexture's layout; sets are never freed individually, same as image_t's own descriptor.
VkDescriptorSet vk_getMultiTextureDescriptor( struct image_s *img0, struct image_s *img1 );

#endif // VK_PIPELINES_H_
