// vk_world.c -- sorts/batches tr_world's (shader, lightmap) surfaces once at map load, then expands each visible surface into a per-frame dynamic vertex buffer with real per-frame rgbGen/tcMod (like vk_entity.c/vk_poly.c).

#include <stdlib.h>
#include <string.h>

#include "VKimpl.h"
#include "vk_instance.h"
#include "vk_pipelines.h"
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
#include "vk_world.h"

// Same falloff shape vk_entity.c's ComputeEntityTint uses for dlight contributions.
#define DLIGHT_AT_RADIUS 16.0f
#define DLIGHT_MINIMUM_RADIUS 16.0f

// One entry per drawable surface; consecutive entries in a batch are contiguous in s_cachedIndices.
typedef struct {
	int surfaceIndex;
	uint32_t firstIndex;
	uint32_t indexCount;
} worldSurfRef_t;

// Keyed by (shader, lightmapIdx); lightmapIdx always points at a real lightmap or the synthetic white fallback.
typedef struct {
	qhandle_t shader;
	int lightmapIdx;
	int firstSurfRef;
	int numSurfRefs;
	VkDescriptorSet descriptor; // diffuse (binding 0) + lightmap (binding 1), built once at load time
} worldBatch_t;

typedef struct {
	VkImage image;
	VkDeviceMemory memory;
	VkImageView view;
	VkSampler sampler;
} lightmapTex_t;

// Cached once at load (tessellation included); the per-frame source data for expansion below.
// Vertex color here is the BSP's raw (overbright-shifted) color -- rgbGen/tcMod are evaluated fresh
// every frame from this, never baked in.
static vkWorldVertex_t *s_cachedVerts;
static uint32_t s_numCachedVerts;
static uint32_t *s_cachedIndices;
static uint32_t s_numCachedIndices;

static worldBatch_t *s_batches;
static int s_numBatches;
static worldSurfRef_t *s_surfRefs;

static lightmapTex_t *s_lightmapTextures;
static int s_numLightmapTextures; // tr_world.numLightmaps real ones, plus one synthetic white fallback
static VkDescriptorPool s_lmDescriptorPool;

static qboolean s_built = qfalse;

static cvar_t *r_showtris;

// Per-frame dynamic buffers that visible-surface geometry is expanded into. Sized at load time from
// s_numCachedIndices, which is a hard upper bound on any single frame's visible vertex count (the
// *2 covers the normal stage-0 pass plus a same-frame r_showtris wireframe re-pass, which reuses the
// same expansion into the same buffer rather than keeping a separate wireframe-only path).
static VkBuffer s_dynLMBuffer;
static VkDeviceMemory s_dynLMMemory;
static vkWorldVertex_t *s_dynLMMapped;
static uint32_t s_dynLMCapacity;
static uint32_t s_dynLMWriteOffset;
static qboolean s_dynLMOverflowWarned;

static VkBuffer s_dyn2DBuffer;
static VkDeviceMemory s_dyn2DMemory;
static vk2d_vertex_t *s_dyn2DMapped;
static uint32_t s_dyn2DCapacity;
static uint32_t s_dyn2DWriteOffset;
static qboolean s_dyn2DOverflowWarned;

// Dynamic lights are a separate additive pass over world geometry (previously they had zero effect
// on world surfaces at all); kept in its own buffer so it doesn't contend with extra-stage capacity.
static VkBuffer s_dynDlightBuffer;
static VkDeviceMemory s_dynDlightMemory;
static vk2d_vertex_t *s_dynDlightMapped;
static uint32_t s_dynDlightCapacity;
static uint32_t s_dynDlightWriteOffset;
static qboolean s_dynDlightOverflowWarned;

extern uint32_t vk_find_memory_type( uint32_t type_bits, VkMemoryPropertyFlags properties );

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

	if ( s_lmDescriptorPool ) { qvkDestroyDescriptorPool( vk.device, s_lmDescriptorPool, NULL ); s_lmDescriptorPool = VK_NULL_HANDLE; }

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

	s_built = qfalse;
}

void vk_world_shutdown( void ) {
	vk_world_clear();
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
		int pv = R_PatchTessVertCount( surf->patchWidth, surf->patchHeight );
		int pi = R_PatchTessIndexCount( surf->patchWidth, surf->patchHeight );
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

	vk_world_clear();

	if ( !tr_world.loaded || tr_world.numDrawVerts == 0 || tr_world.numShaders == 0 ) {
		return;
	}

	sortedSurfs = malloc( tr_world.numSurfaces * sizeof( int ) );

	tr_world.hasSky = qfalse;

	for ( surfIdx = 0; surfIdx < tr_world.numSurfaces; surfIdx++ ) {
		const bspSurface_t *surf = &tr_world.surfaces[surfIdx];
		shader_t *surfShader = R_GetShaderByHandle( tr_world.shaders[surf->shaderNum].shader );

		if ( surfShader->isSky ) {
			// Sky-shaded surfaces aren't batched/drawn here -- vk_sky.c paints the whole box every frame instead.
			if ( !tr_world.hasSky ) {
				memcpy( tr_world.skyOuterBox, surfShader->skyOuterBox, sizeof( tr_world.skyOuterBox ) );
				tr_world.hasSky = qtrue;
			}
			continue;
		}

		if ( surf->surfaceType == MST_PLANAR || surf->surfaceType == MST_TRIANGLE_SOUP ) {
			totalIndexCount += (uint32_t)surf->numIndexes;
		} else if ( surf->surfaceType == MST_PATCH ) {
			int pv = R_PatchTessVertCount( surf->patchWidth, surf->patchHeight );
			int pi = R_PatchTessIndexCount( surf->patchWidth, surf->patchHeight );
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
			image_t *diffuse = ( shader && shader->numStages >= 1 && shader->stages[0].image ) ?
				shader->stages[0].image : R_GetDefaultImage();
			lightmapTex_t *lm = &s_lightmapTextures[batches[i].lightmapIdx];
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

	// *2: one full pass for real rendering, a second full pass' worth of headroom for a same-frame
	// r_showtris re-expansion (see the wireframe block in vk_world_draw).
	s_dynLMCapacity = writeOffset * 2;
	create_dynamic_vertex_buffer( (VkDeviceSize)s_dynLMCapacity * sizeof( vkWorldVertex_t ),
		&s_dynLMBuffer, &s_dynLMMemory, (void **)&s_dynLMMapped );

	// Headroom for a couple of extra-stage passes across the whole visible world within one frame.
	s_dyn2DCapacity = writeOffset * 2;
	create_dynamic_vertex_buffer( (VkDeviceSize)s_dyn2DCapacity * sizeof( vk2d_vertex_t ),
		&s_dyn2DBuffer, &s_dyn2DMemory, (void **)&s_dyn2DMapped );

	// One full visible-world's worth; dlights are rare/bounded so no extra headroom needed here.
	s_dynDlightCapacity = writeOffset;
	create_dynamic_vertex_buffer( (VkDeviceSize)s_dynDlightCapacity * sizeof( vk2d_vertex_t ),
		&s_dynDlightBuffer, &s_dynDlightMemory, (void **)&s_dynDlightMapped );

	s_batches = batches; // already sized to numDrawable, no need to shrink
	s_numBatches = numBatches;
	s_surfRefs = surfRefs;
	s_built = qtrue;

	ri.Printf( PRINT_ALL, "...vk_world_build: %d verts, %d indexes, %d batches, %d lightmaps\n",
			   (int)nextVertSlot, (int)writeOffset, numBatches, tr_world.numLightmaps );
}

// Expands refs[0..numRefs)'s visible surfaces into s_dynLMMapped with stage's real per-frame
// rgbGen/tcMod, merging contiguous visible writes into one draw call per run.
static void ExpandAndDrawStage0( const worldSurfRef_t *refs, int numRefs, const shaderStage_t *stage0 ) {
	qboolean runOpen = qfalse;
	uint32_t runStart = 0;
	int j;

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
			const vkWorldVertex_t *src = &s_cachedVerts[s_cachedIndices[ref->firstIndex + k]];
			vkWorldVertex_t *dst = &s_dynLMMapped[s_dynLMWriteOffset++];
			byte color[4];
			float uv[2];

			R_CalcStageColorAlpha( stage0, NULL, NULL, src->color, color );
			R_CalcStageTexCoord( stage0, src->uv[0], src->uv[1], src->pos, NULL, uv );

			dst->pos[0] = src->pos[0]; dst->pos[1] = src->pos[1]; dst->pos[2] = src->pos[2];
			dst->color[0] = color[0]; dst->color[1] = color[1]; dst->color[2] = color[2]; dst->color[3] = color[3];
			dst->uv[0] = uv[0]; dst->uv[1] = uv[1];
			dst->lmUv[0] = src->lmUv[0]; dst->lmUv[1] = src->lmUv[1];
		}
	}

	if ( runOpen ) {
		qvkCmdDraw( vk.command_buffer, s_dynLMWriteOffset - runStart, 1, runStart, 0 );
	}
}

// Same as above but for stages 1+: no lightmap UV, writes vk2d_vertex_t into s_dyn2DMapped.
static void ExpandAndDrawExtraStage( const worldSurfRef_t *refs, int numRefs, const shaderStage_t *stage ) {
	qboolean runOpen = qfalse;
	uint32_t runStart = 0;
	int j;

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
			const vkWorldVertex_t *src = &s_cachedVerts[s_cachedIndices[ref->firstIndex + k]];
			vk2d_vertex_t *dst = &s_dyn2DMapped[s_dyn2DWriteOffset++];
			byte color[4];
			float uv[2];

			R_CalcStageColorAlpha( stage, NULL, NULL, src->color, color );
			R_CalcStageTexCoord( stage, src->uv[0], src->uv[1], src->pos, NULL, uv );

			dst->pos[0] = src->pos[0]; dst->pos[1] = src->pos[1]; dst->pos[2] = src->pos[2];
			dst->color[0] = color[0]; dst->color[1] = color[1]; dst->color[2] = color[2]; dst->color[3] = color[3];
			dst->uv[0] = uv[0]; dst->uv[1] = uv[1];
		}
	}

	if ( runOpen ) {
		qvkCmdDraw( vk.command_buffer, s_dyn2DWriteOffset - runStart, 1, runStart, 0 );
	}
}

// Additive dlight pass: diffuse texture modulated by the summed per-vertex dlight contribution,
// blended ONE/ONE on top of the already-drawn lit surface. World surfaces got zero dlight effect
// before this; this is a per-vertex distance falloff (same shape as vk_entity.c's ComputeEntityTint),
// not true projected-texture dlights like vkQuake3's ProjectDlightTexture -- a scoped simplification,
// not a faked feature: it's real per-vertex light falloff, just not a radial-blob projection.
static void ExpandAndDrawWorldDlights( const worldSurfRef_t *refs, int numRefs, const dlight_t *dlights, int numDlights ) {
	qboolean runOpen = qfalse;
	uint32_t runStart = 0;
	int j;

	for ( j = 0; j < numRefs; j++ ) {
		const worldSurfRef_t *ref = &refs[j];
		uint32_t k;

		if ( !R_SurfaceIsVisible( ref->surfaceIndex ) ) {
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
			const vkWorldVertex_t *src = &s_cachedVerts[s_cachedIndices[ref->firstIndex + k]];
			vk2d_vertex_t *dst = &s_dynDlightMapped[s_dynDlightWriteOffset++];
			vec3_t sum = { 0.0f, 0.0f, 0.0f };
			int lightIdx, r, g, b;

			for ( lightIdx = 0; lightIdx < numDlights; lightIdx++ ) {
				vec3_t dir;
				float dist, power, d;

				VectorSubtract( dlights[lightIdx].origin, src->pos, dir );
				dist = VectorNormalize( dir );
				power = DLIGHT_AT_RADIUS * ( dlights[lightIdx].radius * dlights[lightIdx].radius );
				if ( dist < DLIGHT_MINIMUM_RADIUS ) {
					dist = DLIGHT_MINIMUM_RADIUS;
				}
				d = power / ( dist * dist );
				VectorMA( sum, d, dlights[lightIdx].color, sum );
			}

			r = (int)sum[0]; g = (int)sum[1]; b = (int)sum[2];
			dst->pos[0] = src->pos[0]; dst->pos[1] = src->pos[1]; dst->pos[2] = src->pos[2];
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

void vk_world_draw( const float mvp[16], const float fogPush[12] ) {
	float push_data[32];
	VkDeviceSize vb_offset = 0;
	int i;
	VkPipeline boundPipeline = VK_NULL_HANDLE;
	qboolean lmBufferBound = qfalse, bound2DBuffer = qfalse;
	static qboolean firstDraw = qtrue;

	if ( !s_built ) {
		return;
	}

	if ( firstDraw ) {
		ri.Printf( PRINT_ALL, "...vk_world_draw checkpoint: first draw starting, %d batches\n", s_numBatches );
	}

	s_dynLMWriteOffset = 0;
	s_dyn2DWriteOffset = 0;

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

	for ( i = 0; i < s_numBatches; i++ ) {
		const worldBatch_t *batch = &s_batches[i];
		shader_t *shader = R_GetShaderByHandle( batch->shader );
		VkPipeline pipeline;
		int s;

		if ( batch->descriptor == VK_NULL_HANDLE ) {
			continue;
		}

		// Batches are already split per-shader (worldBatch_t is keyed by shader+lightmap), so this is free to vary.
		pipeline = vk_getWorldLightmapPipeline( shader->cullType );
		if ( pipeline != boundPipeline ) {
			qvkCmdBindPipeline( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline );
			boundPipeline = pipeline;
		}
		if ( !lmBufferBound ) {
			qvkCmdBindVertexBuffers( vk.command_buffer, 0, 1, &s_dynLMBuffer, &vb_offset );
			lmBufferBound = qtrue; bound2DBuffer = qfalse;
		}

		qvkCmdBindDescriptorSets( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.lightmap_pipeline_layout,
			0, 1, &batch->descriptor, 0, NULL );

		ExpandAndDrawStage0( &s_surfRefs[batch->firstSurfRef], batch->numSurfRefs, &shader->stages[0] );

		// Stages 1+ get their own real per-frame rgbGen/tcMod now, sharing no state with stage 0.
		// vk.pipeline_layout and vk.lightmap_pipeline_layout declare identical push constant ranges,
		// so the mvp/fog push above still applies -- no need to push it again here.
		for ( s = 1; s < shader->numStages; s++ ) {
			const shaderStage_t *stage = &shader->stages[s];
			image_t *stageImage = R_GetStageImage( stage );

			if ( !stageImage || stageImage->vk_descriptor == VK_NULL_HANDLE ) {
				continue;
			}

			pipeline = vk_getPipeline3D( sizeof( vk2d_vertex_t ), stage->blendEnabled, stage->srcBlend, stage->dstBlend,
				vk_cullModeForShader( shader->cullType ), stage->alphaTestFunc, R_InPortalView(), R_InMirrorView(), shader->polygonOffset );
			if ( pipeline != boundPipeline ) {
				qvkCmdBindPipeline( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline );
				boundPipeline = pipeline;
			}
			if ( !bound2DBuffer ) {
				qvkCmdBindVertexBuffers( vk.command_buffer, 0, 1, &s_dyn2DBuffer, &vb_offset );
				bound2DBuffer = qtrue; lmBufferBound = qfalse;
			}

			qvkCmdBindDescriptorSets( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout,
				0, 1, &stageImage->vk_descriptor, 0, NULL );

			ExpandAndDrawExtraStage( &s_surfRefs[batch->firstSurfRef], batch->numSurfRefs, stage );
		}
	}

	s_dynDlightWriteOffset = 0;
	{
		int numDlights;
		const dlight_t *dlights = R_GetSceneDlights( &numDlights );

		if ( numDlights > 0 ) {
			qvkCmdBindVertexBuffers( vk.command_buffer, 0, 1, &s_dynDlightBuffer, &vb_offset );
			lmBufferBound = qfalse; bound2DBuffer = qfalse;
			boundPipeline = VK_NULL_HANDLE;

			for ( i = 0; i < s_numBatches; i++ ) {
				const worldBatch_t *batch = &s_batches[i];
				shader_t *shader;
				image_t *diffuse;
				VkPipeline pipeline;

				if ( batch->descriptor == VK_NULL_HANDLE ) {
					continue;
				}
				shader = R_GetShaderByHandle( batch->shader );
				diffuse = ( shader->numStages >= 1 && shader->stages[0].image ) ? shader->stages[0].image : R_GetDefaultImage();
				if ( diffuse->vk_descriptor == VK_NULL_HANDLE ) {
					continue;
				}

				pipeline = vk_getPipeline3D( sizeof( vk2d_vertex_t ), VK_TRUE, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE,
					vk_cullModeForShader( shader->cullType ), 0, R_InPortalView(), R_InMirrorView(), VK_FALSE );
				if ( pipeline != boundPipeline ) {
					qvkCmdBindPipeline( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline );
					boundPipeline = pipeline;
				}

				qvkCmdBindDescriptorSets( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout,
					0, 1, &diffuse->vk_descriptor, 0, NULL );

				ExpandAndDrawWorldDlights( &s_surfRefs[batch->firstSurfRef], batch->numSurfRefs, dlights, numDlights );
			}
		}
	}

	if ( !r_showtris ) {
		r_showtris = ri.Cvar_Get( "r_showtris", "0", CVAR_CHEAT );
	}
	if ( r_showtris->integer && vk.pipeline_world_wireframe != VK_NULL_HANDLE ) {
		qvkCmdBindPipeline( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_world_wireframe );
		if ( !lmBufferBound ) {
			qvkCmdBindVertexBuffers( vk.command_buffer, 0, 1, &s_dynLMBuffer, &vb_offset );
			lmBufferBound = qtrue; bound2DBuffer = qfalse;
		}
		for ( i = 0; i < s_numBatches; i++ ) {
			const worldBatch_t *batch = &s_batches[i];
			shader_t *shader;

			if ( batch->descriptor == VK_NULL_HANDLE ) {
				continue;
			}
			shader = R_GetShaderByHandle( batch->shader );
			qvkCmdBindDescriptorSets( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.lightmap_pipeline_layout,
				0, 1, &batch->descriptor, 0, NULL );
			ExpandAndDrawStage0( &s_surfRefs[batch->firstSurfRef], batch->numSurfRefs, &shader->stages[0] );
		}
	}

	if ( firstDraw ) {
		ri.Printf( PRINT_ALL, "...vk_world_draw checkpoint: first draw recorded ok\n" );
		firstDraw = qfalse;
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
