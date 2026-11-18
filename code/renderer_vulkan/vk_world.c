// vk_world.c -- uploads tr_world into GPU buffers and draws it, culled per surface by tr_world.c.

#include <stdlib.h>
#include <string.h>

#include "VKimpl.h"
#include "vk_instance.h"
#include "vk_pipelines.h"
#include "vk_image.h"
#include "tr_local.h"
#include "tr_shader.h"
#include "tr_bsp.h"
#include "tr_world.h"
#include "vk_world.h"

// One entry per drawable surface; consecutive entries in a batch are contiguous in the index buffer.
typedef struct {
	int surfaceIndex;
	uint32_t firstIndex;
	uint32_t indexCount;
} worldSurfRef_t;

typedef struct {
	qhandle_t shader;
	int firstSurfRef;
	int numSurfRefs;
} worldDrawBatch_t;

static VkBuffer s_vertexBuffer;
static VkDeviceMemory s_vertexMemory;
static VkBuffer s_indexBuffer;
static VkDeviceMemory s_indexMemory;

static worldDrawBatch_t *s_batches;
static int s_numBatches;
static worldSurfRef_t *s_surfRefs;
static qboolean s_built = qfalse;

void vk_world_clear( void ) {
	if ( s_vertexBuffer ) { qvkDestroyBuffer( vk.device, s_vertexBuffer, NULL ); s_vertexBuffer = VK_NULL_HANDLE; }
	if ( s_vertexMemory ) { qvkFreeMemory( vk.device, s_vertexMemory, NULL ); s_vertexMemory = VK_NULL_HANDLE; }
	if ( s_indexBuffer ) { qvkDestroyBuffer( vk.device, s_indexBuffer, NULL ); s_indexBuffer = VK_NULL_HANDLE; }
	if ( s_indexMemory ) { qvkFreeMemory( vk.device, s_indexMemory, NULL ); s_indexMemory = VK_NULL_HANDLE; }
	if ( s_batches ) { free( s_batches ); s_batches = NULL; }
	if ( s_surfRefs ) { free( s_surfRefs ); s_surfRefs = NULL; }
	s_numBatches = 0;
	s_built = qfalse;
}

void vk_world_shutdown( void ) {
	vk_world_clear();
}

void vk_world_build( void ) {
	vk2d_vertex_t *verts;
	uint32_t *indexes;
	worldDrawBatch_t *batches;
	worldSurfRef_t *surfRefs;
	uint32_t writeOffset;
	int numBatches, numSurfRefs;
	int shaderIdx, surfIdx, i;

	vk_world_clear();

	if ( !tr_world.loaded || tr_world.numDrawVerts == 0 || tr_world.numShaders == 0 ) {
		return;
	}

	verts = malloc( tr_world.numDrawVerts * sizeof( vk2d_vertex_t ) );
	for ( i = 0; i < tr_world.numDrawVerts; i++ ) {
		const drawVert_t *dv = &tr_world.drawVerts[i];
		verts[i].pos[0] = dv->xyz[0];
		verts[i].pos[1] = dv->xyz[1];
		verts[i].pos[2] = dv->xyz[2];
		verts[i].color[0] = dv->color[0];
		verts[i].color[1] = dv->color[1];
		verts[i].color[2] = dv->color[2];
		verts[i].color[3] = dv->color[3];
		verts[i].uv[0] = dv->st[0];
		verts[i].uv[1] = dv->st[1];
	}

	// Surfaces' index ranges can overlap, so numIndexes can sum past tr_world.numDrawIndexes -- count first.
	{
		uint32_t totalIndexCount = 0;
		int numDrawable = 0;
		for ( surfIdx = 0; surfIdx < tr_world.numSurfaces; surfIdx++ ) {
			const bspSurface_t *surf = &tr_world.surfaces[surfIdx];
			if ( surf->surfaceType == MST_PLANAR || surf->surfaceType == MST_TRIANGLE_SOUP ) {
				totalIndexCount += (uint32_t)surf->numIndexes;
				numDrawable++;
			}
		}
		indexes = malloc( totalIndexCount * sizeof( uint32_t ) );
		surfRefs = malloc( numDrawable * sizeof( worldSurfRef_t ) );
	}
	batches = malloc( tr_world.numShaders * sizeof( worldDrawBatch_t ) );
	numBatches = 0;
	numSurfRefs = 0;
	writeOffset = 0;

	for ( shaderIdx = 0; shaderIdx < tr_world.numShaders; shaderIdx++ ) {
		uint32_t batchStart = writeOffset;
		int batchFirstRef = numSurfRefs;

		for ( surfIdx = 0; surfIdx < tr_world.numSurfaces; surfIdx++ ) {
			const bspSurface_t *surf = &tr_world.surfaces[surfIdx];
			uint32_t surfFirstIndex;

			if ( surf->shaderNum != shaderIdx ) {
				continue;
			}
			if ( surf->surfaceType != MST_PLANAR && surf->surfaceType != MST_TRIANGLE_SOUP ) {
				continue; // patches/flares/foliage: not tessellated/drawable yet
			}

			surfFirstIndex = writeOffset;
			for ( i = 0; i < surf->numIndexes; i++ ) {
				// BSP indices are local to the surface's own vertex range; remap to the global vertex buffer.
				indexes[writeOffset++] = (uint32_t)( surf->firstVert + tr_world.drawIndexes[surf->firstIndex + i] );
			}

			surfRefs[numSurfRefs].surfaceIndex = surfIdx;
			surfRefs[numSurfRefs].firstIndex = surfFirstIndex;
			surfRefs[numSurfRefs].indexCount = writeOffset - surfFirstIndex;
			numSurfRefs++;
		}

		if ( writeOffset > batchStart ) {
			batches[numBatches].shader = tr_world.shaders[shaderIdx].shader;
			batches[numBatches].firstSurfRef = batchFirstRef;
			batches[numBatches].numSurfRefs = numSurfRefs - batchFirstRef;
			numBatches++;
		}
	}

	if ( writeOffset == 0 ) {
		free( verts );
		free( indexes );
		free( batches );
		free( surfRefs );
		return;
	}

	ri.Printf( PRINT_ALL, "...vk_world_build checkpoint: about to upload %d verts\n", tr_world.numDrawVerts );
	vk_create_device_local_buffer( verts, (VkDeviceSize)tr_world.numDrawVerts * sizeof( vk2d_vertex_t ),
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &s_vertexBuffer, &s_vertexMemory );
	ri.Printf( PRINT_ALL, "...vk_world_build checkpoint: about to upload %d indexes\n", writeOffset );
	vk_create_device_local_buffer( indexes, (VkDeviceSize)writeOffset * sizeof( uint32_t ),
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT, &s_indexBuffer, &s_indexMemory );
	ri.Printf( PRINT_ALL, "...vk_world_build checkpoint: GPU buffers uploaded\n" );

	s_batches = batches; // already sized to numShaders, no need to shrink
	s_numBatches = numBatches;
	s_surfRefs = surfRefs;
	s_built = qtrue;

	free( verts );
	free( indexes );

	ri.Printf( PRINT_ALL, "...vk_world_build: %d verts, %d indexes, %d batches\n",
			   tr_world.numDrawVerts, writeOffset, numBatches );
}

void vk_world_draw( const float mvp[16] ) {
	float push_data[32];
	VkDeviceSize vb_offset = 0;
	int i;
	static qboolean firstDraw = qtrue;

	if ( !s_built ) {
		return;
	}

	if ( firstDraw ) {
		ri.Printf( PRINT_ALL, "...vk_world_draw checkpoint: first draw starting, %d batches\n", s_numBatches );
	}

	memset( push_data, 0, sizeof( push_data ) );
	memcpy( push_data, mvp, 16 * sizeof( float ) );
	qvkCmdPushConstants( vk.command_buffer, vk.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT,
		0, sizeof( push_data ), push_data );

	qvkCmdBindPipeline( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_3d_opaque );
	qvkCmdBindVertexBuffers( vk.command_buffer, 0, 1, &s_vertexBuffer, &vb_offset );
	qvkCmdBindIndexBuffer( vk.command_buffer, s_indexBuffer, 0, VK_INDEX_TYPE_UINT32 );

	for ( i = 0; i < s_numBatches; i++ ) {
		const worldDrawBatch_t *batch = &s_batches[i];
		qboolean runOpen = qfalse;
		uint32_t runFirst = 0, runCount = 0;
		int j;

		shader_t *shader = R_GetShaderByHandle( batch->shader );
		if ( !shader || shader->numStages < 1 || !shader->stages[0].image ||
			 shader->stages[0].image->vk_descriptor == VK_NULL_HANDLE ) {
			continue;
		}

		qvkCmdBindDescriptorSets( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout,
			0, 1, &shader->stages[0].image->vk_descriptor, 0, NULL );

		// Contiguous visible refs still merge into a single draw call.
		for ( j = 0; j < batch->numSurfRefs; j++ ) {
			const worldSurfRef_t *ref = &s_surfRefs[batch->firstSurfRef + j];

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

	if ( firstDraw ) {
		ri.Printf( PRINT_ALL, "...vk_world_draw checkpoint: first draw recorded ok\n" );
		firstDraw = qfalse;
	}
}
