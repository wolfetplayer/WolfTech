// vk_sky.c -- static, camera-centered sky box, see vk_sky.h. No per-triangle visibility clipping or portal sky; relies on pipeline_sky's depth test/write being off to stay behind everything.

#include <string.h>

#include "VKimpl.h"
#include "vk_instance.h"
#include "vk_pipelines.h"
#include "tr_image.h"
#include "tr_bsp.h"
#include "vk_sky.h"

#define SKY_VERTS 36 // 6 faces * 2 triangles * 3 verts, non-indexed
#define SKY_HALF_SIZE 4096.0f

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

// Face order matches ParseSkyParms' suf[] (rt, lf, bk, ft, up, dn); UV orientation is a best-effort pick, unverified against the reference's exact texgen.
static void build_sky_box( vk2d_vertex_t *out, const vec3_t camPos, float s ) {
	int i = 0;

	set_sky_vert( &out[i++], camPos, s, -s, -s, 0, 0 ); set_sky_vert( &out[i++], camPos, s, s, -s, 1, 0 ); set_sky_vert( &out[i++], camPos, s, s, s, 1, 1 );
	set_sky_vert( &out[i++], camPos, s, -s, -s, 0, 0 ); set_sky_vert( &out[i++], camPos, s, s, s, 1, 1 ); set_sky_vert( &out[i++], camPos, s, -s, s, 0, 1 );

	set_sky_vert( &out[i++], camPos, -s, s, -s, 0, 0 ); set_sky_vert( &out[i++], camPos, -s, -s, -s, 1, 0 ); set_sky_vert( &out[i++], camPos, -s, -s, s, 1, 1 );
	set_sky_vert( &out[i++], camPos, -s, s, -s, 0, 0 ); set_sky_vert( &out[i++], camPos, -s, -s, s, 1, 1 ); set_sky_vert( &out[i++], camPos, -s, s, s, 0, 1 );

	set_sky_vert( &out[i++], camPos, s, s, -s, 0, 0 ); set_sky_vert( &out[i++], camPos, -s, s, -s, 1, 0 ); set_sky_vert( &out[i++], camPos, -s, s, s, 1, 1 );
	set_sky_vert( &out[i++], camPos, s, s, -s, 0, 0 ); set_sky_vert( &out[i++], camPos, -s, s, s, 1, 1 ); set_sky_vert( &out[i++], camPos, s, s, s, 0, 1 );

	set_sky_vert( &out[i++], camPos, -s, -s, -s, 0, 0 ); set_sky_vert( &out[i++], camPos, s, -s, -s, 1, 0 ); set_sky_vert( &out[i++], camPos, s, -s, s, 1, 1 );
	set_sky_vert( &out[i++], camPos, -s, -s, -s, 0, 0 ); set_sky_vert( &out[i++], camPos, s, -s, s, 1, 1 ); set_sky_vert( &out[i++], camPos, -s, -s, s, 0, 1 );

	set_sky_vert( &out[i++], camPos, -s, -s, s, 0, 0 ); set_sky_vert( &out[i++], camPos, s, -s, s, 1, 0 ); set_sky_vert( &out[i++], camPos, s, s, s, 1, 1 );
	set_sky_vert( &out[i++], camPos, -s, -s, s, 0, 0 ); set_sky_vert( &out[i++], camPos, s, s, s, 1, 1 ); set_sky_vert( &out[i++], camPos, -s, s, s, 0, 1 );

	set_sky_vert( &out[i++], camPos, -s, s, -s, 0, 0 ); set_sky_vert( &out[i++], camPos, s, s, -s, 1, 0 ); set_sky_vert( &out[i++], camPos, s, -s, -s, 1, 1 );
	set_sky_vert( &out[i++], camPos, -s, s, -s, 0, 0 ); set_sky_vert( &out[i++], camPos, s, -s, -s, 1, 1 ); set_sky_vert( &out[i++], camPos, -s, -s, -s, 0, 1 );
}

// Same r_vk_debugSky cvar as tr_world.c's R_MarkVisibleSurfaces (own cvar_t pointer, registered here too). Remove once root-caused.
static cvar_t *r_vk_debugSky_sky;

void vk_sky_draw( const vec3_t camPos, const float mvp[16], const float fogPush[12] ) {
	float push_data[32];
	VkDeviceSize vb_offset = 0;
	int face;

	if ( !r_vk_debugSky_sky ) {
		r_vk_debugSky_sky = ri.Cvar_Get( "r_vk_debugSky", "0", 0 );
	}
	if ( r_vk_debugSky_sky->integer ) {
		int validFaces = 0;
		for ( face = 0; face < 6; face++ ) {
			if ( tr_world.skyOuterBox[face] && tr_world.skyOuterBox[face]->vk_descriptor != VK_NULL_HANDLE ) { validFaces++; }
		}
		ri.Printf( PRINT_ALL, "vk_sky dbg: vk_sky_draw hasSky=%d camPos=(%.1f %.1f %.1f) validFaces=%d/6\n",
			tr_world.hasSky, camPos[0], camPos[1], camPos[2], validFaces );
	}

	if ( !tr_world.hasSky || !s_mapped ) {
		return;
	}

	build_sky_box( s_mapped, camPos, SKY_HALF_SIZE );

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
