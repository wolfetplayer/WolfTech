// vk_draw3d.c -- depth-tested 3D geometry recording: a sanity test cube, and per-entity position markers until real model rendering lands.

#include <string.h>

#include "VKimpl.h"
#include "vk_instance.h"
#include "vk_pipelines.h"
#include "tr_image.h"
#include "vk_entity.h"
#include "vk_draw3d.h"

#define VERTS_PER_CUBE 36
#define VK3D_MAX_VERTS 256
#define VK3D_MAX_MARKER_VERTS ( MAX_REFENTITIES * VERTS_PER_CUBE )

static VkBuffer s_vertexBuffer;
static VkDeviceMemory s_vertexMemory;
static vk2d_vertex_t *s_mapped;

static VkBuffer s_markerVertexBuffer;
static VkDeviceMemory s_markerVertexMemory;
static vk2d_vertex_t *s_markerMapped;

static cvar_t *r_vk_test3d;
static cvar_t *r_vk_showEntityMarkers;

extern uint32_t vk_find_memory_type( uint32_t type_bits, VkMemoryPropertyFlags properties );

static void create_mapped_vertex_buffer( VkDeviceSize vertCount, VkBuffer *outBuffer, VkDeviceMemory *outMemory, vk2d_vertex_t **outMapped ) {
	VkBufferCreateInfo buf_desc;
	buf_desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buf_desc.pNext = NULL;
	buf_desc.flags = 0;
	buf_desc.size = vertCount * sizeof( vk2d_vertex_t );
	buf_desc.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	buf_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	buf_desc.queueFamilyIndexCount = 0;
	buf_desc.pQueueFamilyIndices = NULL;
	VK_CHECK( qvkCreateBuffer( vk.device, &buf_desc, NULL, outBuffer ) );

	VkMemoryRequirements mem_reqs;
	qvkGetBufferMemoryRequirements( vk.device, *outBuffer, &mem_reqs );

	VkMemoryAllocateInfo alloc_info;
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	alloc_info.allocationSize = mem_reqs.size;
	alloc_info.memoryTypeIndex = vk_find_memory_type( mem_reqs.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	VK_CHECK( qvkAllocateMemory( vk.device, &alloc_info, NULL, outMemory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, *outBuffer, *outMemory, 0 ) );

	VK_CHECK( qvkMapMemory( vk.device, *outMemory, 0, VK_WHOLE_SIZE, 0, (void **)outMapped ) );
}

static void destroy_mapped_vertex_buffer( VkBuffer buffer, VkDeviceMemory memory, void *mapped ) {
	if ( mapped ) {
		qvkUnmapMemory( vk.device, memory );
	}
	qvkDestroyBuffer( vk.device, buffer, NULL );
	qvkFreeMemory( vk.device, memory, NULL );
}

void vk_draw3d_init( void ) {
	// Sanity check: draws a fixed RGB cube in the 3D scene. Off by default now that real world geometry renders.
	r_vk_test3d = ri.Cvar_Get( "r_vk_test3d", "0", CVAR_ARCHIVE );
	// Marker cube per entity lacking real mesh rendering. Off by default: a misplaced one (e.g. a bad tag attachment) can fill the whole view.
	r_vk_showEntityMarkers = ri.Cvar_Get( "r_vk_showEntityMarkers", "0", CVAR_ARCHIVE );

	create_mapped_vertex_buffer( VK3D_MAX_VERTS, &s_vertexBuffer, &s_vertexMemory, &s_mapped );
	create_mapped_vertex_buffer( VK3D_MAX_MARKER_VERTS, &s_markerVertexBuffer, &s_markerVertexMemory, &s_markerMapped );
}

void vk_draw3d_shutdown( void ) {
	destroy_mapped_vertex_buffer( s_vertexBuffer, s_vertexMemory, s_mapped );
	s_mapped = NULL;
	destroy_mapped_vertex_buffer( s_markerVertexBuffer, s_markerVertexMemory, s_markerMapped );
	s_markerMapped = NULL;
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

static void bind_opaque_3d_for_draw( const float mvp[16], const float fogPush[12] ) {
	float push_data[32];

	memset( push_data, 0, sizeof( push_data ) );
	memcpy( push_data, mvp, 16 * sizeof( float ) );
	memcpy( push_data + 16, fogPush, 12 * sizeof( float ) );
	qvkCmdPushConstants( vk.command_buffer, vk.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		0, sizeof( push_data ), push_data );

	qvkCmdBindPipeline( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_3d_opaque );

	qvkCmdBindDescriptorSets( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout,
		0, 1, &R_GetDefaultImage()->vk_descriptor, 0, NULL );
}

void vk_draw3d_test_cube( const vec3_t origin, float halfSize, const float mvp[16], const float fogPush[12] ) {
	VkDeviceSize vb_offset = 0;

	if ( !r_vk_test3d->integer ) {
		return;
	}

	build_cube( s_mapped, origin, halfSize );

	bind_opaque_3d_for_draw( mvp, fogPush );
	qvkCmdBindVertexBuffers( vk.command_buffer, 0, 1, &s_vertexBuffer, &vb_offset );
	qvkCmdDraw( vk.command_buffer, VERTS_PER_CUBE, 1, 0, 0 );
}

// r_vk_showEntityMarkers-gated: one small cube per scene entity, standing in for real model rendering.
void vk_draw3d_entity_markers( const refEntity_t *entities, int numEntities, float halfSize, const float mvp[16], const float fogPush[12] ) {
	VkDeviceSize vb_offset = 0;
	int i, written = 0;

	if ( !r_vk_showEntityMarkers->integer || numEntities <= 0 ) {
		return;
	}
	if ( numEntities > MAX_REFENTITIES ) {
		numEntities = MAX_REFENTITIES;
	}

	for ( i = 0; i < numEntities; i++ ) {
		if ( vk_entity_has_mesh( &entities[i] ) ) {
			continue; // drawn for real by vk_entity_draw_all instead
		}
		build_cube( &s_markerMapped[written * VERTS_PER_CUBE], entities[i].origin, halfSize );
		written++;
	}

	if ( written == 0 ) {
		return;
	}

	bind_opaque_3d_for_draw( mvp, fogPush );
	qvkCmdBindVertexBuffers( vk.command_buffer, 0, 1, &s_markerVertexBuffer, &vb_offset );
	qvkCmdDraw( vk.command_buffer, (uint32_t)written * VERTS_PER_CUBE, 1, 0, 0 );
}
