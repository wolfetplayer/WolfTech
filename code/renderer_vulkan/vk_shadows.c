// vk_shadows.c -- stencil shadow volumes (r_shadows 2), same two-sided z-pass algorithm as code/renderer/tr_shadows.c. See vk_shadows.h.

#include <string.h>
#include <math.h>

#include "VKimpl.h"
#include "vk_instance.h"
#include "vk_pipelines.h"
#include "vk_frame.h"
#include "tr_light.h"
#include "tr_image.h"
#include "vk_shadows.h"

#define SHADOW_MAX_VERTS 10000
#define SHADOW_MAX_TRIS 10000
#define SHADOW_MAX_EDGES 8192 // silhouette edges actually drawn; always << numTris*3 for a closed mesh
#define SHADOW_SCRATCH_VERTS ( SHADOW_MAX_EDGES * 6 )
#define SHADOW_EXTRUDE_DIST 512.0f

typedef struct { int indexes[3]; } shadowTri_t;

typedef struct {
	int i2;
	qboolean facing;
} edgeDef_t;

#define MAX_EDGE_DEFS 32

static vec3_t s_nearXyz[SHADOW_MAX_VERTS];
static vec3_t s_farXyz[SHADOW_MAX_VERTS];
static int s_numVerts;

static shadowTri_t s_tris[SHADOW_MAX_TRIS];
static int s_numTris;

static edgeDef_t s_edgeDefs[SHADOW_MAX_VERTS][MAX_EDGE_DEFS];
static int s_numEdgeDefs[SHADOW_MAX_VERTS];

static VkBuffer s_vertexBuffer;
static VkDeviceMemory s_vertexMemory;
static vk2d_vertex_t *s_mapped;
static uint32_t s_writeOffset;
static uint32_t s_regionBase; // vk_current_frame_slot() * SHADOW_SCRATCH_VERTS -- see vk_shadows_begin_frame

static cvar_t *r_shadows;
static qboolean s_anyStamped; // qtrue once any entity has stamped the stencil buffer this frame

extern uint32_t vk_find_memory_type( uint32_t type_bits, VkMemoryPropertyFlags properties );

void vk_shadows_init( void ) {
	VkBufferCreateInfo buf_desc;
	VkMemoryRequirements mem_reqs;
	VkMemoryAllocateInfo alloc_info;

	r_shadows = ri.Cvar_Get( "r_shadows", "1", CVAR_ARCHIVE );

	buf_desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buf_desc.pNext = NULL;
	buf_desc.flags = 0;
	buf_desc.size = (VkDeviceSize)SHADOW_SCRATCH_VERTS * VK_FRAMES_IN_FLIGHT * sizeof( vk2d_vertex_t );
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

void vk_shadows_shutdown( void ) {
	if ( s_mapped ) {
		qvkUnmapMemory( vk.device, s_vertexMemory );
		s_mapped = NULL;
	}
	qvkDestroyBuffer( vk.device, s_vertexBuffer, NULL );
	qvkFreeMemory( vk.device, s_vertexMemory, NULL );
}

qboolean vk_shadows_entity_casts( const refEntity_t *ent ) {
	return r_shadows->integer == 2 && !( ent->renderfx & ( RF_NOSHADOW | RF_DEPTHHACK ) );
}

void vk_shadows_begin_frame( void ) {
	s_anyStamped = qfalse;
	s_regionBase = vk_current_frame_slot() * (uint32_t)SHADOW_SCRATCH_VERTS;
}

void vk_shadows_begin_entity( void ) {
	s_numVerts = 0;
	s_numTris = 0;
}

void vk_shadows_add_surface( const float *positions, size_t posStride, int numVerts,
							   const void *trianglesPtr, int numTriangles ) {
	const shadowTri_t *tri = (const shadowTri_t *)trianglesPtr;
	int i, base;

	if ( s_numVerts + numVerts > SHADOW_MAX_VERTS || s_numTris + numTriangles > SHADOW_MAX_TRIS ) {
		return;
	}

	base = s_numVerts;
	for ( i = 0; i < numVerts; i++ ) {
		const float *p = (const float *)( (const byte *)positions + (size_t)i * posStride );
		VectorCopy( p, s_nearXyz[base + i] );
	}
	for ( i = 0; i < numTriangles; i++ ) {
		s_tris[s_numTris + i].indexes[0] = base + tri[i].indexes[0];
		s_tris[s_numTris + i].indexes[1] = base + tri[i].indexes[1];
		s_tris[s_numTris + i].indexes[2] = base + tri[i].indexes[2];
	}
	s_numVerts += numVerts;
	s_numTris += numTriangles;
}

static void AddEdgeDef( int i1, int i2, qboolean facing ) {
	int c = s_numEdgeDefs[i1];
	if ( c >= MAX_EDGE_DEFS ) {
		return; // overflow -- matches the reference's own silent drop
	}
	s_edgeDefs[i1][c].i2 = i2;
	s_edgeDefs[i1][c].facing = facing;
	s_numEdgeDefs[i1]++;
}

static void EmitSilhouetteQuad( int i1, int i2 ) {
	vk2d_vertex_t *out;
	static const byte white[4] = { 255, 255, 255, 255 };
	int i;

	if ( s_writeOffset + 6 > SHADOW_SCRATCH_VERTS ) {
		return;
	}
	out = &s_mapped[s_regionBase + s_writeOffset];

	// GL_TRIANGLE_STRIP(near1, far1, near2, far2) converted to a triangle list.
	VectorCopy( s_nearXyz[i1], out[0].pos );
	VectorCopy( s_farXyz[i1], out[1].pos );
	VectorCopy( s_nearXyz[i2], out[2].pos );
	VectorCopy( s_nearXyz[i2], out[3].pos );
	VectorCopy( s_farXyz[i1], out[4].pos );
	VectorCopy( s_farXyz[i2], out[5].pos );
	for ( i = 0; i < 6; i++ ) {
		out[i].color[0] = white[0]; out[i].color[1] = white[1]; out[i].color[2] = white[2]; out[i].color[3] = white[3];
		out[i].uv[0] = 0.0f; out[i].uv[1] = 0.0f;
	}

	s_writeOffset += 6;
}

// Fog's "enabled" flag is forced off -- the darken quad is in clip space, not world space, so a real fog distance calc on it would be meaningless.
static void PushTransform( const float viewProj[16], const float fogPush[12] ) {
	float push_data[32];
	float noFog[12];

	memcpy( noFog, fogPush, sizeof( noFog ) );
	noFog[11] = 0.0f;

	memset( push_data, 0, sizeof( push_data ) );
	memcpy( push_data, viewProj, 16 * sizeof( float ) );
	memcpy( push_data + 16, noFog, 12 * sizeof( float ) );
	qvkCmdPushConstants( vk.command_buffer, vk.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		0, sizeof( push_data ), push_data );
}

void vk_shadows_finish_entity( const refEntity_t *ent, const float viewProj[16], const float fogPush[12] ) {
	vec3_t origin, ambient, directed, lightDir;
	VkDeviceSize vb_offset = (VkDeviceSize)s_regionBase * sizeof( vk2d_vertex_t );
	int i;

	if ( s_numVerts == 0 || s_numTris == 0 ) {
		return;
	}

	if ( ent->renderfx & RF_LIGHTING_ORIGIN ) {
		VectorCopy( ent->lightingOrigin, origin );
	} else {
		VectorCopy( ent->origin, origin );
	}
	R_LightForPoint( origin, ambient, directed, lightDir );

	for ( i = 0; i < s_numVerts; i++ ) {
		VectorMA( s_nearXyz[i], -SHADOW_EXTRUDE_DIST, lightDir, s_farXyz[i] );
	}

	// decide which triangles face the light, and build the directed-edge list
	memset( s_numEdgeDefs, 0, sizeof( int ) * s_numVerts );
	for ( i = 0; i < s_numTris; i++ ) {
		vec3_t d1, d2, normal;
		int i1 = s_tris[i].indexes[0], i2 = s_tris[i].indexes[1], i3 = s_tris[i].indexes[2];
		qboolean facing;

		VectorSubtract( s_nearXyz[i2], s_nearXyz[i1], d1 );
		VectorSubtract( s_nearXyz[i3], s_nearXyz[i1], d2 );
		CrossProduct( d1, d2, normal );
		facing = DotProduct( normal, lightDir ) > 0.0f;

		AddEdgeDef( i1, i2, facing );
		AddEdgeDef( i2, i3, facing );
		AddEdgeDef( i3, i1, facing );
	}

	// an edge is a silhouette edge if it's front-facing and has no reverse-direction front-facing twin
	s_writeOffset = 0;
	for ( i = 0; i < s_numVerts; i++ ) {
		int c = s_numEdgeDefs[i], j;
		for ( j = 0; j < c; j++ ) {
			int i2, k, c2;
			qboolean hasTwin = qfalse;

			if ( !s_edgeDefs[i][j].facing ) {
				continue;
			}
			i2 = s_edgeDefs[i][j].i2;
			c2 = s_numEdgeDefs[i2];
			for ( k = 0; k < c2; k++ ) {
				if ( s_edgeDefs[i2][k].i2 == i && s_edgeDefs[i2][k].facing ) {
					hasTwin = qtrue;
					break;
				}
			}
			if ( hasTwin ) {
				continue;
			}
			EmitSilhouetteQuad( i, i2 );
		}
	}

	if ( s_writeOffset == 0 ) {
		return;
	}

	PushTransform( viewProj, fogPush );
	qvkCmdBindVertexBuffers( vk.command_buffer, 0, 1, &s_vertexBuffer, &vb_offset );
	// Neither of these two passes writes color (pure stencil stamping), so this texture never shows --
	// white anyway, not the unrelated "shader failed to resolve" default.
	qvkCmdBindDescriptorSets( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout,
		0, 1, &R_GetWhiteImage()->vk_descriptor, 0, NULL );

	qvkCmdBindPipeline( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_shadow_volume_incr );
	qvkCmdDraw( vk.command_buffer, s_writeOffset, 1, 0, 0 );

	qvkCmdBindPipeline( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_shadow_volume_decr );
	qvkCmdDraw( vk.command_buffer, s_writeOffset, 1, 0, 0 );

	s_anyStamped = qtrue;
}

void vk_shadows_draw_darken( const float viewProj[16], const float fogPush[12] ) {
	float identity[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
	VkDeviceSize vb_offset = (VkDeviceSize)s_regionBase * sizeof( vk2d_vertex_t );
	vk2d_vertex_t quad[6];
	static const float z = 0.0f;
	int i;
	(void)viewProj;

	if ( !s_anyStamped ) {
		return;
	}

	// Full-viewport quad in clip space -- the stencil test alone decides where this darkens.
	quad[0].pos[0] = -1.0f; quad[0].pos[1] = -1.0f; quad[0].pos[2] = z;
	quad[1].pos[0] =  1.0f; quad[1].pos[1] = -1.0f; quad[1].pos[2] = z;
	quad[2].pos[0] =  1.0f; quad[2].pos[1] =  1.0f; quad[2].pos[2] = z;
	quad[3].pos[0] = -1.0f; quad[3].pos[1] = -1.0f; quad[3].pos[2] = z;
	quad[4].pos[0] =  1.0f; quad[4].pos[1] =  1.0f; quad[4].pos[2] = z;
	quad[5].pos[0] = -1.0f; quad[5].pos[1] =  1.0f; quad[5].pos[2] = z;
	for ( i = 0; i < 6; i++ ) {
		quad[i].color[0] = 153; quad[i].color[1] = 153; quad[i].color[2] = 153; quad[i].color[3] = 255; // 0.6 (reference's own darken factor)
		quad[i].uv[0] = 0.0f; quad[i].uv[1] = 0.0f;
	}

	if ( SHADOW_SCRATCH_VERTS < 6 ) {
		return; // never true -- keeps the memcpy below obviously in-bounds
	}
	memcpy( &s_mapped[s_regionBase], quad, sizeof( quad ) );

	PushTransform( identity, fogPush );
	qvkCmdBindVertexBuffers( vk.command_buffer, 0, 1, &s_vertexBuffer, &vb_offset );
	// Must be white: the darken factor is baked into the quad's vertex color (0.6 gray) and multiplied
	// by this texture (GLS_SRCBLEND_DST_COLOR) -- anything but flat white would pattern the darken.
	qvkCmdBindDescriptorSets( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout,
		0, 1, &R_GetWhiteImage()->vk_descriptor, 0, NULL );
	qvkCmdBindPipeline( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_shadow_darken );
	qvkCmdDraw( vk.command_buffer, 6, 1, 0, 0 );

	s_anyStamped = qfalse;
}
