// vk_draw3d.c -- depth-tested 3D geometry recording, for now just a sanity test cube.

#include <string.h>

#include "VKimpl.h"
#include "vk_instance.h"
#include "vk_pipelines.h"
#include "tr_image.h"
#include "vk_draw3d.h"

#define VK3D_MAX_VERTS 256

static VkBuffer s_vertexBuffer;
static VkDeviceMemory s_vertexMemory;
static vk2d_vertex_t *s_mapped;

static cvar_t *r_vk_test3d;

extern uint32_t vk_find_memory_type( uint32_t type_bits, VkMemoryPropertyFlags properties );

void vk_draw3d_init( void ) {
	// Sanity check: draws a fixed RGB cube in the 3D scene. Off by default now that real world geometry renders.
	r_vk_test3d = ri.Cvar_Get( "r_vk_test3d", "0", CVAR_ARCHIVE );

	VkBufferCreateInfo buf_desc;
	buf_desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buf_desc.pNext = NULL;
	buf_desc.flags = 0;
	buf_desc.size = VK3D_MAX_VERTS * sizeof( vk2d_vertex_t );
	buf_desc.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	buf_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	buf_desc.queueFamilyIndexCount = 0;
	buf_desc.pQueueFamilyIndices = NULL;
	VK_CHECK( qvkCreateBuffer( vk.device, &buf_desc, NULL, &s_vertexBuffer ) );

	VkMemoryRequirements mem_reqs;
	qvkGetBufferMemoryRequirements( vk.device, s_vertexBuffer, &mem_reqs );

	VkMemoryAllocateInfo alloc_info;
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = mem_reqs.size;
	alloc_info.memoryTypeIndex = vk_find_memory_type( mem_reqs.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, &s_vertexMemory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, s_vertexBuffer, s_vertexMemory, 0 ) );

	VK_CHECK( qvkMapMemory( vk.device, s_vertexMemory, 0, VK_WHOLE_SIZE, 0, (void **)&s_mapped ) );
}

void vk_draw3d_shutdown( void ) {
	if ( s_mapped ) {
		qvkUnmapMemory( vk.device, s_vertexMemory );
		s_mapped = NULL;
	}
	qvkDestroyBuffer( vk.device, s_vertexBuffer, NULL );
	qvkFreeMemory( vk.device, s_vertexMemory, NULL );
}

static void set_vert( vk2d_vertex_t *v, float x, float y, float z, byte r, byte g, byte b ) {
	v->pos[0] = x;
	v->pos[1] = y;
	v->pos[2] = z;
	v->color[0] = r;
	v->color[1] = g;
	v->color[2] = b;
	v->color[3] = 255;
	v->uv[0] = 0.0f;
	v->uv[1] = 0.0f;
}

// 8 corners of an axis-aligned box, two triangles per face, one solid color per face.
static void build_cube( vk2d_vertex_t *verts, const vec3_t origin, float h ) {
	float x0 = origin[0] - h, x1 = origin[0] + h;
	float y0 = origin[1] - h, y1 = origin[1] + h;
	float z0 = origin[2] - h, z1 = origin[2] + h;
	int i = 0;

	// -X / +X (red / cyan)
	set_vert( &verts[i++], x0, y0, z0, 255, 0, 0 ); set_vert( &verts[i++], x0, y1, z0, 255, 0, 0 ); set_vert( &verts[i++], x0, y1, z1, 255, 0, 0 );
	set_vert( &verts[i++], x0, y0, z0, 255, 0, 0 ); set_vert( &verts[i++], x0, y1, z1, 255, 0, 0 ); set_vert( &verts[i++], x0, y0, z1, 255, 0, 0 );
	set_vert( &verts[i++], x1, y0, z0, 0, 255, 255 ); set_vert( &verts[i++], x1, y1, z1, 0, 255, 255 ); set_vert( &verts[i++], x1, y1, z0, 0, 255, 255 );
	set_vert( &verts[i++], x1, y0, z0, 0, 255, 255 ); set_vert( &verts[i++], x1, y0, z1, 0, 255, 255 ); set_vert( &verts[i++], x1, y1, z1, 0, 255, 255 );

	// -Y / +Y (green / magenta)
	set_vert( &verts[i++], x0, y0, z0, 0, 255, 0 ); set_vert( &verts[i++], x1, y0, z1, 0, 255, 0 ); set_vert( &verts[i++], x1, y0, z0, 0, 255, 0 );
	set_vert( &verts[i++], x0, y0, z0, 0, 255, 0 ); set_vert( &verts[i++], x0, y0, z1, 0, 255, 0 ); set_vert( &verts[i++], x1, y0, z1, 0, 255, 0 );
	set_vert( &verts[i++], x0, y1, z0, 255, 0, 255 ); set_vert( &verts[i++], x1, y1, z0, 255, 0, 255 ); set_vert( &verts[i++], x1, y1, z1, 255, 0, 255 );
	set_vert( &verts[i++], x0, y1, z0, 255, 0, 255 ); set_vert( &verts[i++], x1, y1, z1, 255, 0, 255 ); set_vert( &verts[i++], x0, y1, z1, 255, 0, 255 );

	// -Z / +Z (blue / yellow)
	set_vert( &verts[i++], x0, y0, z0, 0, 0, 255 ); set_vert( &verts[i++], x1, y1, z0, 0, 0, 255 ); set_vert( &verts[i++], x0, y1, z0, 0, 0, 255 );
	set_vert( &verts[i++], x0, y0, z0, 0, 0, 255 ); set_vert( &verts[i++], x1, y0, z0, 0, 0, 255 ); set_vert( &verts[i++], x1, y1, z0, 0, 0, 255 );
	set_vert( &verts[i++], x0, y0, z1, 255, 255, 0 ); set_vert( &verts[i++], x0, y1, z1, 255, 255, 0 ); set_vert( &verts[i++], x1, y1, z1, 255, 255, 0 );
	set_vert( &verts[i++], x0, y0, z1, 255, 255, 0 ); set_vert( &verts[i++], x1, y1, z1, 255, 255, 0 ); set_vert( &verts[i++], x1, y0, z1, 255, 255, 0 );
}

void vk_draw3d_test_cube( const vec3_t origin, float halfSize, const float mvp[16] ) {
	float push_data[32];

	if ( !r_vk_test3d->integer ) {
		return;
	}

	build_cube( s_mapped, origin, halfSize );

	memset( push_data, 0, sizeof( push_data ) );
	memcpy( push_data, mvp, 16 * sizeof( float ) );
	qvkCmdPushConstants( vk.command_buffer, vk.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT,
		0, sizeof( push_data ), push_data );

	qvkCmdBindPipeline( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_3d_opaque );

	qvkCmdBindDescriptorSets( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout,
		0, 1, &R_GetDefaultImage()->vk_descriptor, 0, NULL );

	VkDeviceSize vb_offset = 0;
	qvkCmdBindVertexBuffers( vk.command_buffer, 0, 1, &s_vertexBuffer, &vb_offset );

	qvkCmdDraw( vk.command_buffer, 36, 1, 0, 0 );
}
