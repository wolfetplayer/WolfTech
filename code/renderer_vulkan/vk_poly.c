// vk_poly.c -- see vk_poly.h. Fan-triangulated on the CPU, drawn once per shader stage.

#include <string.h>

#include "VKimpl.h"
#include "vk_instance.h"
#include "vk_pipelines.h"
#include "vk_frame.h"
#include "tr_shader.h"
#include "tr_shade_calc.h"
#include "tr_scene.h"
#include "vk_poly.h"

// A poly with numVerts fans into (numVerts - 2) triangles; this is the worst case summed over every scene poly.
#define POLY_SCRATCH_VERTS ( 3 * MAX_SCENE_POLY_VERTS )

// Per-poly scratch cap before fan-triangulating; real scene polys are always well under this.
#define MAX_POLY_SOURCE_VERTS 256

static VkBuffer s_vertexBuffer;
static VkDeviceMemory s_vertexMemory;
static vk2d_vertex_t *s_mapped;

static VkBuffer s_mtVertexBuffer;
static VkDeviceMemory s_mtVertexMemory;
static vkMultiTexVertex_t *s_mtMapped;

// Shared write cursors into s_mapped/s_mtMapped; persist across every vk_poly_draw call this frame, reset only in vk_poly_begin_frame (see vk_entity.c's s_vertWrite).
static uint32_t s_writeOffset;
static uint32_t s_mtWriteOffset;

// This frame's region within each (now VK_FRAMES_IN_FLIGHT-sized) buffer; s_writeOffset/s_mtWriteOffset stay region-relative, added in at bind/write time.
static uint32_t s_regionBase;
static uint32_t s_mtRegionBase;

void vk_poly_begin_frame( void ) {
	s_writeOffset = 0;
	s_mtWriteOffset = 0;
	s_regionBase = vk_current_frame_slot() * (uint32_t)POLY_SCRATCH_VERTS;
	s_mtRegionBase = vk_current_frame_slot() * (uint32_t)POLY_SCRATCH_VERTS;
}

extern uint32_t vk_find_memory_type( uint32_t type_bits, VkMemoryPropertyFlags properties );

static void create_mapped_buffer( VkDeviceSize size, VkBuffer *outBuffer, VkDeviceMemory *outMemory, void **outMapped ) {
	VkBufferCreateInfo buf_desc;
	VkMemoryRequirements mem_reqs;
	VkMemoryAllocateInfo alloc_info;

	buf_desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buf_desc.pNext = NULL;
	buf_desc.flags = 0;
	buf_desc.size = size;
	buf_desc.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	buf_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	buf_desc.queueFamilyIndexCount = 0;
	buf_desc.pQueueFamilyIndices = NULL;
	VK_CHECK( qvkCreateBuffer( vk.device, &buf_desc, NULL, outBuffer ) );

	qvkGetBufferMemoryRequirements( vk.device, *outBuffer, &mem_reqs );

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = mem_reqs.size;
	alloc_info.memoryTypeIndex = vk_find_memory_type( mem_reqs.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, outMemory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, *outBuffer, *outMemory, 0 ) );

	VK_CHECK( qvkMapMemory( vk.device, *outMemory, 0, VK_WHOLE_SIZE, 0, outMapped ) );
}

static void destroy_mapped_buffer( VkBuffer buffer, VkDeviceMemory memory, void *mapped ) {
	if ( mapped ) {
		qvkUnmapMemory( vk.device, memory );
	}
	qvkDestroyBuffer( vk.device, buffer, NULL );
	qvkFreeMemory( vk.device, memory, NULL );
}

void vk_poly_init( void ) {
	create_mapped_buffer( (VkDeviceSize)POLY_SCRATCH_VERTS * VK_FRAMES_IN_FLIGHT * sizeof( vk2d_vertex_t ), &s_vertexBuffer, &s_vertexMemory, (void **)&s_mapped );
	create_mapped_buffer( (VkDeviceSize)POLY_SCRATCH_VERTS * VK_FRAMES_IN_FLIGHT * sizeof( vkMultiTexVertex_t ), &s_mtVertexBuffer, &s_mtVertexMemory, (void **)&s_mtMapped );
}

void vk_poly_shutdown( void ) {
	destroy_mapped_buffer( s_vertexBuffer, s_vertexMemory, s_mapped );
	s_mapped = NULL;
	destroy_mapped_buffer( s_mtVertexBuffer, s_mtVertexMemory, s_mtMapped );
	s_mtMapped = NULL;
}

static void write_scratch_vert( vk2d_vertex_t *out, const float pos[3], const byte color[4], const float uv[2] ) {
	out->pos[0] = pos[0]; out->pos[1] = pos[1]; out->pos[2] = pos[2];
	out->color[0] = color[0]; out->color[1] = color[1]; out->color[2] = color[2]; out->color[3] = color[3];
	out->uv[0] = uv[0]; out->uv[1] = uv[1];
}

static void write_scratch_vert_mt( vkMultiTexVertex_t *out, const float pos[3], const byte color[4], const float uv0[2], const float uv1[2] ) {
	out->pos[0] = pos[0]; out->pos[1] = pos[1]; out->pos[2] = pos[2];
	out->color[0] = color[0]; out->color[1] = color[1]; out->color[2] = color[2]; out->color[3] = color[3];
	out->uv0[0] = uv0[0]; out->uv0[1] = uv0[1];
	out->uv1[0] = uv1[0]; out->uv1[1] = uv1[1];
}

void vk_poly_draw( const float mvp[16], const float fogPush[12] ) {
	float push_data[32];
	VkDeviceSize vb_offset = (VkDeviceSize)s_regionBase * sizeof( vk2d_vertex_t );
	VkDeviceSize mtVbOffset = (VkDeviceSize)s_mtRegionBase * sizeof( vkMultiTexVertex_t );
	int numPolys, i;
	const scenePoly_t *polys = R_GetScenePolys( &numPolys );
	const polyVert_t *polyVerts = R_GetScenePolyVerts();
	uint32_t writeOffset = s_writeOffset;
	uint32_t mtWriteOffset = s_mtWriteOffset;
	VkPipeline boundPipeline = VK_NULL_HANDLE;
	VkBuffer boundVertexBuffer = s_vertexBuffer;

	if ( numPolys <= 0 || !s_mapped ) {
		return;
	}

	memset( push_data, 0, sizeof( push_data ) );
	memcpy( push_data, mvp, 16 * sizeof( float ) );
	memcpy( push_data + 16, fogPush, 12 * sizeof( float ) );
	qvkCmdPushConstants( vk.command_buffer, vk.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		0, sizeof( push_data ), push_data );
	qvkCmdBindVertexBuffers( vk.command_buffer, 0, 1, &s_vertexBuffer, &vb_offset );

	for ( i = 0; i < numPolys; i++ ) {
		const scenePoly_t *poly = &polys[i];
		shader_t *shader = R_GetShaderByHandle( poly->shader );
		float deformedPos[MAX_POLY_SOURCE_VERTS][3];
		int v, s, combineMode;

		if ( poly->numVerts < 3 || poly->numVerts > MAX_POLY_SOURCE_VERTS ) {
			continue;
		}

		// deformVertexes is shader-level: computed once, shared by every stage below.
		for ( v = 0; v < poly->numVerts; v++ ) {
			R_DeformPosition( shader, polyVerts[poly->firstVert + v].xyz, NULL, 0.0f, deformedPos[v] ); // scene polys carry no per-vertex normal -- wave/bulge pass through, same as before
		}

		if ( vk_canCollapseMultiTex( shader, &combineMode ) ) {
			const shaderStage_t *stage0 = &shader->stages[0];
			const shaderStage_t *stage1 = &shader->stages[1];
			image_t *image0 = R_GetStageImage( stage0 );
			image_t *image1 = R_GetStageImage( stage1 );
			byte stageColor[MAX_POLY_SOURCE_VERTS][4];
			float uv0[MAX_POLY_SOURCE_VERTS][2], uv1[MAX_POLY_SOURCE_VERTS][2];
			uint32_t firstVert = mtWriteOffset;
			uint32_t vertCount;
			VkDescriptorSet descSet;
			VkPipeline pipeline;

			if ( !image0 || !image1 || mtWriteOffset + (uint32_t)( poly->numVerts - 2 ) * 3 > POLY_SCRATCH_VERTS ) {
				continue;
			}

			for ( v = 0; v < poly->numVerts; v++ ) {
				const polyVert_t *srcVert = &polyVerts[poly->firstVert + v];
				R_CalcStageColorAlpha( stage0, NULL, srcVert->modulate, stageColor[v] );
				R_CalcStageTexCoord( stage0, srcVert->st[0], srcVert->st[1], deformedPos[v], NULL, uv0[v] );
				R_CalcStageTexCoord( stage1, srcVert->st[0], srcVert->st[1], deformedPos[v], NULL, uv1[v] );
			}

			for ( v = 0; v < poly->numVerts - 2; v++ ) {
				write_scratch_vert_mt( &s_mtMapped[s_mtRegionBase + mtWriteOffset + 0], deformedPos[0], stageColor[0], uv0[0], uv1[0] );
				write_scratch_vert_mt( &s_mtMapped[s_mtRegionBase + mtWriteOffset + 1], deformedPos[v + 1], stageColor[v + 1], uv0[v + 1], uv1[v + 1] );
				write_scratch_vert_mt( &s_mtMapped[s_mtRegionBase + mtWriteOffset + 2], deformedPos[v + 2], stageColor[v + 2], uv0[v + 2], uv1[v + 2] );
				mtWriteOffset += 3;
			}
			vertCount = mtWriteOffset - firstVert;

			if ( boundVertexBuffer != s_mtVertexBuffer ) {
				qvkCmdBindVertexBuffers( vk.command_buffer, 0, 1, &s_mtVertexBuffer, &mtVbOffset );
				boundVertexBuffer = s_mtVertexBuffer;
			}

			pipeline = vk_getPipelineMultiTexture( combineMode, vk_cullModeForShader( shader->cullType ), stage0->alphaTestFunc );
			if ( pipeline != boundPipeline ) {
				qvkCmdBindPipeline( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline );
				boundPipeline = pipeline;
			}
			descSet = vk_getMultiTextureDescriptor( image0, image1 );
			qvkCmdBindDescriptorSets( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.lightmap_pipeline_layout,
				0, 1, &descSet, 0, NULL );
			qvkCmdDraw( vk.command_buffer, vertCount, 1, firstVert, 0 );
			continue;
		}

		// rgbGen/alphaGen/tcMod are per-stage, so each stage gets its own fresh copy of the triangle fan.
		for ( s = 0; s < shader->numStages; s++ ) {
			const shaderStage_t *stage = &shader->stages[s];
			image_t *stageImage = R_GetStageImage( stage );
			byte stageColor[MAX_POLY_SOURCE_VERTS][4];
			float stageUV[MAX_POLY_SOURCE_VERTS][2];
			uint32_t firstVert = writeOffset;
			uint32_t vertCount;
			VkPipeline pipeline;

			if ( !stageImage || stageImage->vk_descriptor == VK_NULL_HANDLE ||
				 writeOffset + (uint32_t)( poly->numVerts - 2 ) * 3 > POLY_SCRATCH_VERTS ) {
				continue;
			}

			for ( v = 0; v < poly->numVerts; v++ ) {
				const polyVert_t *srcVert = &polyVerts[poly->firstVert + v];
				R_CalcStageColorAlpha( stage, NULL /* no entity context for scene polys */, srcVert->modulate, stageColor[v] );
				R_CalcStageTexCoord( stage, srcVert->st[0], srcVert->st[1], deformedPos[v], NULL, stageUV[v] );
			}

			for ( v = 0; v < poly->numVerts - 2; v++ ) {
				write_scratch_vert( &s_mapped[s_regionBase + writeOffset + 0], deformedPos[0], stageColor[0], stageUV[0] );
				write_scratch_vert( &s_mapped[s_regionBase + writeOffset + 1], deformedPos[v + 1], stageColor[v + 1], stageUV[v + 1] );
				write_scratch_vert( &s_mapped[s_regionBase + writeOffset + 2], deformedPos[v + 2], stageColor[v + 2], stageUV[v + 2] );
				writeOffset += 3;
			}
			vertCount = writeOffset - firstVert;

			if ( boundVertexBuffer != s_vertexBuffer ) {
				qvkCmdBindVertexBuffers( vk.command_buffer, 0, 1, &s_vertexBuffer, &vb_offset );
				boundVertexBuffer = s_vertexBuffer;
			}

			pipeline = vk_getPipeline3D( sizeof( vk2d_vertex_t ), stage->blendEnabled, stage->srcBlend, stage->dstBlend,
				vk_cullModeForShader( shader->cullType ), stage->alphaTestFunc, VK_FALSE, VK_FALSE, shader->polygonOffset );
			if ( pipeline != boundPipeline ) {
				qvkCmdBindPipeline( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline );
				boundPipeline = pipeline;
			}
			qvkCmdBindDescriptorSets( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout,
				0, 1, &stageImage->vk_descriptor, 0, NULL );
			qvkCmdDraw( vk.command_buffer, vertCount, 1, firstVert, 0 );
		}
	}

	// Persist back for the next vk_poly_draw call this frame (see s_writeOffset comment above).
	s_writeOffset = writeOffset;
	s_mtWriteOffset = mtWriteOffset;
}
