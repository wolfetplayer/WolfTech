// vk_sky.c -- static, camera-centered sky box, see vk_sky.h. No per-triangle visibility clipping or portal sky; relies on pipeline_sky's depth test/write being off to stay behind everything.

#include <math.h>
#include <string.h>

#include "VKimpl.h"
#include "vk_instance.h"
#include "vk_pipelines.h"
#include "tr_image.h"
#include "tr_bsp.h"
#include "tr_shade_calc.h"
#include "vk_sky.h"

#define SKY_SUBDIVISIONS 8
#define SKY_VERTS_PER_FACE ( SKY_SUBDIVISIONS * SKY_SUBDIVISIONS * 6 ) // 2 triangles * 3 verts per grid cell, non-indexed
#define SKY_VERTS ( SKY_VERTS_PER_FACE * 6 ) // 6 faces
#define SKY_ZFAR_SCALE ( 1.0f / 1.75f ) // matches code/renderer/tr_sky.c's MakeSkyVec: keeps the box's farthest corner inside zFar
#define SKY_CLOUD_MAX_LAYERS MAX_SHADER_STAGES // one vertex block per potential cloud-layer stage, after the outer box's own block

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
	buf_desc.size = SKY_VERTS * ( 1 + SKY_CLOUD_MAX_LAYERS ) * sizeof( vk2d_vertex_t );
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

static void sky_grid_vert( float s, float t, int face, float size, const vec3_t camPos, vk2d_vertex_t *out ) {
	vec3_t offset;
	float u, v;

	sky_vec( s, t, face, size, offset );
	u = ( s + 1.0f ) * 0.5f;
	v = 1.0f - ( t + 1.0f ) * 0.5f; // matches MakeSkyVec's own t = 1 - t flip
	set_sky_vert( out, camPos, offset[0], offset[1], offset[2], u, v );
}

static void build_sky_box( vk2d_vertex_t *out, const vec3_t camPos, float size ) {
	int face, gs, gt, i = 0;

	for ( face = 0; face < 6; face++ ) {
		for ( gt = 0; gt < SKY_SUBDIVISIONS; gt++ ) {
			float t0 = -1.0f + 2.0f * (float)gt / SKY_SUBDIVISIONS;
			float t1 = -1.0f + 2.0f * (float)( gt + 1 ) / SKY_SUBDIVISIONS;

			for ( gs = 0; gs < SKY_SUBDIVISIONS; gs++ ) {
				float s0 = -1.0f + 2.0f * (float)gs / SKY_SUBDIVISIONS;
				float s1 = -1.0f + 2.0f * (float)( gs + 1 ) / SKY_SUBDIVISIONS;

				sky_grid_vert( s0, t0, face, size, camPos, &out[i++] );
				sky_grid_vert( s1, t0, face, size, camPos, &out[i++] );
				sky_grid_vert( s1, t1, face, size, camPos, &out[i++] );
				sky_grid_vert( s0, t0, face, size, camPos, &out[i++] );
				sky_grid_vert( s1, t1, face, size, camPos, &out[i++] );
				sky_grid_vert( s0, t1, face, size, camPos, &out[i++] );
			}
		}
	}
}

#define SQR( a ) ( ( a ) * ( a ) )
#define SKY_CLOUD_RADIUS_WORLD 4096.0f // matches code/renderer/tr_sky.c's R_InitSkyTexCoords

static void cloud_dir_to_uv( const vec3_t dir, float heightCloud, float outST[2] ) {
	float dd = DotProduct( dir, dir );
	float p, v[3];

	if ( dd < 0.000001f ) {
		outST[0] = outST[1] = 0.0f;
		return;
	}

	p = ( 1.0f / ( 2.0f * dd ) ) *
		( -2.0f * dir[2] * SKY_CLOUD_RADIUS_WORLD +
		  2.0f * sqrtf( SQR( dir[2] ) * SQR( SKY_CLOUD_RADIUS_WORLD ) +
						2.0f * SQR( dir[0] ) * SKY_CLOUD_RADIUS_WORLD * heightCloud +
						SQR( dir[0] ) * SQR( heightCloud ) +
						2.0f * SQR( dir[1] ) * SKY_CLOUD_RADIUS_WORLD * heightCloud +
						SQR( dir[1] ) * SQR( heightCloud ) +
						2.0f * SQR( dir[2] ) * SKY_CLOUD_RADIUS_WORLD * heightCloud +
						SQR( dir[2] ) * SQR( heightCloud ) ) );

	VectorScale( dir, p, v );
	v[2] += SKY_CLOUD_RADIUS_WORLD;
	VectorNormalize( v );

	outST[0] = acosf( ( v[0] < -1.0f ) ? -1.0f : ( v[0] > 1.0f ) ? 1.0f : v[0] );
	outST[1] = acosf( ( v[1] < -1.0f ) ? -1.0f : ( v[1] > 1.0f ) ? 1.0f : v[1] );
}

// base is the already-built outer-box block (only its positions are reused; UV is recomputed per cloud_dir_to_uv above).
static void write_cloud_layer( vk2d_vertex_t *out, const vk2d_vertex_t *base, const vec3_t camPos, float cloudHeight, const shaderStage_t *stage ) {
	static const byte whiteBase[4] = { 255, 255, 255, 255 };
	int i;

	for ( i = 0; i < SKY_VERTS; i++ ) {
		vec3_t dir;
		float baseST[2], st[2];
		byte color[4];

		out[i].pos[0] = base[i].pos[0];
		out[i].pos[1] = base[i].pos[1];
		out[i].pos[2] = base[i].pos[2];

		VectorSubtract( base[i].pos, camPos, dir );
		cloud_dir_to_uv( dir, cloudHeight, baseST );
		R_CalcStageTexCoord( stage, baseST[0], baseST[1], base[i].pos, NULL, st );
		out[i].uv[0] = st[0];
		out[i].uv[1] = st[1];

		R_CalcStageColorAlpha( stage, NULL, whiteBase, color );
		out[i].color[0] = color[0];
		out[i].color[1] = color[1];
		out[i].color[2] = color[2];
		out[i].color[3] = color[3];
	}
}

void vk_sky_draw( const vec3_t camPos, const float mvp[16], const float fogPush[12], float zFar ) {
	float push_data[32];
	VkDeviceSize vb_offset = 0;
	int face, i, numCloudLayers;
	shader_t *skyShader;

	if ( !tr_world.hasSky || !s_mapped ) {
		return;
	}

	build_sky_box( s_mapped, camPos, zFar * SKY_ZFAR_SCALE );

	skyShader = tr_world.skyShader;
	numCloudLayers = 0;
	if ( skyShader && skyShader->skyCloudHeight > 0.0f ) { // matches reference R_BuildCloudData's "if (shader->sky.cloudHeight)" gate
		numCloudLayers = skyShader->numStages;
		if ( numCloudLayers > SKY_CLOUD_MAX_LAYERS ) {
			numCloudLayers = SKY_CLOUD_MAX_LAYERS;
		}
		for ( i = 0; i < numCloudLayers; i++ ) {
			write_cloud_layer( s_mapped + SKY_VERTS * ( 1 + i ), s_mapped, camPos, skyShader->skyCloudHeight, &skyShader->stages[i] );
		}
	}

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
		qvkCmdDraw( vk.command_buffer, SKY_VERTS_PER_FACE, 1, (uint32_t)face * SKY_VERTS_PER_FACE, 0 );
	}

	for ( i = 0; i < numCloudLayers; i++ ) {
		const shaderStage_t *stage = &skyShader->stages[i];
		image_t *img = R_GetStageImage( stage );
		VkPipeline pipeline;

		if ( !img || img->vk_descriptor == VK_NULL_HANDLE ) {
			continue;
		}

		pipeline = stage->blendEnabled ?
			vk_getPipelineSkyCloud( stage->srcBlend, stage->dstBlend ) :
			vk_getPipelineSkyCloud( VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO );
		qvkCmdBindPipeline( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline );
		qvkCmdBindDescriptorSets( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout,
			0, 1, &img->vk_descriptor, 0, NULL );
		qvkCmdDraw( vk.command_buffer, SKY_VERTS, 1, (uint32_t)SKY_VERTS * ( 1 + i ), 0 );
	}
}
