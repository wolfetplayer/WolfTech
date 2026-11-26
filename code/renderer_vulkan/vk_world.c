// vk_world.c -- sorts/batches tr_world's (shader, lightmap) surfaces once at map load, then expands each visible surface into a per-frame dynamic vertex buffer with real per-frame rgbGen/tcMod (like vk_entity.c/vk_poly.c).

#include <stdlib.h>
#include <string.h>

#include "VKimpl.h"
#include "vk_instance.h"
#include "vk_pipelines.h"
#include "vk_frame.h"
#include "vk_cmd.h"
#include "vk_image.h"
#include "tr_local.h"
#include "tr_shader.h"
#include "tr_shade_calc.h"
#include "tr_bsp.h"
#include "tr_world.h"
#include "tr_patch.h"
#include "tr_light.h"
#include "tr_scene.h"
#include "tr_portal.h"
#include "tr_main.h"
#include "vk_world.h"

// Same falloff shape vk_entity.c's ComputeEntityTint uses for dlight contributions.
#define DLIGHT_AT_RADIUS 16.0f
#define DLIGHT_MINIMUM_RADIUS 16.0f
#define DLIGHT_CULL_RADIUS_FACTOR 8.0f

// One entry per drawable surface; consecutive entries in a batch are contiguous in s_cachedIndices.
typedef struct {
	int surfaceIndex;
	uint32_t firstIndex;
	uint32_t indexCount;
	vec3_t mins, maxs; // world-space AABB of this surface's static (undeformed) vertices; used to cull the per-vertex dlight pass
} worldSurfRef_t;

// Keyed by (shader, lightmapIdx); lightmapIdx always points at a real lightmap or the synthetic white fallback.
typedef struct {
	qhandle_t shader;
	int lightmapIdx;
	int firstSurfRef;
	int numSurfRefs;
	VkDescriptorSet descriptor; // diffuse (binding 0) + lightmap (binding 1), built once at load time
	qboolean staticEligible;
} worldBatch_t;

typedef struct {
	VkImage image;
	VkDeviceMemory memory;
	VkImageView view;
	VkSampler sampler;
	VkDescriptorSet descriptor; // standalone single-texture descriptor (vk.set_layout), for "map $lightmap" extra-stage draws -- separate from worldBatch_t.descriptor's combined diffuse+lightmap set
} lightmapTex_t;

// Cached once at load (tessellation included); the per-frame source data for expansion below.
// Vertex color here is the BSP's raw (overbright-shifted) color -- rgbGen/tcMod are evaluated fresh
// every frame from this, never baked in.
static vkWorldVertex_t *s_cachedVerts;
static uint32_t s_numCachedVerts;
static uint32_t *s_cachedIndices;
static uint32_t s_numCachedIndices;

static VkBuffer s_staticVertexBuffer;
static VkDeviceMemory s_staticVertexMemory;
static VkBuffer s_staticIndexBuffer;
static VkDeviceMemory s_staticIndexMemory;

static vec3_t *s_deformCache;
static uint32_t *s_deformCacheStamp;
static uint32_t s_deformFrameStamp;

static worldBatch_t *s_batches;
static int s_numBatches;
static worldSurfRef_t *s_surfRefs;

static lightmapTex_t *s_lightmapTextures;
static int s_numLightmapTextures; // tr_world.numLightmaps real ones, plus one synthetic white fallback
static VkDescriptorPool s_lmDescriptorPool;
static VkDescriptorPool s_lmSingleDescriptorPool; // backs lightmapTex_t.descriptor, one set per lightmap texture

static qboolean s_built = qfalse;

static cvar_t *r_showtris;

// Per-frame dynamic buffers that visible-surface geometry is expanded into. Sized at load time from
// s_numCachedIndices, which is a hard upper bound on any single frame's visible vertex count (the
// *2 covers the normal stage-0 pass plus a same-frame r_showtris wireframe re-pass, which reuses the
// same expansion into the same buffer rather than keeping a separate wireframe-only path).
static VkBuffer s_dynLMBuffer;
static VkDeviceMemory s_dynLMMemory;
static vkWorldVertex_t *s_dynLMMapped;
static uint32_t s_dynLMCapacity; // size of one frame-in-flight's region, not the whole (now VK_FRAMES_IN_FLIGHT-sized) buffer
static uint32_t s_dynLMWriteOffset;
static uint32_t s_dynLMRegionBase; // this frame's region, set in vk_world_begin_frame
static qboolean s_dynLMOverflowWarned;

static VkBuffer s_dyn2DBuffer;
static VkDeviceMemory s_dyn2DMemory;
static vk2d_vertex_t *s_dyn2DMapped;
static uint32_t s_dyn2DCapacity;
static uint32_t s_dyn2DWriteOffset;
static uint32_t s_dyn2DRegionBase;
static qboolean s_dyn2DOverflowWarned;

// Dynamic lights are a separate additive pass over world geometry (previously they had zero effect
// on world surfaces at all); kept in its own buffer so it doesn't contend with extra-stage capacity.
static VkBuffer s_dynDlightBuffer;
static VkDeviceMemory s_dynDlightMemory;
static vk2d_vertex_t *s_dynDlightMapped;
static uint32_t s_dynDlightCapacity;
static uint32_t s_dynDlightWriteOffset;
static uint32_t s_dynDlightRegionBase;
static qboolean s_dynDlightOverflowWarned;

// Brush-entity (func_door/etc.) geometry: positions move every frame, so no load-time cache -- shaded straight from tr_world.drawVerts at draw time.
#define BRUSH_DYN_CAPACITY 16384
static VkBuffer s_dynBrushLMBuffer;
static VkDeviceMemory s_dynBrushLMMemory;
static vkWorldVertex_t *s_dynBrushLMMapped;
static uint32_t s_dynBrushLMWriteOffset;
static uint32_t s_dynBrushLMRegionBase;
static qboolean s_dynBrushLMOverflowWarned;

static VkBuffer s_dynBrushExtraBuffer;
static VkDeviceMemory s_dynBrushExtraMemory;
static vk2d_vertex_t *s_dynBrushExtraMapped;
static uint32_t s_dynBrushExtraWriteOffset;
static uint32_t s_dynBrushExtraRegionBase;
static qboolean s_dynBrushExtraOverflowWarned;

static int s_whiteLightmapIdx;

typedef struct {
	qhandle_t shader;
	int firstVert, numVerts;        // base mesh, into tr_world.drawVerts
	int firstIndex, numIndexes;     // local indices (0..numVerts-1), into tr_world.drawIndexes
	int firstInstance, numInstances; // into s_foliageInstances
} foliageSurf_t;

typedef struct {
	vec3_t origin;
	byte color[4]; // baked by q3map2, already overbright/gamma-shifted by R_LoadDrawVerts
} foliageInstance_t;

static foliageSurf_t *s_foliageSurfs;
static int s_numFoliageSurfs;
static foliageInstance_t *s_foliageInstances;

static VkBuffer s_dynFoliageBuffer;
static VkDeviceMemory s_dynFoliageMemory;
static vk2d_vertex_t *s_dynFoliageMapped;
static uint32_t s_dynFoliageCapacity;
static uint32_t s_dynFoliageWriteOffset;
static uint32_t s_dynFoliageRegionBase;
static qboolean s_dynFoliageOverflowWarned;

static cvar_t *r_drawfoliage;
static cvar_t *r_foliageCullDistance;

// Lazily-built (shader, lightmap) -> descriptor cache for brush-entity stage 0; a door's pair may not be among s_batches.
#define MAX_BRUSH_LM_DESCRIPTORS 64
typedef struct {
	qhandle_t shader;
	int lightmapIdx;
	VkDescriptorSet descriptor;
} brushLMDescCache_t;
static brushLMDescCache_t s_brushLMDescCache[MAX_BRUSH_LM_DESCRIPTORS];
static int s_numBrushLMDescCache;
static VkDescriptorPool s_brushLMDescriptorPool;

// Scratch for one surface's world-space vertices, rebuilt every call; static since it's too big for the stack.
#define BRUSH_SURF_MAX_VERTS 8192
typedef struct {
	float pos[3];
	byte color[4];
	float uv[2];
	float lmUv[2];
	vec3_t normal; // world-space, rotated by the entity's axis in TransformBrushVerts -- packed to snorm16 only at the final vkWorldVertex_t write
} brushVert_t;
static brushVert_t s_brushVertScratch[BRUSH_SURF_MAX_VERTS];

extern uint32_t vk_find_memory_type( uint32_t type_bits, VkMemoryPropertyFlags properties );

static qboolean IsTransparentBatch( const shader_t *shader );

static void create_dynamic_vertex_buffer( VkDeviceSize bytes, VkBuffer *outBuf, VkDeviceMemory *outMem, void **outMapped ) {
	VkBufferCreateInfo buf_desc;
	VkMemoryRequirements mem_reqs;
	VkMemoryAllocateInfo alloc_info;

	buf_desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buf_desc.pNext = NULL;
	buf_desc.flags = 0;
	buf_desc.size = bytes;
	buf_desc.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	buf_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	buf_desc.queueFamilyIndexCount = 0;
	buf_desc.pQueueFamilyIndices = NULL;
	VK_CHECK( qvkCreateBuffer( vk.device, &buf_desc, NULL, outBuf ) );

	qvkGetBufferMemoryRequirements( vk.device, *outBuf, &mem_reqs );

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = mem_reqs.size;
	alloc_info.memoryTypeIndex = vk_find_memory_type( mem_reqs.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, outMem ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, *outBuf, *outMem, 0 ) );

	VK_CHECK( qvkMapMemory( vk.device, *outMem, 0, VK_WHOLE_SIZE, 0, outMapped ) );
}

static void destroy_dynamic_vertex_buffer( VkBuffer buf, VkDeviceMemory mem, void *mapped ) {
	if ( mapped ) {
		qvkUnmapMemory( vk.device, mem );
	}
	if ( buf ) {
		qvkDestroyBuffer( vk.device, buf, NULL );
	}
	if ( mem ) {
		qvkFreeMemory( vk.device, mem, NULL );
	}
}

void vk_world_clear( void ) {
	int i;

	destroy_dynamic_vertex_buffer( s_dynLMBuffer, s_dynLMMemory, s_dynLMMapped );
	s_dynLMBuffer = VK_NULL_HANDLE; s_dynLMMemory = VK_NULL_HANDLE; s_dynLMMapped = NULL;
	s_dynLMCapacity = 0; s_dynLMWriteOffset = 0; s_dynLMOverflowWarned = qfalse;

	destroy_dynamic_vertex_buffer( s_dyn2DBuffer, s_dyn2DMemory, s_dyn2DMapped );
	s_dyn2DBuffer = VK_NULL_HANDLE; s_dyn2DMemory = VK_NULL_HANDLE; s_dyn2DMapped = NULL;
	s_dyn2DCapacity = 0; s_dyn2DWriteOffset = 0; s_dyn2DOverflowWarned = qfalse;

	destroy_dynamic_vertex_buffer( s_dynDlightBuffer, s_dynDlightMemory, s_dynDlightMapped );
	s_dynDlightBuffer = VK_NULL_HANDLE; s_dynDlightMemory = VK_NULL_HANDLE; s_dynDlightMapped = NULL;
	s_dynDlightCapacity = 0; s_dynDlightWriteOffset = 0; s_dynDlightOverflowWarned = qfalse;

	destroy_dynamic_vertex_buffer( s_dynBrushLMBuffer, s_dynBrushLMMemory, s_dynBrushLMMapped );
	s_dynBrushLMBuffer = VK_NULL_HANDLE; s_dynBrushLMMemory = VK_NULL_HANDLE; s_dynBrushLMMapped = NULL;
	s_dynBrushLMWriteOffset = 0; s_dynBrushLMOverflowWarned = qfalse;

	destroy_dynamic_vertex_buffer( s_dynFoliageBuffer, s_dynFoliageMemory, s_dynFoliageMapped );
	s_dynFoliageBuffer = VK_NULL_HANDLE; s_dynFoliageMemory = VK_NULL_HANDLE; s_dynFoliageMapped = NULL;
	s_dynFoliageCapacity = 0; s_dynFoliageWriteOffset = 0; s_dynFoliageOverflowWarned = qfalse;

	if ( s_foliageSurfs ) { free( s_foliageSurfs ); s_foliageSurfs = NULL; }
	if ( s_foliageInstances ) { free( s_foliageInstances ); s_foliageInstances = NULL; }
	s_numFoliageSurfs = 0;

	destroy_dynamic_vertex_buffer( s_dynBrushExtraBuffer, s_dynBrushExtraMemory, s_dynBrushExtraMapped );
	s_dynBrushExtraBuffer = VK_NULL_HANDLE; s_dynBrushExtraMemory = VK_NULL_HANDLE; s_dynBrushExtraMapped = NULL;
	s_dynBrushExtraWriteOffset = 0; s_dynBrushExtraOverflowWarned = qfalse;

	if ( s_brushLMDescriptorPool ) { qvkDestroyDescriptorPool( vk.device, s_brushLMDescriptorPool, NULL ); s_brushLMDescriptorPool = VK_NULL_HANDLE; }
	s_numBrushLMDescCache = 0;

	if ( s_lmDescriptorPool ) { qvkDestroyDescriptorPool( vk.device, s_lmDescriptorPool, NULL ); s_lmDescriptorPool = VK_NULL_HANDLE; }
	if ( s_lmSingleDescriptorPool ) { qvkDestroyDescriptorPool( vk.device, s_lmSingleDescriptorPool, NULL ); s_lmSingleDescriptorPool = VK_NULL_HANDLE; }

	for ( i = 0; i < s_numLightmapTextures; i++ ) {
		vk_destroy_texture_image( s_lightmapTextures[i].image, s_lightmapTextures[i].memory,
			s_lightmapTextures[i].view, s_lightmapTextures[i].sampler );
	}
	if ( s_lightmapTextures ) { free( s_lightmapTextures ); s_lightmapTextures = NULL; }
	s_numLightmapTextures = 0;

	if ( s_batches ) { free( s_batches ); s_batches = NULL; }
	if ( s_surfRefs ) { free( s_surfRefs ); s_surfRefs = NULL; }
	s_numBatches = 0;

	if ( s_cachedVerts ) { free( s_cachedVerts ); s_cachedVerts = NULL; }
	if ( s_cachedIndices ) { free( s_cachedIndices ); s_cachedIndices = NULL; }
	s_numCachedVerts = 0;
	s_numCachedIndices = 0;

	if ( s_staticVertexBuffer ) { qvkDestroyBuffer( vk.device, s_staticVertexBuffer, NULL ); s_staticVertexBuffer = VK_NULL_HANDLE; }
	if ( s_staticVertexMemory ) { qvkFreeMemory( vk.device, s_staticVertexMemory, NULL ); s_staticVertexMemory = VK_NULL_HANDLE; }
	if ( s_staticIndexBuffer ) { qvkDestroyBuffer( vk.device, s_staticIndexBuffer, NULL ); s_staticIndexBuffer = VK_NULL_HANDLE; }
	if ( s_staticIndexMemory ) { qvkFreeMemory( vk.device, s_staticIndexMemory, NULL ); s_staticIndexMemory = VK_NULL_HANDLE; }

	if ( s_deformCache ) { free( s_deformCache ); s_deformCache = NULL; }
	if ( s_deformCacheStamp ) { free( s_deformCacheStamp ); s_deformCacheStamp = NULL; }
	s_deformFrameStamp = 0;

	s_built = qfalse;
}

void vk_world_shutdown( void ) {
	vk_world_clear();
}

// float[3] in [-1,1] -> snorm int16[4], w left 0; see vkWorldVertex_t's packing comment.
static void pack_snorm16_3( const vec3_t in, int16_t out[4] ) {
	int i;
	for ( i = 0; i < 3; i++ ) {
		float v = in[i];
		if ( v < -1.0f ) v = -1.0f;
		if ( v > 1.0f ) v = 1.0f;
		out[i] = (int16_t)( v * 32767.0f );
	}
	out[3] = 0;
}

// Inverse of pack_snorm16_3 -- needed to get a real float normal back out of s_cachedVerts for CPU-side deformVertexes wave/bulge (see R_DeformPosition).
static void unpack_snorm16_3( const int16_t in[4], vec3_t out ) {
	out[0] = in[0] * ( 1.0f / 32767.0f );
	out[1] = in[1] * ( 1.0f / 32767.0f );
	out[2] = in[2] * ( 1.0f / 32767.0f );
}


static const float *GetDeformedWorldVertex( const shader_t *shader, uint32_t vertIdx ) {
	if ( s_deformCacheStamp[vertIdx] != s_deformFrameStamp ) {
		const vkWorldVertex_t *src = &s_cachedVerts[vertIdx];
		vec3_t normal;

		unpack_snorm16_3( src->normal, normal );
		R_DeformPosition( shader, src->pos, normal, src->uv[0], s_deformCache[vertIdx] );
		s_deformCacheStamp[vertIdx] = s_deformFrameStamp;
	}
	return s_deformCache[vertIdx];
}

// Raw BSP vertex color (already overbright-shifted by R_LoadDrawVerts) -- not stage-baked.
static void CopyVertex( vkWorldVertex_t *out, const drawVert_t *in ) {
	out->pos[0] = in->xyz[0];
	out->pos[1] = in->xyz[1];
	out->pos[2] = in->xyz[2];
	out->color[0] = in->color[0];
	out->color[1] = in->color[1];
	out->color[2] = in->color[2];
	out->color[3] = in->color[3];
	out->uv[0] = in->st[0];
	out->uv[1] = in->st[1];
	out->lmUv[0] = in->lightmap[0];
	out->lmUv[1] = in->lightmap[1];
	pack_snorm16_3( in->normal, out->normal );
	out->tangent[0] = out->tangent[1] = out->tangent[2] = out->tangent[3] = 0;
	out->lightdir[0] = out->lightdir[1] = out->lightdir[2] = out->lightdir[3] = 0;
}

// Appends one surface's triangles into verts/indexes, tessellating patches on the fly; returns its index range.
static void EmitSurfaceGeometry( const bspSurface_t *surf, vkWorldVertex_t *verts, uint32_t *nextVertSlot,
								  uint32_t *indexes, uint32_t *writeOffset,
								  uint32_t *outFirstIndex, uint32_t *outIndexCount ) {
	uint32_t firstIndex = *writeOffset;
	int i;

	if ( surf->surfaceType == MST_PLANAR || surf->surfaceType == MST_TRIANGLE_SOUP ) {
		for ( i = 0; i < surf->numIndexes; i++ ) {
			// BSP indices are local to the surface's own vertex range; remap to the global vertex buffer.
			indexes[(*writeOffset)++] = (uint32_t)( surf->firstVert + tr_world.drawIndexes[surf->firstIndex + i] );
		}
	} else if ( surf->surfaceType == MST_PATCH ) {
		int pv = R_PatchTessVertCount( &tr_world.drawVerts[surf->firstVert], surf->patchWidth, surf->patchHeight );
		int pi = R_PatchTessIndexCount( &tr_world.drawVerts[surf->firstVert], surf->patchWidth, surf->patchHeight );
		uint32_t vertBase = *nextVertSlot;
		drawVert_t *tessVerts = malloc( pv * sizeof( drawVert_t ) );
		int *localIdx = malloc( pi * sizeof( int ) );

		R_TessellatePatch( &tr_world.drawVerts[surf->firstVert], surf->patchWidth, surf->patchHeight, tessVerts, localIdx );

		for ( i = 0; i < pv; i++ ) {
			CopyVertex( &verts[vertBase + (uint32_t)i], &tessVerts[i] );
		}
		*nextVertSlot += (uint32_t)pv;

		for ( i = 0; i < pi; i++ ) {
			indexes[(*writeOffset)++] = vertBase + (uint32_t)localIdx[i];
		}

		free( tessVerts );
		free( localIdx );
	}

	*outFirstIndex = firstIndex;
	*outIndexCount = *writeOffset - firstIndex;
}

static int CompareSurfByShaderThenLightmap( const void *a, const void *b ) {
	int ia = *(const int *)a, ib = *(const int *)b;
	const bspSurface_t *sa = &tr_world.surfaces[ia], *sb = &tr_world.surfaces[ib];
	if ( sa->shaderNum != sb->shaderNum ) return sa->shaderNum - sb->shaderNum;
	return sa->lightmapNum - sb->lightmapNum;
}

// RGB8 -> RGBA8 (opaque) so it can go through the same texture upload path as everything else.
static void BuildLightmapTexture( const byte *rgb, lightmapTex_t *out ) {
	byte *rgba = malloc( LIGHTMAP_WIDTH * LIGHTMAP_HEIGHT * 4 );
	int i;

	for ( i = 0; i < LIGHTMAP_WIDTH * LIGHTMAP_HEIGHT; i++ ) {
		R_ShiftLightingBytes( &rgb[i * 3], &rgba[i * 4] );
		rgba[i * 4 + 3] = 255;
	}

	vk_create_texture_image( rgba, LIGHTMAP_WIDTH, LIGHTMAP_HEIGHT, IMGFLAG_CLAMPTOEDGE,
		&out->image, &out->memory, &out->view, &out->sampler );

	free( rgba );
}

static void BuildWhiteLightmapTexture( lightmapTex_t *out ) {
	byte white[4] = { 255, 255, 255, 255 };
	vk_create_texture_image( white, 1, 1, IMGFLAG_CLAMPTOEDGE, &out->image, &out->memory, &out->view, &out->sampler );
}

static qboolean ShaderHasExplicitLightmapStage( const shader_t *shader ) {
	int s;
	if ( !shader ) {
		return qfalse;
	}
	for ( s = 0; s < shader->numStages; s++ ) {
		if ( shader->stages[s].isLightmap ) {
			return qtrue;
		}
	}
	return qfalse;
}

static void BuildFoliageCache( void ) {
	int firstWorldSurf = ( tr_world.numSubmodels > 0 ) ? tr_world.submodels[0].firstSurface : 0;
	int numWorldSurfs = ( tr_world.numSubmodels > 0 ) ? tr_world.submodels[0].numSurfaces : tr_world.numSurfaces;
	int surfIdx, numFoliageSurfs = 0, totalInstances = 0;
	uint64_t totalExpandedVerts = 0;
	foliageSurf_t *surfs;
	foliageInstance_t *instances;
	float heightScale;
	int outSurf, outInst;

	if ( !r_drawfoliage ) {
		r_drawfoliage = ri.Cvar_Get( "r_drawfoliage", "1", CVAR_ARCHIVE );
	}
	if ( !r_foliageCullDistance ) {
		r_foliageCullDistance = ri.Cvar_Get( "r_foliageCullDistance", "30", CVAR_ARCHIVE );
		ri.Cvar_CheckRange( r_foliageCullDistance, 1, 400, qfalse );
	}

	for ( surfIdx = firstWorldSurf; surfIdx < firstWorldSurf + numWorldSurfs; surfIdx++ ) {
		const bspSurface_t *surf = &tr_world.surfaces[surfIdx];
		shader_t *shader;

		if ( surf->surfaceType != MST_FOLIAGE ) continue;
		if ( tr_world.shaders[surf->shaderNum].surfaceFlags & SURF_NODRAW ) continue;
		shader = R_GetShaderByHandle( tr_world.shaders[surf->shaderNum].shader );
		if ( !shader || shader->isSky || shader->numStages < 1 ) continue;
		// Foliage surfaces repurpose patchHeight/patchWidth for the real base-mesh vert count and
		// instance count (the generic numVerts field spans both, like the reference's ParseFoliage).
		if ( surf->patchHeight <= 0 || surf->patchWidth <= 0 || surf->numIndexes <= 0 ) continue;

		numFoliageSurfs++;
		totalInstances += surf->patchWidth;
	}

	if ( numFoliageSurfs == 0 ) {
		return;
	}

	surfs = malloc( (size_t)numFoliageSurfs * sizeof( foliageSurf_t ) );
	instances = malloc( (size_t)totalInstances * sizeof( foliageInstance_t ) );

	// Changing r_drawfoliage takes effect on the next map load, not live -- matches the reference engine.
	heightScale = r_drawfoliage->value;
	if ( heightScale < 0.0f ) heightScale = 1.0f;
	else if ( heightScale > 2.0f ) heightScale = 2.0f;

	outSurf = 0; outInst = 0;
	for ( surfIdx = firstWorldSurf; surfIdx < firstWorldSurf + numWorldSurfs; surfIdx++ ) {
		const bspSurface_t *surf = &tr_world.surfaces[surfIdx];
		shader_t *shader;
		int numVerts, numInstances, i2;
		drawVert_t *instanceSrc;

		if ( surf->surfaceType != MST_FOLIAGE ) continue;
		if ( tr_world.shaders[surf->shaderNum].surfaceFlags & SURF_NODRAW ) continue;
		shader = R_GetShaderByHandle( tr_world.shaders[surf->shaderNum].shader );
		if ( !shader || shader->isSky || shader->numStages < 1 ) continue;
		numVerts = surf->patchHeight;
		numInstances = surf->patchWidth;
		if ( numVerts <= 0 || numInstances <= 0 || surf->numIndexes <= 0 ) continue;

		surfs[outSurf].shader = tr_world.shaders[surf->shaderNum].shader;
		surfs[outSurf].firstVert = surf->firstVert;
		surfs[outSurf].numVerts = numVerts;
		surfs[outSurf].firstIndex = surf->firstIndex;
		surfs[outSurf].numIndexes = surf->numIndexes;
		surfs[outSurf].firstInstance = outInst;
		surfs[outSurf].numInstances = numInstances;

		// Bake the height scale into the base mesh once, like the reference engine's ParseFoliage --
		// safe to mutate in place, this vertex range belongs solely to this surface.
		if ( heightScale != 1.0f ) {
			int v;
			for ( v = 0; v < numVerts; v++ ) {
				tr_world.drawVerts[surf->firstVert + v].xyz[2] *= heightScale;
			}
		}

		// The instances are additional drawverts right after the base mesh: xyz is the world-space
		// origin, color is the baked lighting (already overbright/gamma-shifted -- see R_LoadDrawVerts).
		instanceSrc = &tr_world.drawVerts[surf->firstVert + numVerts];
		for ( i2 = 0; i2 < numInstances; i2++ ) {
			VectorCopy( instanceSrc[i2].xyz, instances[outInst + i2].origin );
			instances[outInst + i2].color[0] = instanceSrc[i2].color[0];
			instances[outInst + i2].color[1] = instanceSrc[i2].color[1];
			instances[outInst + i2].color[2] = instanceSrc[i2].color[2];
			instances[outInst + i2].color[3] = instanceSrc[i2].color[3];
		}

		totalExpandedVerts += (uint64_t)numInstances * (uint64_t)surf->numIndexes * (uint64_t)shader->numStages;

		outInst += numInstances;
		outSurf++;
	}

	if ( outSurf == 0 ) {
		free( surfs );
		free( instances );
		return;
	}

	s_foliageSurfs = surfs;
	s_numFoliageSurfs = outSurf;
	s_foliageInstances = instances;

	// Worst case for one frame: every instance visible/unculled, through every stage of its shader.
	// No extra headroom for a same-frame portal/mirror recursion's second pass (like s_dynDlightCapacity,
	// not like s_dynLM/s_dyn2D's *2) -- foliage is rare alongside portals; overflow just warns and drops.
	s_dynFoliageCapacity = ( totalExpandedVerts > 0xFFFFFFFFu ) ? 0xFFFFFFFFu : (uint32_t)totalExpandedVerts;
	if ( s_dynFoliageCapacity == 0 ) {
		s_dynFoliageCapacity = 1; // keep create_dynamic_vertex_buffer from being asked for a zero-size buffer
	}
	create_dynamic_vertex_buffer( (VkDeviceSize)s_dynFoliageCapacity * VK_FRAMES_IN_FLIGHT * sizeof( vk2d_vertex_t ),
		&s_dynFoliageBuffer, &s_dynFoliageMemory, (void **)&s_dynFoliageMapped );

	ri.Printf( PRINT_ALL, "...vk_world_build: %d foliage surfaces, %d instances\n", outSurf, outInst );
}

static void PrewarmStagePipelines( const shader_t *shader, int firstStage ) {
	VkCullModeFlags cull = vk_cullModeForShader( shader->cullType );
	int s;

	for ( s = firstStage; s < shader->numStages; s++ ) {
		const shaderStage_t *stage = &shader->stages[s];

		vk_getPipeline3D( sizeof( vk2d_vertex_t ), stage->blendEnabled, stage->srcBlend, stage->dstBlend,
			cull, stage->alphaTestFunc, VK_FALSE, VK_FALSE, shader->polygonOffset, stage->depthWrite );
		vk_getPipeline3D( sizeof( vk2d_vertex_t ), stage->blendEnabled, stage->srcBlend, stage->dstBlend,
			cull, stage->alphaTestFunc, VK_TRUE, VK_FALSE, shader->polygonOffset, stage->depthWrite );
		vk_getPipeline3D( sizeof( vk2d_vertex_t ), stage->blendEnabled, stage->srcBlend, stage->dstBlend,
			cull, stage->alphaTestFunc, VK_TRUE, VK_TRUE, shader->polygonOffset, stage->depthWrite );
	}
}

static void PrewarmWorldBatchPipelines( void ) {
	int i;

	for ( i = 0; i < s_numBatches; i++ ) {
		shader_t *shader = R_GetShaderByHandle( s_batches[i].shader );
		VkCullModeFlags cull;

		if ( !shader || shader->numStages < 1 ) {
			continue;
		}

		if ( IsTransparentBatch( shader ) ) {
			PrewarmStagePipelines( shader, 0 );
			continue;
		}

		PrewarmStagePipelines( shader, shader->stages[0].isLightmap ? 0 : 1 );

		cull = vk_cullModeForShader( shader->cullType );
		vk_getPipeline3D( sizeof( vk2d_vertex_t ), VK_TRUE, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE,
			cull, shader->stages[0].alphaTestFunc, VK_FALSE, VK_FALSE, VK_FALSE, VK_FALSE );
		vk_getPipeline3D( sizeof( vk2d_vertex_t ), VK_TRUE, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE,
			cull, shader->stages[0].alphaTestFunc, VK_TRUE, VK_FALSE, VK_FALSE, VK_FALSE );
		vk_getPipeline3D( sizeof( vk2d_vertex_t ), VK_TRUE, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE,
			cull, shader->stages[0].alphaTestFunc, VK_TRUE, VK_TRUE, VK_FALSE, VK_FALSE );
	}
}

static void PrewarmFoliagePipelines( void ) {
	int i;

	for ( i = 0; i < s_numFoliageSurfs; i++ ) {
		shader_t *shader = R_GetShaderByHandle( s_foliageSurfs[i].shader );

		if ( !shader || shader->numStages < 1 ) {
			continue;
		}
		PrewarmStagePipelines( shader, 0 );
	}
}

static void PrewarmBrushEntityPipelines( void ) {
	int m;

	for ( m = 1; m < tr_world.numSubmodels; m++ ) {
		const dmodel_t *submodel = &tr_world.submodels[m];
		int surfIdx;

		for ( surfIdx = submodel->firstSurface; surfIdx < submodel->firstSurface + submodel->numSurfaces; surfIdx++ ) {
			const bspSurface_t *surf = &tr_world.surfaces[surfIdx];
			shader_t *shader;

			if ( tr_world.shaders[surf->shaderNum].surfaceFlags & SURF_NODRAW ) {
				continue;
			}
			shader = R_GetShaderByHandle( tr_world.shaders[surf->shaderNum].shader );
			if ( !shader || shader->isSky || shader->numStages < 1 ) {
				continue;
			}

			PrewarmStagePipelines( shader, ( shader->stages[0].blendEnabled && shader->explicitlyDefined ) ? 0 : 1 );
		}
	}
}

static void PrewarmPipelines( void ) {
	PrewarmWorldBatchPipelines();
	PrewarmFoliagePipelines();
	PrewarmBrushEntityPipelines();
}

void vk_world_build( void ) {
	vkWorldVertex_t *verts;
	uint32_t *indexes;
	worldBatch_t *batches;
	worldSurfRef_t *surfRefs;
	int *sortedSurfs;
	uint32_t totalIndexCount = 0, totalPatchVertCount = 0, writeOffset = 0, nextVertSlot;
	int numDrawable = 0, numBatches = 0, numSurfRefs = 0;
	int whiteLightmapIdx;
	int surfIdx, i;
	// Submodel 0 is the static world; submodels 1+ are brush entities, drawn separately (vk_world_draw_brushmodel).
	int firstWorldSurf = ( tr_world.numSubmodels > 0 ) ? tr_world.submodels[0].firstSurface : 0;
	int numWorldSurfs = ( tr_world.numSubmodels > 0 ) ? tr_world.submodels[0].numSurfaces : tr_world.numSurfaces;

	vk_world_clear();

	if ( !tr_world.loaded || tr_world.numDrawVerts == 0 || tr_world.numShaders == 0 ) {
		return;
	}

	// Independent of the normal batch/surfRef system below -- a map with foliage but no other
	// drawable surfaces would otherwise never get here (see the numDrawable==0 early return further down).
	BuildFoliageCache();

	sortedSurfs = malloc( tr_world.numSurfaces * sizeof( int ) );

	tr_world.hasSky = qfalse;
	tr_world.skyShader = NULL;

	for ( surfIdx = firstWorldSurf; surfIdx < firstWorldSurf + numWorldSurfs; surfIdx++ ) {
		const bspSurface_t *surf = &tr_world.surfaces[surfIdx];
		shader_t *surfShader = R_GetShaderByHandle( tr_world.shaders[surf->shaderNum].shader );

		if ( tr_world.shaders[surf->shaderNum].surfaceFlags & SURF_NODRAW ) {
			continue;
		}

		if ( surfShader->isSky ) {
			// Sky-shaded surfaces aren't batched/drawn here -- vk_sky.c paints the whole box every frame instead.
			if ( !tr_world.hasSky ) {
				memcpy( tr_world.skyOuterBox, surfShader->skyOuterBox, sizeof( tr_world.skyOuterBox ) );
				tr_world.skyShader = surfShader;
				tr_world.hasSky = qtrue;
			}
			continue;
		}

		if ( surf->surfaceType == MST_PLANAR || surf->surfaceType == MST_TRIANGLE_SOUP ) {
			totalIndexCount += (uint32_t)surf->numIndexes;
		} else if ( surf->surfaceType == MST_PATCH ) {
			int pv = R_PatchTessVertCount( &tr_world.drawVerts[surf->firstVert], surf->patchWidth, surf->patchHeight );
			int pi = R_PatchTessIndexCount( &tr_world.drawVerts[surf->firstVert], surf->patchWidth, surf->patchHeight );
			if ( pv <= 0 || pi <= 0 ) {
				continue; // malformed control grid
			}
			totalPatchVertCount += (uint32_t)pv;
			totalIndexCount += (uint32_t)pi;
		} else {
			continue; // flare/foliage: not drawable yet
		}

		sortedSurfs[numDrawable++] = surfIdx;
	}

	if ( numDrawable == 0 || totalIndexCount == 0 ) {
		free( sortedSurfs );
		return;
	}

	qsort( sortedSurfs, numDrawable, sizeof( int ), CompareSurfByShaderThenLightmap );

	verts = malloc( ( (uint32_t)tr_world.numDrawVerts + totalPatchVertCount ) * sizeof( vkWorldVertex_t ) );
	indexes = malloc( totalIndexCount * sizeof( uint32_t ) );
	surfRefs = malloc( numDrawable * sizeof( worldSurfRef_t ) );
	batches = malloc( numDrawable * sizeof( worldBatch_t ) ); // upper bound: one batch per surface

	for ( i = 0; i < tr_world.numDrawVerts; i++ ) {
		CopyVertex( &verts[i], &tr_world.drawVerts[i] );
	}
	nextVertSlot = (uint32_t)tr_world.numDrawVerts;

	// Lightmap textures: the real ones from the BSP, plus a synthetic white fallback for vertex-lit surfaces.
	whiteLightmapIdx = tr_world.numLightmaps;
	s_lightmapTextures = malloc( ( tr_world.numLightmaps + 1 ) * sizeof( lightmapTex_t ) );
	for ( i = 0; i < tr_world.numLightmaps; i++ ) {
		BuildLightmapTexture( tr_world.lightmapData + (size_t)i * LIGHTMAP_WIDTH * LIGHTMAP_HEIGHT * 3, &s_lightmapTextures[i] );
	}
	BuildWhiteLightmapTexture( &s_lightmapTextures[whiteLightmapIdx] );
	s_numLightmapTextures = tr_world.numLightmaps + 1;
	s_whiteLightmapIdx = whiteLightmapIdx; // brush-entity surfaces (vk_world_draw_brushmodel) need this too

	{
		VkDescriptorPoolSize poolSize;
		VkDescriptorPoolCreateInfo poolDesc;

		poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSize.descriptorCount = (uint32_t)s_numLightmapTextures;

		poolDesc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolDesc.pNext = NULL;
		poolDesc.flags = 0;
		poolDesc.maxSets = (uint32_t)s_numLightmapTextures;
		poolDesc.poolSizeCount = 1;
		poolDesc.pPoolSizes = &poolSize;
		VK_CHECK( qvkCreateDescriptorPool( vk.device, &poolDesc, NULL, &s_lmSingleDescriptorPool ) );

		for ( i = 0; i < s_numLightmapTextures; i++ ) {
			VkDescriptorSetAllocateInfo allocInfo;
			VkDescriptorImageInfo imageInfo;
			VkWriteDescriptorSet write;

			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.pNext = NULL;
			allocInfo.descriptorPool = s_lmSingleDescriptorPool;
			allocInfo.descriptorSetCount = 1;
			allocInfo.pSetLayouts = &vk.set_layout;
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &allocInfo, &s_lightmapTextures[i].descriptor ) );

			imageInfo.sampler = s_lightmapTextures[i].sampler;
			imageInfo.imageView = s_lightmapTextures[i].view;
			imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			write.pNext = NULL;
			write.dstSet = s_lightmapTextures[i].descriptor;
			write.dstBinding = 0;
			write.dstArrayElement = 0;
			write.descriptorCount = 1;
			write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			write.pImageInfo = &imageInfo;
			write.pBufferInfo = NULL;
			write.pTexelBufferView = NULL;
			qvkUpdateDescriptorSets( vk.device, 1, &write, 0, NULL );
		}
	}

	{
		int curShaderNum = -1, curLightmapIdx = -1;

		for ( i = 0; i < numDrawable; i++ ) {
			int sIdx = sortedSurfs[i];
			const bspSurface_t *surf = &tr_world.surfaces[sIdx];
			int lightmapIdx = ( surf->lightmapNum >= 0 && surf->lightmapNum < tr_world.numLightmaps ) ?
				surf->lightmapNum : whiteLightmapIdx;
			uint32_t firstIndex, indexCount;

			if ( surf->shaderNum != curShaderNum || lightmapIdx != curLightmapIdx ) {
				batches[numBatches].shader = tr_world.shaders[surf->shaderNum].shader;
				batches[numBatches].lightmapIdx = lightmapIdx;
				batches[numBatches].firstSurfRef = numSurfRefs;
				batches[numBatches].numSurfRefs = 0;
				batches[numBatches].descriptor = VK_NULL_HANDLE;
				numBatches++;
				curShaderNum = surf->shaderNum;
				curLightmapIdx = lightmapIdx;
			}

			EmitSurfaceGeometry( surf, verts, &nextVertSlot, indexes, &writeOffset, &firstIndex, &indexCount );

			surfRefs[numSurfRefs].surfaceIndex = sIdx;
			surfRefs[numSurfRefs].firstIndex = firstIndex;
			surfRefs[numSurfRefs].indexCount = indexCount;
			{
				vec3_t sMins, sMaxs;
				uint32_t k;
				ClearBounds( sMins, sMaxs );
				for ( k = 0; k < indexCount; k++ ) {
					AddPointToBounds( verts[indexes[firstIndex + k]].pos, sMins, sMaxs );
				}
				VectorCopy( sMins, surfRefs[numSurfRefs].mins );
				VectorCopy( sMaxs, surfRefs[numSurfRefs].maxs );
			}
			numSurfRefs++;
			batches[numBatches - 1].numSurfRefs++;
		}
	}

	free( sortedSurfs );

	// One combo descriptor set (diffuse + lightmap) per batch.
	{
		VkDescriptorPoolSize poolSize;
		VkDescriptorPoolCreateInfo poolDesc;

		poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSize.descriptorCount = (uint32_t)numBatches * 2;

		poolDesc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolDesc.pNext = NULL;
		poolDesc.flags = 0;
		poolDesc.maxSets = (uint32_t)numBatches;
		poolDesc.poolSizeCount = 1;
		poolDesc.pPoolSizes = &poolSize;
		VK_CHECK( qvkCreateDescriptorPool( vk.device, &poolDesc, NULL, &s_lmDescriptorPool ) );

		for ( i = 0; i < numBatches; i++ ) {
			shader_t *shader = R_GetShaderByHandle( batches[i].shader );
			image_t *diffuse = ( shader && shader->numStages >= 1 && !shader->stages[0].isLightmap && shader->stages[0].image ) ?
				shader->stages[0].image : R_GetWhiteImage();

			lightmapTex_t *lm = ShaderHasExplicitLightmapStage( shader ) ?
				&s_lightmapTextures[s_whiteLightmapIdx] : &s_lightmapTextures[batches[i].lightmapIdx];
			VkDescriptorSetAllocateInfo allocInfo;
			VkDescriptorImageInfo imageInfos[2];
			VkWriteDescriptorSet writes[2];

			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.pNext = NULL;
			allocInfo.descriptorPool = s_lmDescriptorPool;
			allocInfo.descriptorSetCount = 1;
			allocInfo.pSetLayouts = &vk.lightmap_set_layout;
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &allocInfo, &batches[i].descriptor ) );

			imageInfos[0].sampler = diffuse->vk_sampler;
			imageInfos[0].imageView = diffuse->vk_view;
			imageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfos[1].sampler = lm->sampler;
			imageInfos[1].imageView = lm->view;
			imageInfos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[0].pNext = NULL;
			writes[0].dstSet = batches[i].descriptor;
			writes[0].dstBinding = 0;
			writes[0].dstArrayElement = 0;
			writes[0].descriptorCount = 1;
			writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writes[0].pImageInfo = &imageInfos[0];
			writes[0].pBufferInfo = NULL;
			writes[0].pTexelBufferView = NULL;
			writes[1] = writes[0];
			writes[1].dstBinding = 1;
			writes[1].pImageInfo = &imageInfos[1];
			qvkUpdateDescriptorSets( vk.device, 2, writes, 0, NULL );
		}
	}

	// Cached CPU-side source data for per-frame expansion -- not uploaded as a static GPU buffer.
	s_cachedVerts = verts;
	s_numCachedVerts = nextVertSlot;
	s_cachedIndices = indexes;
	s_numCachedIndices = writeOffset;

	s_deformCache = malloc( s_numCachedVerts * sizeof( vec3_t ) );
	s_deformCacheStamp = calloc( s_numCachedVerts, sizeof( uint32_t ) );
	s_deformFrameStamp = 0; // stamp 0 means "never computed"; vk_world_begin_frame starts incrementing from 1

	// *2: one full pass plus a same-frame r_showtris re-expansion's worth of headroom; each capacity below is ONE frame-in-flight region, VK_FRAMES_IN_FLIGHT of them back to back in the actual buffer (see vk_world_begin_frame).
	s_dynLMCapacity = writeOffset * 2;
	create_dynamic_vertex_buffer( (VkDeviceSize)s_dynLMCapacity * VK_FRAMES_IN_FLIGHT * sizeof( vkWorldVertex_t ),
		&s_dynLMBuffer, &s_dynLMMemory, (void **)&s_dynLMMapped );

	// Headroom for a couple of extra-stage passes across the whole visible world within one frame.
	s_dyn2DCapacity = writeOffset * 2;
	create_dynamic_vertex_buffer( (VkDeviceSize)s_dyn2DCapacity * VK_FRAMES_IN_FLIGHT * sizeof( vk2d_vertex_t ),
		&s_dyn2DBuffer, &s_dyn2DMemory, (void **)&s_dyn2DMapped );

	// One full visible-world's worth; dlights are rare/bounded so no extra headroom needed here.
	s_dynDlightCapacity = writeOffset;
	create_dynamic_vertex_buffer( (VkDeviceSize)s_dynDlightCapacity * VK_FRAMES_IN_FLIGHT * sizeof( vk2d_vertex_t ),
		&s_dynDlightBuffer, &s_dynDlightMemory, (void **)&s_dynDlightMapped );

	// Brush-entity (func_door/etc.) buffers: fixed capacity, independent of map size -- see their declaration.
	create_dynamic_vertex_buffer( (VkDeviceSize)BRUSH_DYN_CAPACITY * VK_FRAMES_IN_FLIGHT * sizeof( vkWorldVertex_t ),
		&s_dynBrushLMBuffer, &s_dynBrushLMMemory, (void **)&s_dynBrushLMMapped );
	create_dynamic_vertex_buffer( (VkDeviceSize)BRUSH_DYN_CAPACITY * VK_FRAMES_IN_FLIGHT * sizeof( vk2d_vertex_t ),
		&s_dynBrushExtraBuffer, &s_dynBrushExtraMemory, (void **)&s_dynBrushExtraMapped );
	{
		VkDescriptorPoolSize poolSize;
		VkDescriptorPoolCreateInfo poolDesc;

		poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSize.descriptorCount = MAX_BRUSH_LM_DESCRIPTORS * 2;

		poolDesc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolDesc.pNext = NULL;
		poolDesc.flags = 0;
		poolDesc.maxSets = MAX_BRUSH_LM_DESCRIPTORS;
		poolDesc.poolSizeCount = 1;
		poolDesc.pPoolSizes = &poolSize;
		VK_CHECK( qvkCreateDescriptorPool( vk.device, &poolDesc, NULL, &s_brushLMDescriptorPool ) );
	}
	s_numBrushLMDescCache = 0;

	for ( i = 0; i < numBatches; i++ ) {
		shader_t *batchShader = R_GetShaderByHandle( batches[i].shader );
		batches[i].staticEligible = vk.staticWorldSupported && batchShader && batchShader->numStages >= 1 &&
			!batchShader->stages[0].isLightmap && !IsTransparentBatch( batchShader ) &&
			batchShader->numDeforms == 0 && batchShader->stages[0].numTexMods == 0;
	}

	vk_create_device_local_buffer( verts, (VkDeviceSize)nextVertSlot * sizeof( vkWorldVertex_t ),
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &s_staticVertexBuffer, &s_staticVertexMemory );
	vk_create_device_local_buffer( indexes, (VkDeviceSize)writeOffset * sizeof( uint32_t ),
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT, &s_staticIndexBuffer, &s_staticIndexMemory );

	s_batches = batches; // already sized to numDrawable, no need to shrink
	s_numBatches = numBatches;
	s_surfRefs = surfRefs;
	s_built = qtrue;

	PrewarmPipelines();

	ri.Printf( PRINT_ALL, "...vk_world_build: %d verts, %d indexes, %d batches, %d lightmaps\n",
			   (int)nextVertSlot, (int)writeOffset, numBatches, tr_world.numLightmaps );
}

static void ExpandAndDrawStage0( const worldSurfRef_t *refs, int numRefs, const shader_t *shader, qboolean useBakedVertexColor ) {
	shaderStage_t stage0Local;
	const shaderStage_t *stage0 = &shader->stages[0];
	qboolean runOpen = qfalse;
	uint32_t runStart = 0;
	int j;

	if ( useBakedVertexColor && !shader->explicitlyDefined && stage0->rgbGen == CGEN_LIGHTING_DIFFUSE ) {
		stage0Local = *stage0;
		stage0Local.rgbGen = CGEN_EXACT_VERTEX;
		stage0 = &stage0Local;
	}


	qboolean stage0Invariant = R_StageColorAlphaIsVertexInvariant( stage0 );
	byte stage0Color[4];
	if ( stage0Invariant ) {
		static const byte dummyBaseColor[4] = { 255, 255, 255, 255 }; // never read when stage0Invariant -- see R_StageColorAlphaIsVertexInvariant's contract
		R_CalcStageColorAlpha( stage0, NULL, dummyBaseColor, stage0Color );
	}

	for ( j = 0; j < numRefs; j++ ) {
		const worldSurfRef_t *ref = &refs[j];
		uint32_t k;

		if ( !R_SurfaceIsVisible( ref->surfaceIndex ) ) {
			if ( runOpen ) {
				qvkCmdDraw( vk.command_buffer, s_dynLMWriteOffset - runStart, 1, runStart, 0 );
				runOpen = qfalse;
			}
			continue;
		}

		if ( s_dynLMWriteOffset + ref->indexCount > s_dynLMCapacity ) {
			if ( runOpen ) {
				qvkCmdDraw( vk.command_buffer, s_dynLMWriteOffset - runStart, 1, runStart, 0 );
				runOpen = qfalse;
			}
			if ( !s_dynLMOverflowWarned ) {
				ri.Printf( PRINT_WARNING, "vk_world: dynamic lightmap vertex buffer full, dropping remaining world geometry this frame\n" );
				s_dynLMOverflowWarned = qtrue;
			}
			continue;
		}

		if ( !runOpen ) {
			runStart = s_dynLMWriteOffset;
			runOpen = qtrue;
		}

		for ( k = 0; k < ref->indexCount; k++ ) {
			uint32_t vertIdx = s_cachedIndices[ref->firstIndex + k];
			const vkWorldVertex_t *src = &s_cachedVerts[vertIdx];
			vkWorldVertex_t *dst = &s_dynLMMapped[s_dynLMRegionBase + s_dynLMWriteOffset++];
			const float *deformed;
			const byte *color;
			byte computedColor[4];
			float uv[2];

			deformed = GetDeformedWorldVertex( shader, vertIdx );
			if ( stage0Invariant ) {
				color = stage0Color;
			} else {
				R_CalcStageColorAlpha( stage0, NULL, src->color, computedColor );
				color = computedColor;
			}
			R_CalcStageTexCoord( stage0, src->uv[0], src->uv[1], deformed, NULL, uv );

			dst->pos[0] = deformed[0]; dst->pos[1] = deformed[1]; dst->pos[2] = deformed[2];
			dst->color[0] = color[0]; dst->color[1] = color[1]; dst->color[2] = color[2]; dst->color[3] = color[3];
			dst->uv[0] = uv[0]; dst->uv[1] = uv[1];
			dst->lmUv[0] = src->lmUv[0]; dst->lmUv[1] = src->lmUv[1];
			memcpy( dst->normal, src->normal, sizeof( dst->normal ) );
			memcpy( dst->tangent, src->tangent, sizeof( dst->tangent ) );
			memcpy( dst->lightdir, src->lightdir, sizeof( dst->lightdir ) );
		}
	}

	if ( runOpen ) {
		qvkCmdDraw( vk.command_buffer, s_dynLMWriteOffset - runStart, 1, runStart, 0 );
	}
}

// Same as above but for stages 1+: no lightmap UV, writes vk2d_vertex_t into s_dyn2DMapped.
static void ExpandAndDrawExtraStage( const worldSurfRef_t *refs, int numRefs, const shader_t *shader, const shaderStage_t *stage ) {
	qboolean runOpen = qfalse;
	uint32_t runStart = 0;
	int j;
	qboolean stageInvariant = R_StageColorAlphaIsVertexInvariant( stage );
	byte stageColor[4];
	if ( stageInvariant ) {
		static const byte dummyBaseColor[4] = { 255, 255, 255, 255 }; // never read when stageInvariant -- see R_StageColorAlphaIsVertexInvariant's contract
		R_CalcStageColorAlpha( stage, NULL, dummyBaseColor, stageColor );
	}

	for ( j = 0; j < numRefs; j++ ) {
		const worldSurfRef_t *ref = &refs[j];
		uint32_t k;

		if ( !R_SurfaceIsVisible( ref->surfaceIndex ) ) {
			if ( runOpen ) {
				qvkCmdDraw( vk.command_buffer, s_dyn2DWriteOffset - runStart, 1, runStart, 0 );
				runOpen = qfalse;
			}
			continue;
		}

		if ( s_dyn2DWriteOffset + ref->indexCount > s_dyn2DCapacity ) {
			if ( runOpen ) {
				qvkCmdDraw( vk.command_buffer, s_dyn2DWriteOffset - runStart, 1, runStart, 0 );
				runOpen = qfalse;
			}
			if ( !s_dyn2DOverflowWarned ) {
				ri.Printf( PRINT_WARNING, "vk_world: dynamic extra-stage vertex buffer full, dropping remaining world geometry this frame\n" );
				s_dyn2DOverflowWarned = qtrue;
			}
			continue;
		}

		if ( !runOpen ) {
			runStart = s_dyn2DWriteOffset;
			runOpen = qtrue;
		}

		for ( k = 0; k < ref->indexCount; k++ ) {
			uint32_t vertIdx = s_cachedIndices[ref->firstIndex + k];
			const vkWorldVertex_t *src = &s_cachedVerts[vertIdx];
			vk2d_vertex_t *dst = &s_dyn2DMapped[s_dyn2DRegionBase + s_dyn2DWriteOffset++];
			const float *deformed;
			const byte *color;
			byte computedColor[4];
			float uv[2];

			deformed = GetDeformedWorldVertex( shader, vertIdx );
			if ( stageInvariant ) {
				color = stageColor;
			} else {
				R_CalcStageColorAlpha( stage, NULL, src->color, computedColor );
				color = computedColor;
			}
			if ( stage->isLightmap ) {
				R_CalcStageTexCoord( stage, src->lmUv[0], src->lmUv[1], deformed, NULL, uv );
			} else {
				R_CalcStageTexCoord( stage, src->uv[0], src->uv[1], deformed, NULL, uv );
			}

			dst->pos[0] = deformed[0]; dst->pos[1] = deformed[1]; dst->pos[2] = deformed[2];
			dst->color[0] = color[0]; dst->color[1] = color[1]; dst->color[2] = color[2]; dst->color[3] = color[3];
			dst->uv[0] = uv[0]; dst->uv[1] = uv[1];
		}
	}

	if ( runOpen ) {
		qvkCmdDraw( vk.command_buffer, s_dyn2DWriteOffset - runStart, 1, runStart, 0 );
	}
}


static void ExpandAndDrawWorldDlights( const worldSurfRef_t *refs, int numRefs, const shader_t *shader, const dlight_t *dlights, int numDlights ) {
	qboolean runOpen = qfalse;
	uint32_t runStart = 0;
	int j;

	for ( j = 0; j < numRefs; j++ ) {
		const worldSurfRef_t *ref = &refs[j];
		uint32_t k;
		uint32_t activeMask = 0;
		int lightIdx;

		if ( !R_SurfaceIsVisible( ref->surfaceIndex ) ) {
			if ( runOpen ) {
				qvkCmdDraw( vk.command_buffer, s_dynDlightWriteOffset - runStart, 1, runStart, 0 );
				runOpen = qfalse;
			}
			continue;
		}

		for ( lightIdx = 0; lightIdx < numDlights; lightIdx++ ) {
			if ( BoundsIntersectSphere( ref->mins, ref->maxs, dlights[lightIdx].origin, dlights[lightIdx].radius * DLIGHT_CULL_RADIUS_FACTOR ) ) {
				activeMask |= 1u << lightIdx;
			}
		}
		if ( activeMask == 0 ) {
			// no dlight reaches this surface -- same treatment as "not visible": skip the CPU deform+draw entirely
			if ( runOpen ) {
				qvkCmdDraw( vk.command_buffer, s_dynDlightWriteOffset - runStart, 1, runStart, 0 );
				runOpen = qfalse;
			}
			continue;
		}

		if ( s_dynDlightWriteOffset + ref->indexCount > s_dynDlightCapacity ) {
			if ( runOpen ) {
				qvkCmdDraw( vk.command_buffer, s_dynDlightWriteOffset - runStart, 1, runStart, 0 );
				runOpen = qfalse;
			}
			if ( !s_dynDlightOverflowWarned ) {
				ri.Printf( PRINT_WARNING, "vk_world: dynamic dlight vertex buffer full, dropping remaining world geometry this frame\n" );
				s_dynDlightOverflowWarned = qtrue;
			}
			continue;
		}

		if ( !runOpen ) {
			runStart = s_dynDlightWriteOffset;
			runOpen = qtrue;
		}

		for ( k = 0; k < ref->indexCount; k++ ) {
			uint32_t vertIdx = s_cachedIndices[ref->firstIndex + k];
			const vkWorldVertex_t *src = &s_cachedVerts[vertIdx];
			vk2d_vertex_t *dst = &s_dynDlightMapped[s_dynDlightRegionBase + s_dynDlightWriteOffset++];
			const float *deformed;
			vec3_t sum = { 0.0f, 0.0f, 0.0f };
			int r, g, b;

			deformed = GetDeformedWorldVertex( shader, vertIdx );

			for ( lightIdx = 0; lightIdx < numDlights; lightIdx++ ) {
				vec3_t dir;
				float dist, power, d;

				if ( !( activeMask & ( 1u << lightIdx ) ) ) {
					continue;
				}

				VectorSubtract( dlights[lightIdx].origin, deformed, dir );
				dist = VectorNormalize( dir );
				power = DLIGHT_AT_RADIUS * ( dlights[lightIdx].radius * dlights[lightIdx].radius );
				if ( dist < DLIGHT_MINIMUM_RADIUS ) {
					dist = DLIGHT_MINIMUM_RADIUS;
				}
				d = power / ( dist * dist );
				VectorMA( sum, d, dlights[lightIdx].color, sum );
			}

			r = (int)sum[0]; g = (int)sum[1]; b = (int)sum[2];
			dst->pos[0] = deformed[0]; dst->pos[1] = deformed[1]; dst->pos[2] = deformed[2];
			dst->color[0] = (byte)( r < 0 ? 0 : ( r > 255 ? 255 : r ) );
			dst->color[1] = (byte)( g < 0 ? 0 : ( g > 255 ? 255 : g ) );
			dst->color[2] = (byte)( b < 0 ? 0 : ( b > 255 ? 255 : b ) );
			dst->color[3] = 255;
			dst->uv[0] = src->uv[0]; dst->uv[1] = src->uv[1];
		}
	}

	if ( runOpen ) {
		qvkCmdDraw( vk.command_buffer, s_dynDlightWriteOffset - runStart, 1, runStart, 0 );
	}
}

// World-space position (origin + local*axis, same convention as vk_entity.c's LerpSurfaceVerts); rest passed through.
static void TransformBrushVerts( const refEntity_t *ent, const drawVert_t *src, int numVerts, brushVert_t *out ) {
	int i;
	for ( i = 0; i < numVerts; i++ ) {
		const float *local = src[i].xyz;
		out[i].pos[0] = ent->origin[0] + local[0] * ent->axis[0][0] + local[1] * ent->axis[1][0] + local[2] * ent->axis[2][0];
		out[i].pos[1] = ent->origin[1] + local[0] * ent->axis[0][1] + local[1] * ent->axis[1][1] + local[2] * ent->axis[2][1];
		out[i].pos[2] = ent->origin[2] + local[0] * ent->axis[0][2] + local[1] * ent->axis[1][2] + local[2] * ent->axis[2][2];
		out[i].color[0] = src[i].color[0]; out[i].color[1] = src[i].color[1];
		out[i].color[2] = src[i].color[2]; out[i].color[3] = src[i].color[3];
		out[i].uv[0] = src[i].st[0]; out[i].uv[1] = src[i].st[1];
		out[i].lmUv[0] = src[i].lightmap[0]; out[i].lmUv[1] = src[i].lightmap[1];
		{
			const float *n = src[i].normal;
			out[i].normal[0] = n[0] * ent->axis[0][0] + n[1] * ent->axis[1][0] + n[2] * ent->axis[2][0];
			out[i].normal[1] = n[0] * ent->axis[0][1] + n[1] * ent->axis[1][1] + n[2] * ent->axis[2][1];
			out[i].normal[2] = n[0] * ent->axis[0][2] + n[1] * ent->axis[1][2] + n[2] * ent->axis[2][2];
		}
	}
}

// (shader, lightmap) -> descriptor cache for brush-entity stage 0.
static VkDescriptorSet GetBrushLightmapDescriptor( qhandle_t shaderHandle, int lightmapIdx ) {
	shader_t *shader;
	image_t *diffuse;
	lightmapTex_t *lm;
	VkDescriptorSetAllocateInfo allocInfo;
	VkDescriptorImageInfo imageInfos[2];
	VkWriteDescriptorSet writes[2];
	int i;

	for ( i = 0; i < s_numBrushLMDescCache; i++ ) {
		if ( s_brushLMDescCache[i].shader == shaderHandle && s_brushLMDescCache[i].lightmapIdx == lightmapIdx ) {
			return s_brushLMDescCache[i].descriptor;
		}
	}

	if ( s_numBrushLMDescCache >= MAX_BRUSH_LM_DESCRIPTORS ) {
		ri.Printf( PRINT_WARNING, "vk_world: MAX_BRUSH_LM_DESCRIPTORS hit, reusing the last cached descriptor\n" );
		return s_brushLMDescCache[MAX_BRUSH_LM_DESCRIPTORS - 1].descriptor;
	}

	shader = R_GetShaderByHandle( shaderHandle );
	diffuse = ( shader && shader->numStages >= 1 && !shader->stages[0].isLightmap && shader->stages[0].image )
		? shader->stages[0].image : R_GetWhiteImage();
	lm = &s_lightmapTextures[lightmapIdx];

	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.pNext = NULL;
	allocInfo.descriptorPool = s_brushLMDescriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &vk.lightmap_set_layout;
	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &allocInfo, &s_brushLMDescCache[s_numBrushLMDescCache].descriptor ) );

	imageInfos[0].sampler = diffuse->vk_sampler;
	imageInfos[0].imageView = diffuse->vk_view;
	imageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfos[1].sampler = lm->sampler;
	imageInfos[1].imageView = lm->view;
	imageInfos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].pNext = NULL;
	writes[0].dstSet = s_brushLMDescCache[s_numBrushLMDescCache].descriptor;
	writes[0].dstBinding = 0;
	writes[0].dstArrayElement = 0;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[0].pImageInfo = &imageInfos[0];
	writes[0].pBufferInfo = NULL;
	writes[0].pTexelBufferView = NULL;
	writes[1] = writes[0];
	writes[1].dstBinding = 1;
	writes[1].pImageInfo = &imageInfos[1];
	qvkUpdateDescriptorSets( vk.device, 2, writes, 0, NULL );

	s_brushLMDescCache[s_numBrushLMDescCache].shader = shaderHandle;
	s_brushLMDescCache[s_numBrushLMDescCache].lightmapIdx = lightmapIdx;
	return s_brushLMDescCache[s_numBrushLMDescCache++].descriptor;
}

// Stage 0 (diffuse+lightmap) for one brush-entity surface; no visibility check, always drawn.
static void DrawBrushSurfaceStage0( const bspSurface_t *surf, const shader_t *shader, const brushVert_t *verts,
									 const int *indexes, int numIndexes, const byte *entityColor, const float entityTranslate[2] ) {
	const shaderStage_t *stage0 = &shader->stages[0];
	int lightmapIdx = ( surf->lightmapNum >= 0 && surf->lightmapNum < tr_world.numLightmaps ) ? surf->lightmapNum : s_whiteLightmapIdx;
	VkDescriptorSet descriptor;
	VkPipeline pipeline;
	VkDeviceSize vb_offset = (VkDeviceSize)s_dynBrushLMRegionBase * sizeof( vkWorldVertex_t );
	uint32_t base;
	int k;

	if ( s_dynBrushLMWriteOffset + (uint32_t)numIndexes > BRUSH_DYN_CAPACITY ) {
		if ( !s_dynBrushLMOverflowWarned ) {
			ri.Printf( PRINT_WARNING, "vk_world: dynamic brush-model lightmap buffer full, dropping remaining brush-entity geometry this frame\n" );
			s_dynBrushLMOverflowWarned = qtrue;
		}
		return;
	}

	descriptor = GetBrushLightmapDescriptor( tr_world.shaders[surf->shaderNum].shader, lightmapIdx );
	pipeline = vk_getWorldLightmapPipeline( shader->cullType, stage0->alphaTestFunc );
	vk_bind_pipeline_cached( pipeline );
	qvkCmdBindVertexBuffers( vk.command_buffer, 0, 1, &s_dynBrushLMBuffer, &vb_offset );
	vk_bind_descriptor_set_cached( vk.lightmap_pipeline_layout, 0, descriptor, 0, NULL );

	base = s_dynBrushLMWriteOffset;
	for ( k = 0; k < numIndexes; k++ ) {
		const brushVert_t *src = &verts[indexes[k]];
		vkWorldVertex_t *dst = &s_dynBrushLMMapped[s_dynBrushLMRegionBase + s_dynBrushLMWriteOffset++];
		float deformed[3];
		byte color[4];
		float uv[2];

		R_DeformPosition( shader, src->pos, src->normal, src->uv[0], deformed );
		R_CalcStageColorAlpha( stage0, entityColor, src->color, color );
		R_CalcStageTexCoord( stage0, src->uv[0], src->uv[1], deformed, entityTranslate, uv );

		dst->pos[0] = deformed[0]; dst->pos[1] = deformed[1]; dst->pos[2] = deformed[2];
		dst->color[0] = color[0]; dst->color[1] = color[1]; dst->color[2] = color[2]; dst->color[3] = color[3];
		dst->uv[0] = uv[0]; dst->uv[1] = uv[1];
		dst->lmUv[0] = src->lmUv[0]; dst->lmUv[1] = src->lmUv[1];
		pack_snorm16_3( src->normal, dst->normal );
		dst->tangent[0] = dst->tangent[1] = dst->tangent[2] = dst->tangent[3] = 0;
		dst->lightdir[0] = dst->lightdir[1] = dst->lightdir[2] = dst->lightdir[3] = 0;
	}
	qvkCmdDraw( vk.command_buffer, (uint32_t)numIndexes, 1, base, 0 );
}

// Stages 1+ for one brush-entity surface; no lightmap, binds the stage image directly.
static void DrawBrushSurfaceExtraStage( const shader_t *shader, int stageIdx, const brushVert_t *verts,
										 const int *indexes, int numIndexes, const byte *entityColor, const float entityTranslate[2] ) {
	const shaderStage_t *stage = &shader->stages[stageIdx];
	image_t *stageImage = R_GetStageImage( stage );
	VkPipeline pipeline;
	VkDeviceSize vb_offset = (VkDeviceSize)s_dynBrushExtraRegionBase * sizeof( vk2d_vertex_t );
	uint32_t base;
	int k;

	if ( !stageImage || stageImage->vk_descriptor == VK_NULL_HANDLE ) {
		return;
	}
	if ( s_dynBrushExtraWriteOffset + (uint32_t)numIndexes > BRUSH_DYN_CAPACITY ) {
		if ( !s_dynBrushExtraOverflowWarned ) {
			ri.Printf( PRINT_WARNING, "vk_world: dynamic brush-model extra-stage buffer full, dropping remaining brush-entity geometry this frame\n" );
			s_dynBrushExtraOverflowWarned = qtrue;
		}
		return;
	}

	pipeline = vk_getPipeline3D( sizeof( vk2d_vertex_t ), stage->blendEnabled, stage->srcBlend, stage->dstBlend,
		vk_cullModeForShader( shader->cullType ), stage->alphaTestFunc, R_InPortalView(), R_InMirrorView(), shader->polygonOffset, stage->depthWrite );
	vk_bind_pipeline_cached( pipeline );
	qvkCmdBindVertexBuffers( vk.command_buffer, 0, 1, &s_dynBrushExtraBuffer, &vb_offset );
	vk_bind_descriptor_set_cached( vk.pipeline_layout, 0, stageImage->vk_descriptor, 0, NULL );

	base = s_dynBrushExtraWriteOffset;
	for ( k = 0; k < numIndexes; k++ ) {
		const brushVert_t *src = &verts[indexes[k]];
		vk2d_vertex_t *dst = &s_dynBrushExtraMapped[s_dynBrushExtraRegionBase + s_dynBrushExtraWriteOffset++];
		float deformed[3];
		byte color[4];
		float uv[2];

		R_DeformPosition( shader, src->pos, src->normal, src->uv[0], deformed );
		R_CalcStageColorAlpha( stage, entityColor, src->color, color );
		R_CalcStageTexCoord( stage, src->uv[0], src->uv[1], deformed, entityTranslate, uv );

		dst->pos[0] = deformed[0]; dst->pos[1] = deformed[1]; dst->pos[2] = deformed[2];
		dst->color[0] = color[0]; dst->color[1] = color[1]; dst->color[2] = color[2]; dst->color[3] = color[3];
		dst->uv[0] = uv[0]; dst->uv[1] = uv[1];
	}
	qvkCmdDraw( vk.command_buffer, (uint32_t)numIndexes, 1, base, 0 );
}

static qboolean BrushModelCulled( const refEntity_t *ent, const dmodel_t *submodel, const cullPlane_t frustum[NUM_FRUSTUM_PLANES] ) {
	vec3_t worldMins, worldMaxs;
	int i;

	if ( ent->nonNormalizedAxes ) {
		return qfalse;
	}

	ClearBounds( worldMins, worldMaxs );
	for ( i = 0; i < 8; i++ ) {
		vec3_t local, world;
		local[0] = ( i & 1 ) ? submodel->maxs[0] : submodel->mins[0];
		local[1] = ( i & 2 ) ? submodel->maxs[1] : submodel->mins[1];
		local[2] = ( i & 4 ) ? submodel->maxs[2] : submodel->mins[2];
		world[0] = ent->origin[0] + local[0] * ent->axis[0][0] + local[1] * ent->axis[1][0] + local[2] * ent->axis[2][0];
		world[1] = ent->origin[1] + local[0] * ent->axis[0][1] + local[1] * ent->axis[1][1] + local[2] * ent->axis[2][1];
		world[2] = ent->origin[2] + local[0] * ent->axis[0][2] + local[1] * ent->axis[1][2] + local[2] * ent->axis[2][2];
		AddPointToBounds( world, worldMins, worldMaxs );
	}

	return !R_CullBoundsVsFrustum( worldMins, worldMaxs, frustum );
}

void vk_world_draw_brushmodel( const refEntity_t *ent, int bmodelIndex, const float mvp[16], const float fogPush[12],
								 const cullPlane_t frustum[NUM_FRUSTUM_PLANES] ) {
	const dmodel_t *submodel;
	float push_data[32];
	int surfIdx, s;

	if ( !s_built || bmodelIndex < 0 || bmodelIndex >= tr_world.numSubmodels ) {
		return;
	}
	submodel = &tr_world.submodels[bmodelIndex];

	if ( BrushModelCulled( ent, submodel, frustum ) ) {
		return;
	}

	memset( push_data, 0, sizeof( push_data ) );
	memcpy( push_data, mvp, 16 * sizeof( float ) );
	memcpy( push_data + 16, fogPush, 12 * sizeof( float ) );
	if ( R_InPortalView() ) {
		vec3_t normal;
		float dist;
		R_GetActivePortalPlane( normal, &dist );
		push_data[28] = normal[0]; push_data[29] = normal[1]; push_data[30] = normal[2]; push_data[31] = dist;
	}
	qvkCmdPushConstants( vk.command_buffer, vk.lightmap_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		0, sizeof( push_data ), push_data );

	for ( surfIdx = submodel->firstSurface; surfIdx < submodel->firstSurface + submodel->numSurfaces; surfIdx++ ) {
		const bspSurface_t *surf = &tr_world.surfaces[surfIdx];
		shader_t *shader = R_GetShaderByHandle( tr_world.shaders[surf->shaderNum].shader );
		const drawVert_t *rawVerts;
		const int *indexes;
		int numVerts, numIndexes;
		drawVert_t *tessVerts = NULL;
		int *tessIdx = NULL;

		if ( !shader || shader->isSky || shader->numStages < 1 ) {
			continue;
		}
		if ( tr_world.shaders[surf->shaderNum].surfaceFlags & SURF_NODRAW ) {
			continue; // see the static-world build's matching check in vk_world_build
		}

		if ( surf->surfaceType == MST_PLANAR || surf->surfaceType == MST_TRIANGLE_SOUP ) {
			numVerts = surf->numVerts;
			rawVerts = &tr_world.drawVerts[surf->firstVert];
			indexes = &tr_world.drawIndexes[surf->firstIndex];
			numIndexes = surf->numIndexes;
		} else if ( surf->surfaceType == MST_PATCH ) {
			int pv = R_PatchTessVertCount( &tr_world.drawVerts[surf->firstVert], surf->patchWidth, surf->patchHeight );
			int pi = R_PatchTessIndexCount( &tr_world.drawVerts[surf->firstVert], surf->patchWidth, surf->patchHeight );
			if ( pv <= 0 || pi <= 0 || pv > BRUSH_SURF_MAX_VERTS ) {
				continue; // malformed control grid, or too large for the scratch buffer
			}
			tessVerts = malloc( pv * sizeof( drawVert_t ) );
			tessIdx = malloc( pi * sizeof( int ) );
			R_TessellatePatch( &tr_world.drawVerts[surf->firstVert], surf->patchWidth, surf->patchHeight, tessVerts, tessIdx );
			numVerts = pv;
			rawVerts = tessVerts;
			indexes = tessIdx;
			numIndexes = pi;
		} else {
			continue; // flare/foliage: not drawable
		}

		if ( numVerts > BRUSH_SURF_MAX_VERTS ) {
			if ( tessVerts ) free( tessVerts );
			if ( tessIdx ) free( tessIdx );
			continue;
		}

		TransformBrushVerts( ent, rawVerts, numVerts, s_brushVertScratch );

		if ( !( shader->stages[0].blendEnabled && shader->explicitlyDefined ) ) {
			DrawBrushSurfaceStage0( surf, shader, s_brushVertScratch, indexes, numIndexes, ent->shaderRGBA, ent->shaderTexCoord );
		}
		for ( s = ( shader->stages[0].blendEnabled && shader->explicitlyDefined ) ? 0 : 1; s < shader->numStages; s++ ) {
			DrawBrushSurfaceExtraStage( shader, s, s_brushVertScratch, indexes, numIndexes, ent->shaderRGBA, ent->shaderTexCoord );
		}

		if ( tessVerts ) free( tessVerts );
		if ( tessIdx ) free( tessIdx );
	}
}

// Resets the shared dynamic-buffer write cursors; call once per real frame (vk_world_draw can run more than once per frame -- resetting inside it, as before, let one scene's reset clobber another's).
void vk_world_begin_frame( void ) {
	uint32_t slot = vk_current_frame_slot();
	s_deformFrameStamp++;
	if ( s_deformFrameStamp == 0 ) {
		s_deformFrameStamp = 1;
	}

	s_dynLMWriteOffset = 0;
	s_dyn2DWriteOffset = 0;
	s_dynBrushLMWriteOffset = 0;
	s_dynBrushExtraWriteOffset = 0;
	s_dynDlightWriteOffset = 0;
	s_dynFoliageWriteOffset = 0;

	s_dynLMRegionBase = slot * s_dynLMCapacity;
	s_dyn2DRegionBase = slot * s_dyn2DCapacity;
	s_dynDlightRegionBase = slot * s_dynDlightCapacity;
	s_dynBrushLMRegionBase = slot * (uint32_t)BRUSH_DYN_CAPACITY;
	s_dynBrushExtraRegionBase = slot * (uint32_t)BRUSH_DYN_CAPACITY;
	s_dynFoliageRegionBase = slot * s_dynFoliageCapacity;
}

// Shared by vk_world_draw/vk_world_draw_transparent: mvp/fog/portal-clip push constants, identical for both passes within a frame.
static void PushWorldTransform( const float mvp[16], const float fogPush[12] ) {
	float push_data[32];

	memset( push_data, 0, sizeof( push_data ) );
	memcpy( push_data, mvp, 16 * sizeof( float ) );
	memcpy( push_data + 16, fogPush, 12 * sizeof( float ) );
	if ( R_InPortalView() ) {
		// push_data[28..31] is the Transform struct's clipPlane vec4 (world_lightmap.vert).
		vec3_t normal;
		float dist;
		R_GetActivePortalPlane( normal, &dist );
		push_data[28] = normal[0]; push_data[29] = normal[1]; push_data[30] = normal[2]; push_data[31] = dist;
	}
	qvkCmdPushConstants( vk.command_buffer, vk.lightmap_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		0, sizeof( push_data ), push_data );
}

static void PushStaticWorldTransform( const float mvp[16], const float fogPush[12], const float colorMul[4], const float colorAdd[4] ) {
	float push_data[40];

	memset( push_data, 0, sizeof( push_data ) );
	memcpy( push_data, mvp, 16 * sizeof( float ) );
	memcpy( push_data + 16, fogPush, 12 * sizeof( float ) );
	if ( R_InPortalView() ) {
		vec3_t normal;
		float dist;
		R_GetActivePortalPlane( normal, &dist );
		push_data[28] = normal[0]; push_data[29] = normal[1]; push_data[30] = normal[2]; push_data[31] = dist;
	}
	memcpy( push_data + 32, colorMul, 4 * sizeof( float ) );
	memcpy( push_data + 36, colorAdd, 4 * sizeof( float ) );
	qvkCmdPushConstants( vk.command_buffer, vk.static_lightmap_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		0, sizeof( push_data ), push_data );
}

static void ComputeStaticStageColor( const shader_t *shader, qboolean useBakedVertexColor, float colorMul[4], float colorAdd[4] ) {
	shaderStage_t stage0Local;
	const shaderStage_t *stage0 = &shader->stages[0];
	static const byte zero[4] = { 0, 0, 0, 0 };
	static const byte full[4] = { 255, 255, 255, 255 };
	byte add[4], probe[4];
	int c;

	if ( useBakedVertexColor && !shader->explicitlyDefined && stage0->rgbGen == CGEN_LIGHTING_DIFFUSE ) {
		stage0Local = *stage0;
		stage0Local.rgbGen = CGEN_EXACT_VERTEX;
		stage0 = &stage0Local;
	}

	R_CalcStageColorAlpha( stage0, NULL, zero, add );
	R_CalcStageColorAlpha( stage0, NULL, full, probe );

	for ( c = 0; c < 4; c++ ) {
		colorAdd[c] = add[c] * ( 1.0f / 255.0f );
		colorMul[c] = probe[c] * ( 1.0f / 255.0f ) - colorAdd[c];
	}
}

static void DrawStaticStage0( const worldSurfRef_t *refs, int numRefs ) {
	qboolean runOpen = qfalse;
	uint32_t runStart = 0, runEnd = 0;
	int j;

	for ( j = 0; j < numRefs; j++ ) {
		const worldSurfRef_t *ref = &refs[j];

		if ( !R_SurfaceIsVisible( ref->surfaceIndex ) ) {
			if ( runOpen ) {
				qvkCmdDrawIndexed( vk.command_buffer, runEnd - runStart, 1, runStart, 0, 0 );
				runOpen = qfalse;
			}
			continue;
		}

		if ( runOpen && ref->firstIndex == runEnd ) {
			runEnd = ref->firstIndex + ref->indexCount;
			continue;
		}

		if ( runOpen ) {
			qvkCmdDrawIndexed( vk.command_buffer, runEnd - runStart, 1, runStart, 0, 0 );
		}
		runStart = ref->firstIndex;
		runEnd = ref->firstIndex + ref->indexCount;
		runOpen = qtrue;
	}

	if ( runOpen ) {
		qvkCmdDrawIndexed( vk.command_buffer, runEnd - runStart, 1, runStart, 0, 0 );
	}
}

static qboolean IsTransparentBatch( const shader_t *shader ) {
	const shaderStage_t *stage0 = &shader->stages[0];

	if ( !stage0->blendEnabled || !shader->explicitlyDefined ) {
		return qfalse;
	}

	if ( stage0->srcBlend == VK_BLEND_FACTOR_ONE && stage0->dstBlend == VK_BLEND_FACTOR_ZERO ) {
		return qfalse;
	}
	return qtrue;
}

void vk_world_draw( const float mvp[16], const float fogPush[12] ) {
	VkDeviceSize lmVbOffset = (VkDeviceSize)s_dynLMRegionBase * sizeof( vkWorldVertex_t );
	VkDeviceSize vb2dOffset = (VkDeviceSize)s_dyn2DRegionBase * sizeof( vk2d_vertex_t );
	VkDeviceSize dlightVbOffset = (VkDeviceSize)s_dynDlightRegionBase * sizeof( vk2d_vertex_t );
	int i;
	qboolean lmBufferBound = qfalse, bound2DBuffer = qfalse, staticBufferBound = qfalse;
	static qboolean firstDraw = qtrue;

	if ( !s_built ) {
		return;
	}

	if ( firstDraw ) {
		ri.Printf( PRINT_ALL, "...vk_world_draw checkpoint: first draw starting, %d batches\n", s_numBatches );
	}

	PushWorldTransform( mvp, fogPush );

	for ( i = 0; i < s_numBatches; i++ ) {
		const worldBatch_t *batch = &s_batches[i];
		shader_t *shader = R_GetShaderByHandle( batch->shader );
		VkPipeline pipeline;
		int s, firstExtraStage;

		if ( batch->descriptor == VK_NULL_HANDLE ) {
			continue;
		}
		if ( IsTransparentBatch( shader ) ) {
			continue; // drawn later by vk_world_draw_transparent, after opaque entities
		}

		if ( shader->stages[0].isLightmap ) {
			firstExtraStage = 0;
		} else if ( batch->staticEligible ) {
			float colorMul[4], colorAdd[4];
			VkDeviceSize zeroOffset = 0;

			pipeline = vk_getWorldStaticLightmapPipeline( shader->cullType, shader->stages[0].alphaTestFunc );
			vk_bind_pipeline_cached( pipeline );
			if ( !staticBufferBound ) {
				qvkCmdBindVertexBuffers( vk.command_buffer, 0, 1, &s_staticVertexBuffer, &zeroOffset );
				qvkCmdBindIndexBuffer( vk.command_buffer, s_staticIndexBuffer, 0, VK_INDEX_TYPE_UINT32 );
				staticBufferBound = qtrue; lmBufferBound = qfalse; bound2DBuffer = qfalse;
			}

			vk_bind_descriptor_set_cached( vk.static_lightmap_pipeline_layout, 0, batch->descriptor, 0, NULL );

			ComputeStaticStageColor( shader, batch->lightmapIdx == s_whiteLightmapIdx, colorMul, colorAdd );
			PushStaticWorldTransform( mvp, fogPush, colorMul, colorAdd );
			DrawStaticStage0( &s_surfRefs[batch->firstSurfRef], batch->numSurfRefs );
			PushWorldTransform( mvp, fogPush );
			firstExtraStage = 1;
		} else {
			// Batches are already split per-shader (worldBatch_t is keyed by shader+lightmap), so this is free to vary.
			pipeline = vk_getWorldLightmapPipeline( shader->cullType, shader->stages[0].alphaTestFunc );
			vk_bind_pipeline_cached( pipeline );
			if ( !lmBufferBound ) {
				qvkCmdBindVertexBuffers( vk.command_buffer, 0, 1, &s_dynLMBuffer, &lmVbOffset );
				lmBufferBound = qtrue; bound2DBuffer = qfalse; staticBufferBound = qfalse;
			}

			vk_bind_descriptor_set_cached( vk.lightmap_pipeline_layout, 0, batch->descriptor, 0, NULL );

			ExpandAndDrawStage0( &s_surfRefs[batch->firstSurfRef], batch->numSurfRefs, shader,
				batch->lightmapIdx == s_whiteLightmapIdx );
			firstExtraStage = 1;
		}

		for ( s = firstExtraStage; s < shader->numStages; s++ ) {
			const shaderStage_t *stage = &shader->stages[s];
			VkDescriptorSet stageDescriptor;

			if ( stage->isLightmap ) {
				stageDescriptor = s_lightmapTextures[batch->lightmapIdx].descriptor;
				if ( stageDescriptor == VK_NULL_HANDLE ) {
					continue;
				}
			} else {
				image_t *stageImage = R_GetStageImage( stage );
				if ( !stageImage || stageImage->vk_descriptor == VK_NULL_HANDLE ) {
					continue;
				}
				stageDescriptor = stageImage->vk_descriptor;
			}

			pipeline = vk_getPipeline3D( sizeof( vk2d_vertex_t ), stage->blendEnabled, stage->srcBlend, stage->dstBlend,
				vk_cullModeForShader( shader->cullType ), stage->alphaTestFunc, R_InPortalView(), R_InMirrorView(), shader->polygonOffset, stage->depthWrite );
			vk_bind_pipeline_cached( pipeline );
			if ( !bound2DBuffer ) {
				qvkCmdBindVertexBuffers( vk.command_buffer, 0, 1, &s_dyn2DBuffer, &vb2dOffset );
				bound2DBuffer = qtrue; lmBufferBound = qfalse; staticBufferBound = qfalse;
			}

			vk_bind_descriptor_set_cached( vk.pipeline_layout, 0, stageDescriptor, 0, NULL );

			ExpandAndDrawExtraStage( &s_surfRefs[batch->firstSurfRef], batch->numSurfRefs, shader, stage );
		}
	}

	{
		int numDlights;
		const dlight_t *dlights = R_GetSceneDlights( &numDlights );

		if ( numDlights > 0 ) {
			qvkCmdBindVertexBuffers( vk.command_buffer, 0, 1, &s_dynDlightBuffer, &dlightVbOffset );
			lmBufferBound = qfalse; bound2DBuffer = qfalse;

			for ( i = 0; i < s_numBatches; i++ ) {
				const worldBatch_t *batch = &s_batches[i];
				shader_t *shader;
				image_t *diffuse;
				VkPipeline pipeline;

				if ( batch->descriptor == VK_NULL_HANDLE ) {
					continue;
				}
				shader = R_GetShaderByHandle( batch->shader );
				diffuse = ( shader->numStages >= 1 && !shader->stages[0].isLightmap && shader->stages[0].image )
					? shader->stages[0].image : R_GetWhiteImage();
				if ( diffuse->vk_descriptor == VK_NULL_HANDLE ) {
					continue;
				}

				pipeline = vk_getPipeline3D( sizeof( vk2d_vertex_t ), VK_TRUE, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE,
					vk_cullModeForShader( shader->cullType ), shader->stages[0].alphaTestFunc, R_InPortalView(), R_InMirrorView(), VK_FALSE, VK_FALSE );
				vk_bind_pipeline_cached( pipeline );

				vk_bind_descriptor_set_cached( vk.pipeline_layout, 0, diffuse->vk_descriptor, 0, NULL );

				ExpandAndDrawWorldDlights( &s_surfRefs[batch->firstSurfRef], batch->numSurfRefs, shader, dlights, numDlights );
			}
		}
	}

	if ( !r_showtris ) {
		r_showtris = ri.Cvar_Get( "r_showtris", "0", CVAR_CHEAT );
	}
	if ( r_showtris->integer && vk.pipeline_world_wireframe != VK_NULL_HANDLE ) {
		vk_bind_pipeline_cached( vk.pipeline_world_wireframe );
		if ( !lmBufferBound ) {
			qvkCmdBindVertexBuffers( vk.command_buffer, 0, 1, &s_dynLMBuffer, &lmVbOffset );
			lmBufferBound = qtrue; bound2DBuffer = qfalse;
		}
		for ( i = 0; i < s_numBatches; i++ ) {
			const worldBatch_t *batch = &s_batches[i];
			shader_t *shader;

			if ( batch->descriptor == VK_NULL_HANDLE ) {
				continue;
			}
			shader = R_GetShaderByHandle( batch->shader );
			vk_bind_descriptor_set_cached( vk.lightmap_pipeline_layout, 0, batch->descriptor, 0, NULL );
			ExpandAndDrawStage0( &s_surfRefs[batch->firstSurfRef], batch->numSurfRefs, shader,
				batch->lightmapIdx == s_whiteLightmapIdx );
		}
	}

	if ( firstDraw ) {
		ri.Printf( PRINT_ALL, "...vk_world_draw checkpoint: first draw recorded ok\n" );
		firstDraw = qfalse;
	}
}

void vk_world_draw_transparent( const float mvp[16], const float fogPush[12] ) {
	VkDeviceSize vb2dOffset = (VkDeviceSize)s_dyn2DRegionBase * sizeof( vk2d_vertex_t );
	qboolean bound2DBuffer = qfalse;
	int i;

	if ( !s_built ) {
		return;
	}

	PushWorldTransform( mvp, fogPush );

	for ( i = 0; i < s_numBatches; i++ ) {
		const worldBatch_t *batch = &s_batches[i];
		shader_t *shader = R_GetShaderByHandle( batch->shader );
		int s;

		if ( batch->descriptor == VK_NULL_HANDLE || !IsTransparentBatch( shader ) ) {
			continue;
		}

		for ( s = 0; s < shader->numStages; s++ ) {
			const shaderStage_t *stage = &shader->stages[s];
			VkDescriptorSet stageDescriptor;
			VkPipeline pipeline;

			if ( stage->isLightmap ) {
				stageDescriptor = s_lightmapTextures[batch->lightmapIdx].descriptor;
				if ( stageDescriptor == VK_NULL_HANDLE ) {
					continue;
				}
			} else {
				image_t *stageImage = R_GetStageImage( stage );
				if ( !stageImage || stageImage->vk_descriptor == VK_NULL_HANDLE ) {
					continue;
				}
				stageDescriptor = stageImage->vk_descriptor;
			}

			pipeline = vk_getPipeline3D( sizeof( vk2d_vertex_t ), stage->blendEnabled, stage->srcBlend, stage->dstBlend,
				vk_cullModeForShader( shader->cullType ), stage->alphaTestFunc, R_InPortalView(), R_InMirrorView(), shader->polygonOffset, stage->depthWrite );
			vk_bind_pipeline_cached( pipeline );
			if ( !bound2DBuffer ) {
				qvkCmdBindVertexBuffers( vk.command_buffer, 0, 1, &s_dyn2DBuffer, &vb2dOffset );
				bound2DBuffer = qtrue;
			}

			vk_bind_descriptor_set_cached( vk.pipeline_layout, 0, stageDescriptor, 0, NULL );

			ExpandAndDrawExtraStage( &s_surfRefs[batch->firstSurfRef], batch->numSurfRefs, shader, stage );
		}
	}
}

void vk_world_draw_foliage( const refdef_t *fd, const float mvp[16], const float fogPush[12] ) {
	cullPlane_t frustum[NUM_FRUSTUM_PLANES];
	qboolean haveFrustum = qfalse;
	float fovScale = fd->fov_x * ( 1.0f / 90.0f );
	float cullScale;
	VkDeviceSize vb_offset = (VkDeviceSize)s_dynFoliageRegionBase * sizeof( vk2d_vertex_t );
	int i;

	if ( !s_built || s_numFoliageSurfs == 0 ) {
		return;
	}

	cullScale = r_foliageCullDistance ? ( r_foliageCullDistance->value * 0.01f ) : 0.3f;
	if ( cullScale < 0.01f ) {
		cullScale = 0.01f; // keep a sliver of range so foliage still fades out near the player instead of collapsing to a point
	}

	PushWorldTransform( mvp, fogPush );
	qvkCmdBindVertexBuffers( vk.command_buffer, 0, 1, &s_dynFoliageBuffer, &vb_offset );

	for ( i = 0; i < s_numFoliageSurfs; i++ ) {
		const foliageSurf_t *fsurf = &s_foliageSurfs[i];
		shader_t *shader = R_GetShaderByHandle( fsurf->shader );
		int s;

		if ( !shader ) {
			continue;
		}

		for ( s = 0; s < shader->numStages; s++ ) {
			const shaderStage_t *stage = &shader->stages[s];
			VkDescriptorSet stageDescriptor;
			VkPipeline pipeline;
			qboolean runOpen = qfalse;
			uint32_t runStart = 0;
			int inst;

			if ( stage->isLightmap ) {
				stageDescriptor = s_lightmapTextures[s_whiteLightmapIdx].descriptor; // vertex-lit only -- no real lightmap UVs here
			} else {
				image_t *stageImage = R_GetStageImage( stage );
				if ( !stageImage || stageImage->vk_descriptor == VK_NULL_HANDLE ) {
					continue;
				}
				stageDescriptor = stageImage->vk_descriptor;
			}

			pipeline = vk_getPipeline3D( sizeof( vk2d_vertex_t ), stage->blendEnabled, stage->srcBlend, stage->dstBlend,
				vk_cullModeForShader( shader->cullType ), stage->alphaTestFunc, R_InPortalView(), R_InMirrorView(),
				shader->polygonOffset, stage->depthWrite );
			vk_bind_pipeline_cached( pipeline );
			vk_bind_descriptor_set_cached( vk.pipeline_layout, 0, stageDescriptor, 0, NULL );

			for ( inst = 0; inst < fsurf->numInstances; inst++ ) {
				const foliageInstance_t *instance = &s_foliageInstances[fsurf->firstInstance + inst];
				byte baseColor[4] = { 0, 0, 0, 0 };
				qboolean visible = qtrue;
				int k;

				if ( shader->distanceCull[1] > 0.0f ) {
					vec3_t toInst;
					float z;

					if ( !haveFrustum ) {
						R_SetupFrustumPlanes( fd, frustum );
						haveFrustum = qtrue;
					}

					VectorSubtract( instance->origin, fd->vieworg, toInst );
					z = fovScale * DotProduct( toInst, fd->viewaxis[0] );

					if ( z < -64.0f ) {
						visible = qfalse;
					} else {
						vec3_t mins, maxs;
						mins[0] = instance->origin[0] - 64.0f; mins[1] = instance->origin[1] - 64.0f; mins[2] = instance->origin[2] - 64.0f;
						maxs[0] = instance->origin[0] + 64.0f; maxs[1] = instance->origin[1] + 64.0f; maxs[2] = instance->origin[2] + 64.0f;
						if ( !R_CullBoundsVsFrustum( mins, maxs, frustum ) ) {
							visible = qfalse;
						}
					}

					if ( visible ) {
						float inner = shader->distanceCull[0] * cullScale;
						float outer = shader->distanceCull[1] * cullScale;
						float invRange = ( outer - inner > 0.0f ) ? ( 1.0f / ( outer - inner ) ) : 0.0f;
						float zBias = z;
						float alpha;

						// Cheap per-instance dither so the fade band isn't a uniform ring around the player.
						if ( inst & 1 ) {
							zBias *= 1.25f;
							if ( inst & 2 ) {
								zBias *= 1.25f;
							}
						}

						alpha = ( outer - zBias ) * invRange;
						if ( alpha < shader->distanceCull[2] ) {
							visible = qfalse;
						} else {
							baseColor[0] = instance->color[0]; baseColor[1] = instance->color[1]; baseColor[2] = instance->color[2];
							baseColor[3] = ( alpha > 1.0f ) ? 255 : (byte)( alpha * 255.0f );
						}
					}
				} else {
					baseColor[0] = instance->color[0]; baseColor[1] = instance->color[1]; baseColor[2] = instance->color[2];
					baseColor[3] = instance->color[3];
				}

				if ( visible && s_dynFoliageWriteOffset + (uint32_t)fsurf->numIndexes > s_dynFoliageCapacity ) {
					if ( !s_dynFoliageOverflowWarned ) {
						ri.Printf( PRINT_WARNING, "vk_world: dynamic foliage vertex buffer full, dropping remaining foliage this frame\n" );
						s_dynFoliageOverflowWarned = qtrue;
					}
					visible = qfalse;
				}

				if ( !visible ) {
					if ( runOpen ) {
						qvkCmdDraw( vk.command_buffer, s_dynFoliageWriteOffset - runStart, 1, runStart, 0 );
						runOpen = qfalse;
					}
					continue;
				}

				if ( !runOpen ) {
					runStart = s_dynFoliageWriteOffset;
					runOpen = qtrue;
				}

				for ( k = 0; k < fsurf->numIndexes; k++ ) {
					int localIdx = tr_world.drawIndexes[fsurf->firstIndex + k];
					const drawVert_t *src = &tr_world.drawVerts[fsurf->firstVert + localIdx];
					vk2d_vertex_t *dst = &s_dynFoliageMapped[s_dynFoliageRegionBase + s_dynFoliageWriteOffset++];
					vec3_t worldPos, deformed;
					byte color[4];
					float uv[2];

					VectorAdd( src->xyz, instance->origin, worldPos );
					R_DeformPosition( shader, worldPos, src->normal, src->st[0], deformed );
					R_CalcStageColorAlpha( stage, NULL, baseColor, color );
					if ( stage->isLightmap ) {
						R_CalcStageTexCoord( stage, src->lightmap[0], src->lightmap[1], deformed, NULL, uv );
					} else {
						R_CalcStageTexCoord( stage, src->st[0], src->st[1], deformed, NULL, uv );
					}

					dst->pos[0] = deformed[0]; dst->pos[1] = deformed[1]; dst->pos[2] = deformed[2];
					dst->color[0] = color[0]; dst->color[1] = color[1]; dst->color[2] = color[2]; dst->color[3] = color[3];
					dst->uv[0] = uv[0]; dst->uv[1] = uv[1];
				}
			}

			if ( runOpen ) {
				qvkCmdDraw( vk.command_buffer, s_dynFoliageWriteOffset - runStart, 1, runStart, 0 );
			}
		}
	}
}

qboolean vk_world_findVisiblePortalSurface( vec3_t outNormal, float *outDist, vec3_t outPoint ) {
	int i, j;

	if ( !s_built ) {
		return qfalse;
	}

	for ( i = 0; i < s_numBatches; i++ ) {
		const worldBatch_t *batch = &s_batches[i];
		shader_t *shader;

		if ( batch->descriptor == VK_NULL_HANDLE ) {
			continue;
		}
		shader = R_GetShaderByHandle( batch->shader );
		if ( !shader->isPortal ) {
			continue;
		}

		for ( j = 0; j < batch->numSurfRefs; j++ ) {
			const worldSurfRef_t *ref = &s_surfRefs[batch->firstSurfRef + j];
			const vkWorldVertex_t *v0, *v1, *v2;
			vec3_t e1, e2, normal;
			float len;

			if ( !R_SurfaceIsVisible( ref->surfaceIndex ) || ref->indexCount < 3 ) {
				continue;
			}

			v0 = &s_cachedVerts[s_cachedIndices[ref->firstIndex + 0]];
			v1 = &s_cachedVerts[s_cachedIndices[ref->firstIndex + 1]];
			v2 = &s_cachedVerts[s_cachedIndices[ref->firstIndex + 2]];

			VectorSubtract( v1->pos, v0->pos, e1 );
			VectorSubtract( v2->pos, v0->pos, e2 );
			CrossProduct( e1, e2, normal );
			len = VectorNormalize( normal );
			if ( len < 0.001f ) {
				continue; // degenerate triangle -- try this batch's next surface
			}

			VectorCopy( normal, outNormal );
			*outDist = DotProduct( normal, v0->pos );
			VectorAdd( v0->pos, v1->pos, outPoint );
			VectorAdd( outPoint, v2->pos, outPoint );
			VectorScale( outPoint, 1.0f / 3.0f, outPoint );
			return qtrue;
		}
	}

	return qfalse;
}
