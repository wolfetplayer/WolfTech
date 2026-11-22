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

// World geometry: vk2d_vertex_t's layout plus a lightmap UV set and packed snorm16 normal/tangent/lightdir (VK_FORMAT_R16G16B16A16_SNORM auto-unpacks to float in the shader; xyz used, w is padding except tangent's, which is the bitangent handedness sign). normal is real (threaded from vk_world.c's BSP/patch/brush-entity data); tangent/lightdir are zeroed until their own later roadmap items (tangent precompute, light-grid sampling) populate them.
typedef struct {
	float pos[3];
	byte color[4];
	float uv[2];
	float lmUv[2];
	int16_t normal[4];
	int16_t tangent[4];
	int16_t lightdir[4];
} vkWorldVertex_t;

// Two independent UV sets, one per collapsed stage; see vk_getPipelineMultiTexture.
typedef struct {
	float pos[3];
	byte color[4];
	float uv0[2];
	float uv1[2];
} vkMultiTexVertex_t;

// One frame of a GPU-animated entity surface (MD3/MDC vertex-lerp, or MDS/MDM bind pose): built once at model-load time into a static per-surface buffer (see vk_entity_register_model), never re-uploaded per-frame; normal/tangent are packed snorm16 like vkWorldVertex_t's, tangent's w is the handedness sign. Vertex-lerp models repeat this numFrames times back to back (frame f at byte offset f * numVerts * sizeof(vkEntityFrameVert_t)); bone-skinned models have exactly one (bind pose).
typedef struct {
	float pos[3];
	int16_t normal[4];
	int16_t tangent[4];
} vkEntityFrameVert_t;

// Per-frame dynamic data for an entity draw -- rgbGen/alphaGen/tcMod still run on the CPU every frame exactly as before, they just no longer carry position (that's vkEntityFrameVert_t's job now).
typedef struct {
	byte color[4];
	float uv[2];
} vkEntityDynamicVert_t;

// Dynamic twin of vkEntityDynamicVert_t for the collapsed multitex path (two UV sets, no second pos).
typedef struct {
	byte color[4];
	float uv0[2];
	float uv1[2];
} vkEntityDynamicMTVert_t;

// GPU-skinned entity surface vertex (MDS/MDM), built once at model-load time (see vk_entity_register_model), one entry per vertex with no per-frame dimension since bones animate it, not frame-blending. offset[k] is the vertex's position in bone boneIndex[k]'s own bind-pose-relative local frame (this file format's actual per-weight representation -- not a single shared bind-pose position transformed by a blended bone matrix like IQM-style GPU skinning), so the vertex shader must do a real per-slot matrix-vector transform (boneMatrix[boneIndex[k]] * offset[k] + translation) and weighted-sum the 4 results, not mix()-blend them -- this mirrors R_SkinMDSSurface's own CPU math (LocalAddScaledMatrixTransformVectorTranslate) exactly. boneWeight is unorm8 (0..255 = 0..1), reduced from the model's real (possibly >4) per-vertex weight list to the top 4 by magnitude and renormalized to sum 255; unused slots (fewer than 4 real weights) have boneWeight 0 and contribute nothing regardless of their (zeroed) offset/index.
typedef struct {
	float offset[4][3];
	int16_t normal[4];
	int16_t tangent[4];
	byte boneIndex[4];
	byte boneWeight[4];
} vkEntitySkinnedVert_t;

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
// polygonOffset should be shader_t.polygonOffset (decal shaders set this to avoid z-fighting).
VkPipeline vk_getPipeline3D( VkDeviceSize vertexStride, VkBool32 blendEnable, VkBlendFactor srcBlend, VkBlendFactor dstBlend,
							  VkCullModeFlags cullMode, int32_t alphaTestFunc, VkBool32 clipPlane, VkBool32 mirror,
							  VkBool32 polygonOffset );

// vk.pipeline_3d_lightmap[cullType], auto-swapped for the clip-plane/mirror variant while tr_portal.c is recursing.
VkPipeline vk_getWorldLightmapPipeline( cullType_t cullType );

// Lazily creates/caches a multi_texture pipeline (two samplers combined in one pass, always opaque); combineMode: 0 = modulate, 1 = add.
VkPipeline vk_getPipelineMultiTexture( int combineMode, VkCullModeFlags cullMode, int32_t alphaTestFunc );

// Lazily creates/caches the GPU vertex-lerp pipeline for MD3/MDC entities (vk_entity.c): 3 vertex bindings -- the model's static per-surface buffer bound twice (current/old frame) plus this frame's dynamic color/uv. No clipPlane/mirror axis -- entities never use either.
VkPipeline vk_getPipelineEntityLerp( VkBool32 blendEnable, VkBlendFactor srcBlend, VkBlendFactor dstBlend,
									   VkCullModeFlags cullMode, int32_t alphaTestFunc, VkBool32 polygonOffset );

// Multitex twin of the above, mirroring vk_getPipelineMultiTexture's (combine, cull, alphaTest) key.
VkPipeline vk_getPipelineEntityLerpMultiTex( int combineMode, VkCullModeFlags cullMode, int32_t alphaTestFunc );

// Lazily creates/caches the GPU bone-skinning pipeline for MDS/MDM entities (vk_entity.c): 2 vertex bindings -- the model's static bind-pose buffer plus this frame's dynamic color/uv. Uses vk.skin_pipeline_layout (set 1 = the bone-matrix UBO vk_entity.c owns).
VkPipeline vk_getPipelineEntitySkin( VkBool32 blendEnable, VkBlendFactor srcBlend, VkBlendFactor dstBlend,
									   VkCullModeFlags cullMode, int32_t alphaTestFunc, VkBool32 polygonOffset );

// Multitex twin of the above, mirroring vk_getPipelineMultiTexture's (combine, cull, alphaTest) key.
VkPipeline vk_getPipelineEntitySkinMultiTex( int combineMode, VkCullModeFlags cullMode, int32_t alphaTestFunc );

// True if shader's exactly-two stages are an opaque stage 0 plus a trivially-tinted modulate/add stage 1, i.e. collapsible into one vk_getPipelineMultiTexture draw; sets *outCombineMode when true.
qboolean vk_canCollapseMultiTex( const shader_t *shader, int *outCombineMode );

// Lazily creates/caches a 2-binding (img0, img1) descriptor set for vk_getPipelineMultiTexture's layout; sets are never freed individually, same as image_t's own descriptor.
VkDescriptorSet vk_getMultiTextureDescriptor( struct image_s *img0, struct image_s *img1 );

#endif // VK_PIPELINES_H_
