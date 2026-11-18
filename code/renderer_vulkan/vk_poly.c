// vk_poly.c -- see vk_poly.h. Each poly is fan-triangulated on the CPU (matching RB_SurfacePolychain) into a scratch buffer, then drawn with its own draw call since each may use a different shader.

#include <string.h>

#include "VKimpl.h"
#include "vk_instance.h"
#include "vk_pipelines.h"
#include "tr_shader.h"
#include "tr_scene.h"
#include "vk_poly.h"

// A poly with numVerts fans into (numVerts - 2) triangles; this is the worst case summed over every scene poly.
#define POLY_SCRATCH_VERTS ( 3 * MAX_SCENE_POLY_VERTS )

static VkBuffer s_vertexBuffer;
static VkDeviceMemory s_vertexMemory;
static vk2d_vertex_t *s_mapped;

extern uint32_t vk_find_memory_type( uint32_t type_bits, VkMemoryPropertyFlags properties );

void vk_poly_init( void ) {
	VkBufferCreateInfo buf_desc;
	VkMemoryRequirements mem_reqs;
	VkMemoryAllocateInfo alloc_info;

	buf_desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buf_desc.pNext = NULL;
	buf_desc.flags = 0;
	buf_desc.size = POLY_SCRATCH_VERTS * sizeof( vk2d_vertex_t );
	buf_desc.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	buf_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	buf_desc.queueFamilyIndexCount = 0;
	buf_desc.pQueueFamilyIndices = NULL;
	VK_CHECK( qvkCreateBuffer( vk.device, &buf_desc, NULL, &s_vertexBuffer ) );

	qvkGetBufferMemoryRequirements( vk.device, s_vertexBuffer, &mem_reqs );

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = mem_reqs.size;
	alloc_info.memoryTypeIndex = vk_find_memory_type( mem_reqs.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &s_vertexMemory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, s_vertexBuffer, s_vertexMemory, 0 ) );

	VK_CHECK( qvkMapMemory( vk.device, s_vertexMemory, 0, VK_WHOLE_SIZE, 0, (void **)&s_mapped ) );
}

void vk_poly_shutdown( void ) {
	if ( s_mapped ) {
		qvkUnmapMemory( vk.device, s_vertexMemory );
		s_mapped = NULL;
	}
	qvkDestroyBuffer( vk.device, s_vertexBuffer, NULL );
	qvkFreeMemory( vk.device, s_vertexMemory, NULL );
}

static void set_poly_vert( vk2d_vertex_t *out, const polyVert_t *in ) {
	out->pos[0] = in->xyz[0];
	out->pos[1] = in->xyz[1];
	out->pos[2] = in->xyz[2];
	out->color[0] = in->modulate[0];
	out->color[1] = in->modulate[1];
	out->color[2] = in->modulate[2];
	out->color[3] = in->modulate[3];
	out->uv[0] = in->st[0];
	out->uv[1] = in->st[1];
}

void vk_poly_draw( const float mvp[16], const float fogPush[12] ) {
	float push_data[32];
	VkDeviceSize vb_offset = 0;
	int numPolys, i;
	const scenePoly_t *polys = R_GetScenePolys( &numPolys );
	const polyVert_t *polyVerts = R_GetScenePolyVerts();
	uint32_t writeOffset = 0;
	VkPipeline boundPipeline = VK_NULL_HANDLE;

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
		const shaderStage_t *stage = &shader->stages[0];
		VkPipeline pipeline;
		uint32_t firstVert = writeOffset;
		uint32_t vertCount;
		int v;

		if ( poly->numVerts < 3 || !shader->stages[0].image || shader->stages[0].image->vk_descriptor == VK_NULL_HANDLE ||
			 writeOffset + (uint32_t)( poly->numVerts - 2 ) * 3 > POLY_SCRATCH_VERTS ) {
			continue;
		}

		for ( v = 0; v < poly->numVerts - 2; v++ ) {
			set_poly_vert( &s_mapped[writeOffset + 0], &polyVerts[poly->firstVert] );
			set_poly_vert( &s_mapped[writeOffset + 1], &polyVerts[poly->firstVert + v + 1] );
			set_poly_vert( &s_mapped[writeOffset + 2], &polyVerts[poly->firstVert + v + 2] );
			writeOffset += 3;
		}
		vertCount = writeOffset - firstVert;

		pipeline = vk_getPipeline3D( stage->blendEnabled, stage->srcBlend, stage->dstBlend,
			vk_cullModeForShader( shader->cullType ), stage->alphaTestFunc );
		if ( pipeline != boundPipeline ) {
			qvkCmdBindPipeline( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline );
			boundPipeline = pipeline;
		}
		qvkCmdBindDescriptorSets( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout,
			0, 1, &shader->stages[0].image->vk_descriptor, 0, NULL );
		qvkCmdDraw( vk.command_buffer, vertCount, 1, firstVert, 0 );
	}
}
