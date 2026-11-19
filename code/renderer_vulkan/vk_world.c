// vk_world.c -- uploads tr_world into GPU buffers and draws it, culled per surface by tr_world.c.

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
#include "vk_world.h"

// One entry per drawable surface; consecutive entries in a batch are contiguous in the index buffer.
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

static VkBuffer s_vertexBuffer;
static VkDeviceMemory s_vertexMemory;
static VkBuffer s_indexBuffer;
static VkDeviceMemory s_indexMemory;

static worldBatch_t *s_batches;
static int s_numBatches;
static worldSurfRef_t *s_surfRefs;

static lightmapTex_t *s_lightmapTextures;
static int s_numLightmapTextures; // tr_world.numLightmaps real ones, plus one synthetic white fallback
static VkDescriptorPool s_lmDescriptorPool;

static qboolean s_built = qfalse;

static cvar_t *r_showtris;

void vk_world_clear( void ) {
	int i;

	if ( s_vertexBuffer ) { qvkDestroyBuffer( vk.device, s_vertexBuffer, NULL ); s_vertexBuffer = VK_NULL_HANDLE; }
	if ( s_vertexMemory ) { qvkFreeMemory( vk.device, s_vertexMemory, NULL ); s_vertexMemory = VK_NULL_HANDLE; }
	if ( s_indexBuffer ) { qvkDestroyBuffer( vk.device, s_indexBuffer, NULL ); s_indexBuffer = VK_NULL_HANDLE; }
	if ( s_indexMemory ) { qvkFreeMemory( vk.device, s_indexMemory, NULL ); s_indexMemory = VK_NULL_HANDLE; }

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
	s_built = qfalse;
}

void vk_world_shutdown( void ) {
	vk_world_clear();
}

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

// Bakes stage 0's rgbGen/alphaGen once at load time; world vertex data is static, so this can't rerun per frame.
static void ApplyBuildTimeColorGen( const shaderStage_t *stage, vkWorldVertex_t *verts, int firstVert, int numVerts ) {
	int i;

	for ( i = 0; i < numVerts; i++ ) {
		vkWorldVertex_t *v = &verts[firstVert + i];
		byte outColor[4];

		R_CalcStageColorAlpha( stage, NULL, NULL, v->color, outColor );
		v->color[0] = outColor[0]; v->color[1] = outColor[1]; v->color[2] = outColor[2]; v->color[3] = outColor[3];
	}
}

// RGB8 -> RGBA8 (opaque) so it can go through the same texture upload path as everything else.
static void BuildLightmapTexture( const byte *rgb, lightmapTex_t *out ) {
	byte *rgba = malloc( LIGHTMAP_WIDTH * LIGHTMAP_HEIGHT * 4 );
	int i;

	for ( i = 0; i < LIGHTMAP_WIDTH * LIGHTMAP_HEIGHT; i++ ) {
		rgba[i * 4 + 0] = rgb[i * 3 + 0];
		rgba[i * 4 + 1] = rgb[i * 3 + 1];
		rgba[i * 4 + 2] = rgb[i * 3 + 2];
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

			{
				uint32_t vertBaseBefore = nextVertSlot;

				EmitSurfaceGeometry( surf, verts, &nextVertSlot, indexes, &writeOffset, &firstIndex, &indexCount );

				{
					shader_t *surfShader = R_GetShaderByHandle( tr_world.shaders[surf->shaderNum].shader );
					qboolean hasRealLightmap = ( lightmapIdx != whiteLightmapIdx );
					const shaderStage_t *stage0 = &surfShader->stages[0];

					// No real lightmap: skip the bake on a default rgbGen so baked vertex lighting survives.
					if ( surfShader->numStages >= 1 &&
						 ( hasRealLightmap || !( stage0->rgbGen == CGEN_IDENTITY_LIGHTING || stage0->rgbGen == CGEN_IDENTITY ) ) ) {
						int bakedFirstVert = ( surf->surfaceType == MST_PATCH ) ? (int)vertBaseBefore : surf->firstVert;
						int bakedNumVerts = ( surf->surfaceType == MST_PATCH ) ? (int)( nextVertSlot - vertBaseBefore ) : surf->numVerts;
						ApplyBuildTimeColorGen( stage0, verts, bakedFirstVert, bakedNumVerts );
					}
				}
			}

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

	ri.Printf( PRINT_ALL, "...vk_world_build checkpoint: about to upload %d verts\n", (int)nextVertSlot );
	vk_create_device_local_buffer( verts, (VkDeviceSize)nextVertSlot * sizeof( vkWorldVertex_t ),
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &s_vertexBuffer, &s_vertexMemory );
	ri.Printf( PRINT_ALL, "...vk_world_build checkpoint: about to upload %d indexes\n", (int)writeOffset );
	vk_create_device_local_buffer( indexes, (VkDeviceSize)writeOffset * sizeof( uint32_t ),
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT, &s_indexBuffer, &s_indexMemory );
	ri.Printf( PRINT_ALL, "...vk_world_build checkpoint: GPU buffers uploaded\n" );

	s_batches = batches; // already sized to numDrawable, no need to shrink
	s_numBatches = numBatches;
	s_surfRefs = surfRefs;
	s_built = qtrue;

	free( verts );
	free( indexes );

	ri.Printf( PRINT_ALL, "...vk_world_build: %d verts, %d indexes, %d batches, %d lightmaps\n",
			   (int)nextVertSlot, (int)writeOffset, numBatches, tr_world.numLightmaps );
}

// Draws refs[0..numRefs), merging contiguous visible ones into a single draw call per run.
static void DrawSurfRefRun( const worldSurfRef_t *refs, int numRefs ) {
	qboolean runOpen = qfalse;
	uint32_t runFirst = 0, runCount = 0;
	int j;

	for ( j = 0; j < numRefs; j++ ) {
		const worldSurfRef_t *ref = &refs[j];

		if ( R_SurfaceIsVisible( ref->surfaceIndex ) ) {
			if ( runOpen && ref->firstIndex == runFirst + runCount ) {
				runCount += ref->indexCount;
			} else {
				if ( runOpen ) {
					qvkCmdDrawIndexed( vk.command_buffer, runCount, 1, runFirst, 0, 0 );
				}
				runFirst = ref->firstIndex;
				runCount = ref->indexCount;
				runOpen = qtrue;
			}
		} else if ( runOpen ) {
			qvkCmdDrawIndexed( vk.command_buffer, runCount, 1, runFirst, 0, 0 );
			runOpen = qfalse;
		}
	}
	if ( runOpen ) {
		qvkCmdDrawIndexed( vk.command_buffer, runCount, 1, runFirst, 0, 0 );
	}
}

void vk_world_draw( const float mvp[16], const float fogPush[12] ) {
	float push_data[32];
	VkDeviceSize vb_offset = 0;
	int i;
	VkPipeline boundPipeline = VK_NULL_HANDLE;
	static qboolean firstDraw = qtrue;

	if ( !s_built ) {
		return;
	}

	if ( firstDraw ) {
		ri.Printf( PRINT_ALL, "...vk_world_draw checkpoint: first draw starting, %d batches\n", s_numBatches );
	}

	memset( push_data, 0, sizeof( push_data ) );
	memcpy( push_data, mvp, 16 * sizeof( float ) );
	memcpy( push_data + 16, fogPush, 12 * sizeof( float ) );
	qvkCmdPushConstants( vk.command_buffer, vk.lightmap_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		0, sizeof( push_data ), push_data );

	qvkCmdBindVertexBuffers( vk.command_buffer, 0, 1, &s_vertexBuffer, &vb_offset );
	qvkCmdBindIndexBuffer( vk.command_buffer, s_indexBuffer, 0, VK_INDEX_TYPE_UINT32 );

	for ( i = 0; i < s_numBatches; i++ ) {
		const worldBatch_t *batch = &s_batches[i];
		shader_t *shader = R_GetShaderByHandle( batch->shader );
		VkPipeline pipeline;
		int s;

		if ( batch->descriptor == VK_NULL_HANDLE ) {
			continue;
		}

		// Batches are already split per-shader (worldBatch_t is keyed by shader+lightmap), so this is free to vary.
		pipeline = vk.pipeline_3d_lightmap[shader->cullType];
		if ( pipeline != boundPipeline ) {
			qvkCmdBindPipeline( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline );
			boundPipeline = pipeline;
		}

		qvkCmdBindDescriptorSets( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.lightmap_pipeline_layout,
			0, 1, &batch->descriptor, 0, NULL );

		DrawSurfRefRun( &s_surfRefs[batch->firstSurfRef], batch->numSurfRefs );

		// Stages 1+ redraw the same indices with that stage's own texture/blend state, sharing stage 0's
		// baked vertex color/UV data. vk.pipeline_layout and vk.lightmap_pipeline_layout declare identical
		// push constant ranges, so the mvp/fog push above still applies -- no need to push it again here.
		for ( s = 1; s < shader->numStages; s++ ) {
			const shaderStage_t *stage = &shader->stages[s];
			image_t *stageImage = R_GetStageImage( stage );

			if ( !stageImage || stageImage->vk_descriptor == VK_NULL_HANDLE ) {
				continue;
			}

			pipeline = vk_getPipeline3D( sizeof( vkWorldVertex_t ), stage->blendEnabled, stage->srcBlend, stage->dstBlend,
				vk_cullModeForShader( shader->cullType ), stage->alphaTestFunc );
			if ( pipeline != boundPipeline ) {
				qvkCmdBindPipeline( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline );
				boundPipeline = pipeline;
			}

			qvkCmdBindDescriptorSets( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout,
				0, 1, &stageImage->vk_descriptor, 0, NULL );

			DrawSurfRefRun( &s_surfRefs[batch->firstSurfRef], batch->numSurfRefs );
		}
	}

	if ( !r_showtris ) {
		r_showtris = ri.Cvar_Get( "r_showtris", "0", CVAR_CHEAT );
	}
	if ( r_showtris->integer && vk.pipeline_world_wireframe != VK_NULL_HANDLE ) {
		qvkCmdBindPipeline( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_world_wireframe );
		for ( i = 0; i < s_numBatches; i++ ) {
			const worldBatch_t *batch = &s_batches[i];

			if ( batch->descriptor == VK_NULL_HANDLE ) {
				continue;
			}
			qvkCmdBindDescriptorSets( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.lightmap_pipeline_layout,
				0, 1, &batch->descriptor, 0, NULL );
			DrawSurfRefRun( &s_surfRefs[batch->firstSurfRef], batch->numSurfRefs );
		}
	}

	if ( firstDraw ) {
		ri.Printf( PRINT_ALL, "...vk_world_draw checkpoint: first draw recorded ok\n" );
		firstDraw = qfalse;
	}
}
