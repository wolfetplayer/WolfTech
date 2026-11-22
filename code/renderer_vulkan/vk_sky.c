// vk_sky.c -- static, camera-centered sky box, see vk_sky.h. No per-triangle visibility clipping or portal sky; relies on pipeline_sky's depth test/write being off to stay behind everything.

#include <string.h>

#include "VKimpl.h"
#include "vk_instance.h"
#include "vk_pipelines.h"
#include "tr_image.h"
#include "tr_bsp.h"
#include "vk_sky.h"

#define SKY_VERTS 36 // 6 faces * 2 triangles * 3 verts, non-indexed
#define SKY_ZFAR_SCALE ( 1.0f / 1.75f ) // matches code/renderer/tr_sky.c's MakeSkyVec: keeps the box's farthest corner inside zFar

static VkBuffer s_vertexBuffer;
static VkDeviceMemory s_vertexMemory;
static vk2d_vertex_t *s_mapped;

extern uint32_t vk_find_memory_type( uint32_t type_bits, VkMemoryPropertyFlags properties );

void vk_sky_init( void ) {
	VkBufferCreateInfo buf_desc;
	VkMemoryRequirements mem_reqs;
	VkMemoryAllocateInfo alloc_info;

	buf_desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buf_desc.pNext = NULL;
	buf_desc.flags = 0;
	buf_desc.size = SKY_VERTS * sizeof( vk2d_vertex_t );
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

void vk_sky_shutdown( void ) {
	if ( s_mapped ) {
		qvkUnmapMemory( vk.device, s_vertexMemory );
		s_mapped = NULL;
	}
	qvkDestroyBuffer( vk.device, s_vertexBuffer, NULL );
	qvkFreeMemory( vk.device, s_vertexMemory, NULL );
}

static void set_sky_vert( vk2d_vertex_t *v, const vec3_t camPos, float ox, float oy, float oz, float u, float vcoord ) {
	v->pos[0] = camPos[0] + ox;
	v->pos[1] = camPos[1] + oy;
	v->pos[2] = camPos[2] + oz;
	v->color[0] = v->color[1] = v->color[2] = v->color[3] = 255;
	v->uv[0] = u;
	v->uv[1] = vcoord;
}

// Matches code/renderer/tr_sky.c's MakeSkyVec exactly (face axis order rt/lf/bk/ft/up/dn) -- the old hand-picked corners/UVs here didn't, which mirrored/misplaced at least the rt face.
static const int sky_st_to_vec[6][3] = {
	{  3, -1,  2 }, // rt
	{ -3,  1,  2 }, // lf
	{  1,  3,  2 }, // bk
	{ -1, -3,  2 }, // ft
	{ -2, -1,  3 }, // up
	{  2, -1, -3 }, // dn
};

static void sky_vec( float s, float t, int face, float size, vec3_t outOffset ) {
	float b[3] = { s * size, t * size, size };
	int j;
	for ( j = 0; j < 3; j++ ) {
		int k = sky_st_to_vec[face][j];
		outOffset[j] = ( k < 0 ) ? -b[-k - 1] : b[k - 1];
	}
}

static void build_sky_box( vk2d_vertex_t *out, const vec3_t camPos, float size ) {
	static const float stCorner[4][2] = { { -1, -1 }, { 1, -1 }, { 1, 1 }, { -1, 1 } };
	static const int order[6] = { 0, 1, 2, 0, 2, 3 };
	int face, i = 0;

	for ( face = 0; face < 6; face++ ) {
		vec3_t offset[4];
		float uv[4][2];
		int c;

		for ( c = 0; c < 4; c++ ) {
			float s = stCorner[c][0], t = stCorner[c][1];
			sky_vec( s, t, face, size, offset[c] );
			uv[c][0] = ( s + 1.0f ) * 0.5f;
			uv[c][1] = 1.0f - ( t + 1.0f ) * 0.5f; // matches MakeSkyVec's own t = 1 - t flip
		}

		for ( c = 0; c < 6; c++ ) {
			int k = order[c];
			set_sky_vert( &out[i++], camPos, offset[k][0], offset[k][1], offset[k][2], uv[k][0], uv[k][1] );
		}
	}
}

void vk_sky_draw( const vec3_t camPos, const float mvp[16], const float fogPush[12], float zFar ) {
	float push_data[32];
	VkDeviceSize vb_offset = 0;
	int face;

	if ( !tr_world.hasSky || !s_mapped ) {
		return;
	}

	build_sky_box( s_mapped, camPos, zFar * SKY_ZFAR_SCALE );

	memset( push_data, 0, sizeof( push_data ) );
	memcpy( push_data, mvp, 16 * sizeof( float ) );
	memcpy( push_data + 16, fogPush, 12 * sizeof( float ) );
	qvkCmdPushConstants( vk.command_buffer, vk.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		0, sizeof( push_data ), push_data );

	qvkCmdBindPipeline( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_sky );
	qvkCmdBindVertexBuffers( vk.command_buffer, 0, 1, &s_vertexBuffer, &vb_offset );

	for ( face = 0; face < 6; face++ ) {
		image_t *img = tr_world.skyOuterBox[face];
		if ( !img || img->vk_descriptor == VK_NULL_HANDLE ) {
			continue;
		}
		qvkCmdBindDescriptorSets( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout,
			0, 1, &img->vk_descriptor, 0, NULL );
		qvkCmdDraw( vk.command_buffer, 6, 1, (uint32_t)face * 6, 0 );
	}
}
