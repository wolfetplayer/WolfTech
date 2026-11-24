// vk_entity.c -- CPU-skinned entity mesh rendering (MD3/MDC frame-morph, MDS/MDM bone-weighted), see vk_entity.h.

#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "VKimpl.h"
#include "vk_instance.h"
#include "vk_pipelines.h"
#include "vk_frame.h"
#include "vk_image.h"
#include "glConfig.h"
#include "tr_image.h"
#include "tr_shader.h"
#include "tr_shade_calc.h"
#include "tr_model.h"
#include "tr_skin.h"
#include "tr_bones.h"
#include "tr_light.h"
#include "tr_main.h"
#include "tr_scene.h"
#include "vk_world.h"
#include "vk_shadows.h"
#include "vk_entity.h"

// Same falloff shape the opengl1 renderer uses for dlight contributions to entity lighting.
#define DLIGHT_AT_RADIUS 16.0f
#define DLIGHT_MINIMUM_RADIUS 16.0f

// 4x the old single-scene budget: shared across every scene drawn in one real frame (sky-portal, main, view-weapon -- see CG_RenderViewWeapon), not just one.
#define ENTITY_MAX_VERTS ( 65536 * 4 )
#define ENTITY_MAX_INDEXES ( 262144 * 4 )

#define MDC_DIST_SCALE 0.05f
#define MDC_MAX_OFS 127.0f

#define SKIN_SCRATCH_VERTS 40000
static vec3_t s_skinScratch[SKIN_SCRATCH_VERTS];

typedef struct {
	float pos[3];
	float uv[2];
	float localPos[3];
	float normal[3];
} entityBaseVert_t;
static entityBaseVert_t s_baseScratch[SKIN_SCRATCH_VERTS];

// RF_DEPTHHACK: same minDepth/maxDepth values as code/renderer's glDepthRange(0, 0.3) weapon hack.
static void SetDepthRange( qboolean weaponHack ) {
	int width, height;
	VkViewport viewport;

	R_GetWinResolution( &width, &height );
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)width;
	viewport.height = (float)height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = weaponHack ? 0.3f : 1.0f;
	qvkCmdSetViewport( vk.command_buffer, 0, 1, &viewport );
}

static VkBuffer s_indexBuffer;
static VkDeviceMemory s_indexMemory;
static uint32_t *s_indexMapped;

// Dynamic (color/uv only) buffers for the GPU vertex-lerp path (MD3/MDC, see vk_entity_register_model); position/normal/tangent come from each model's static per-surface buffer instead. Indices are plain integers agnostic to which vertex buffer they pair with, so this path reuses s_indexBuffer/s_idxWrite.
static VkBuffer s_lerpVertexBuffer;
static VkDeviceMemory s_lerpVertexMemory;
static vkEntityDynamicVert_t *s_lerpVertexMapped;

static VkBuffer s_lerpMtVertexBuffer;
static VkDeviceMemory s_lerpMtVertexMemory;
static vkEntityDynamicMTVert_t *s_lerpMtVertexMapped;

static uint32_t s_lerpVertWrite;
static uint32_t s_lerpMtVertWrite;
static uint32_t s_lerpVertRegionBase;
static uint32_t s_lerpMtVertRegionBase;

// Bone-matrix UBO for the GPU bone-skinning path (MDS/MDM): one MDX_MAX_BONES-sized mat4 array per skinned entity drawn this frame (bones are shared across all of one entity's surfaces, so this is written once per entity, not per surface -- see WriteBoneMatricesMDS/MDM). s_skinDescriptorSet is the one persistent descriptor set pointing at the whole buffer; each draw rebinds it with a different dynamic offset instead of allocating a set per entity.
#define MAX_SKINNED_ENTITY_DRAWS 256
static VkBuffer s_boneUboBuffer;
static VkDeviceMemory s_boneUboMemory;
static byte *s_boneUboMapped;
static VkDeviceSize s_boneUboStride; // MDX_MAX_BONES * sizeof(mat4), rounded up to the device's minUniformBufferOffsetAlignment
static VkDescriptorPool s_skinDescriptorPool;
static VkDescriptorSet s_skinDescriptorSet;
static uint32_t s_boneUboWrite;
static VkDeviceSize s_boneUboRegionBase;

// Shared index write cursor into s_indexMapped (plain integers, agnostic to which vertex buffer they pair with -- both the lerp and skin immediate-draw paths share it); persists across every vk_entity_draw_all call this frame, reset only in vk_entity_begin_frame.
static uint32_t s_idxWrite;

// This frame's region within each (now VK_FRAMES_IN_FLIGHT-sized) buffer; the write cursors above stay region-relative, added in at write/bind time.
static uint32_t s_idxRegionBase;

void vk_entity_begin_frame( void ) {
	s_idxWrite = 0;
	s_lerpVertWrite = 0;
	s_lerpMtVertWrite = 0;
	s_lerpVertRegionBase = vk_current_frame_slot() * (uint32_t)ENTITY_MAX_VERTS;
	s_lerpMtVertRegionBase = vk_current_frame_slot() * (uint32_t)ENTITY_MAX_VERTS;
	s_idxRegionBase = vk_current_frame_slot() * (uint32_t)ENTITY_MAX_INDEXES;
	s_boneUboWrite = 0;
	s_boneUboRegionBase = (VkDeviceSize)vk_current_frame_slot() * MAX_SKINNED_ENTITY_DRAWS * s_boneUboStride;
}

extern uint32_t vk_find_memory_type( uint32_t type_bits, VkMemoryPropertyFlags properties );

static void create_mapped_buffer( VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer *outBuffer, VkDeviceMemory *outMemory, void **outMapped ) {
	VkBufferCreateInfo buf_desc;
	buf_desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buf_desc.pNext = NULL;
	buf_desc.flags = 0;
	buf_desc.size = size;
	buf_desc.usage = usage;
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

	VK_CHECK( qvkMapMemory( vk.device, *outMemory, 0, VK_WHOLE_SIZE, 0, outMapped ) );
}

static void destroy_mapped_buffer( VkBuffer buffer, VkDeviceMemory memory, void *mapped ) {
	if ( mapped ) {
		qvkUnmapMemory( vk.device, memory );
	}
	qvkDestroyBuffer( vk.device, buffer, NULL );
	qvkFreeMemory( vk.device, memory, NULL );
}

static void FreeModelAnimData( model_t *mod ); // defined below, alongside vk_entity_register_model

void vk_entity_init( void ) {
	create_mapped_buffer( (VkDeviceSize)ENTITY_MAX_INDEXES * VK_FRAMES_IN_FLIGHT * sizeof( uint32_t ), VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		&s_indexBuffer, &s_indexMemory, (void **)&s_indexMapped );
	create_mapped_buffer( (VkDeviceSize)ENTITY_MAX_VERTS * VK_FRAMES_IN_FLIGHT * sizeof( vkEntityDynamicVert_t ), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		&s_lerpVertexBuffer, &s_lerpVertexMemory, (void **)&s_lerpVertexMapped );
	create_mapped_buffer( (VkDeviceSize)ENTITY_MAX_VERTS * VK_FRAMES_IN_FLIGHT * sizeof( vkEntityDynamicMTVert_t ), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		&s_lerpMtVertexBuffer, &s_lerpMtVertexMemory, (void **)&s_lerpMtVertexMapped );

	{
		VkDeviceSize rawSize = (VkDeviceSize)MDX_MAX_BONES * 16 * sizeof( float );
		VkDeviceSize align = vk.deviceProperties.limits.minUniformBufferOffsetAlignment;
		VkDescriptorPoolSize poolSize;
		VkDescriptorPoolCreateInfo poolDesc;
		VkDescriptorSetAllocateInfo allocInfo;
		VkDescriptorBufferInfo bufInfo;
		VkWriteDescriptorSet write;

		s_boneUboStride = align > 0 ? ( ( rawSize + align - 1 ) / align ) * align : rawSize;

		create_mapped_buffer( s_boneUboStride * MAX_SKINNED_ENTITY_DRAWS * VK_FRAMES_IN_FLIGHT, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			&s_boneUboBuffer, &s_boneUboMemory, (void **)&s_boneUboMapped );

		poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		poolSize.descriptorCount = 1;
		poolDesc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolDesc.pNext = NULL;
		poolDesc.flags = 0;
		poolDesc.maxSets = 1;
		poolDesc.poolSizeCount = 1;
		poolDesc.pPoolSizes = &poolSize;
		VK_CHECK( qvkCreateDescriptorPool( vk.device, &poolDesc, NULL, &s_skinDescriptorPool ) );

		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.pNext = NULL;
		allocInfo.descriptorPool = s_skinDescriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &vk.skin_set_layout;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &allocInfo, &s_skinDescriptorSet ) );

		bufInfo.buffer = s_boneUboBuffer;
		bufInfo.offset = 0;
		bufInfo.range = s_boneUboStride; // the dynamic offset at bind time selects which slot's worth of bytes this covers

		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.pNext = NULL;
		write.dstSet = s_skinDescriptorSet;
		write.dstBinding = 0;
		write.dstArrayElement = 0;
		write.descriptorCount = 1;
		write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		write.pImageInfo = NULL;
		write.pBufferInfo = &bufInfo;
		write.pTexelBufferView = NULL;
		qvkUpdateDescriptorSets( vk.device, 1, &write, 0, NULL );
	}
}

void vk_entity_shutdown( void ) {
	int h, numModels = R_GetNumModels();
	for ( h = 1; h < numModels; h++ ) {
		model_t *mod = R_GetModelByHandle( h );
		if ( mod ) {
			FreeModelAnimData( mod );
		}
	}

	destroy_mapped_buffer( s_indexBuffer, s_indexMemory, s_indexMapped );
	s_indexMapped = NULL;
	destroy_mapped_buffer( s_lerpVertexBuffer, s_lerpVertexMemory, s_lerpVertexMapped );
	s_lerpVertexMapped = NULL;
	destroy_mapped_buffer( s_lerpMtVertexBuffer, s_lerpMtVertexMemory, s_lerpMtVertexMapped );
	s_lerpMtVertexMapped = NULL;
	qvkDestroyDescriptorPool( vk.device, s_skinDescriptorPool, NULL ); // frees s_skinDescriptorSet too
	destroy_mapped_buffer( s_boneUboBuffer, s_boneUboMemory, s_boneUboMapped );
	s_boneUboMapped = NULL;
}

qboolean vk_entity_has_mesh( const refEntity_t *ent ) {
	model_t *model = R_GetModelByHandle( ent->hModel );
	if ( !model ) {
		return qfalse;
	}
	return ( model->type == MOD_MD3 && model->md3 ) || ( model->type == MOD_MDC && model->mdc ) ||
		   ( model->type == MOD_MDS && model->mds ) || ( model->type == MOD_MDM && model->mdm ) ||
		   model->type == MOD_BRUSH;
}

static void DecodeMD3Normal( short packed, vec3_t out );


static void LerpSurfaceVerts( const refEntity_t *ent, const md3Surface_t *surf, int frame, int oldframe, float backlerp, entityBaseVert_t *out ) {
	const md3XyzNormal_t *base = (const md3XyzNormal_t *)( (const byte *)surf + surf->ofsXyzNormals );
	const md3XyzNormal_t *newXyz = base + (size_t)frame * surf->numVerts;
	const md3XyzNormal_t *oldXyz = base + (size_t)oldframe * surf->numVerts;
	const md3St_t *st = (const md3St_t *)( (const byte *)surf + surf->ofsSt );
	float frontlerp = 1.0f - backlerp;
	int i, k;

	for ( i = 0; i < surf->numVerts; i++ ) {
		float local[3];
		vec3_t newN, oldN;

		for ( k = 0; k < 3; k++ ) {
			local[k] = ( frontlerp * (float)newXyz[i].xyz[k] + backlerp * (float)oldXyz[i].xyz[k] ) * MD3_XYZ_SCALE;
		}

		out[i].pos[0] = ent->origin[0] + local[0] * ent->axis[0][0] + local[1] * ent->axis[1][0] + local[2] * ent->axis[2][0];
		out[i].pos[1] = ent->origin[1] + local[0] * ent->axis[0][1] + local[1] * ent->axis[1][1] + local[2] * ent->axis[2][1];
		out[i].pos[2] = ent->origin[2] + local[0] * ent->axis[0][2] + local[1] * ent->axis[1][2] + local[2] * ent->axis[2][2];

		out[i].uv[0] = st[i].st[0];
		out[i].uv[1] = st[i].st[1];

		out[i].localPos[0] = local[0]; out[i].localPos[1] = local[1]; out[i].localPos[2] = local[2];

		DecodeMD3Normal( newXyz[i].normal, newN );
		DecodeMD3Normal( oldXyz[i].normal, oldN );
		out[i].normal[0] = frontlerp * newN[0] + backlerp * oldN[0];
		out[i].normal[1] = frontlerp * newN[1] + backlerp * oldN[1];
		out[i].normal[2] = frontlerp * newN[2] + backlerp * oldN[2];
		VectorNormalize( out[i].normal );
	}
}

// Position only -- the compressed delta's normal byte is unused since entities aren't lit yet.
static void DecodeMDCVertex( const md3XyzNormal_t *base, const mdcXyzCompressed_t *comp, vec3_t out ) {
	out[0] = (float)base->xyz[0] * MD3_XYZ_SCALE;
	out[1] = (float)base->xyz[1] * MD3_XYZ_SCALE;
	out[2] = (float)base->xyz[2] * MD3_XYZ_SCALE;

	if ( comp ) {
		unsigned int ofsVec = comp->ofsVec;
		out[0] += ( (float)( ofsVec & 255 ) - MDC_MAX_OFS ) * MDC_DIST_SCALE;
		out[1] += ( (float)( ( ofsVec >> 8 ) & 255 ) - MDC_MAX_OFS ) * MDC_DIST_SCALE;
		out[2] += ( (float)( ( ofsVec >> 16 ) & 255 ) - MDC_MAX_OFS ) * MDC_DIST_SCALE;
	}
}

// Per-model GPU vertex-animation data (MD3/MDC frames; MDS/MDM bind pose is Stage 3), built once at load time by vk_entity_register_model and stored opaquely in model_t.vkAnimData.
typedef struct {
	VkBuffer buffer;
	VkDeviceMemory memory;
	uint32_t numVertsPerFrame;
} entitySurfAnimBuffer_t;

typedef struct {
	int numSurfaces;
	entitySurfAnimBuffer_t *surfaces;
} entityModelAnimData_t;

// float[3] in [-1,1] -> snorm int16[4], w left 0 (vk_world.c has its own copy of this same small helper -- not worth a shared header for 8 lines of math).
static void pack_entity_snorm16_3( const vec3_t in, int16_t out[4] ) {
	int i;
	for ( i = 0; i < 3; i++ ) {
		float v = in[i];
		if ( v < -1.0f ) v = -1.0f;
		if ( v > 1.0f ) v = 1.0f;
		out[i] = (int16_t)( v * 32767.0f );
	}
	out[3] = 0;
}

// vec4 (xyz tangent direction, w handedness sign already +-1) -> snorm int16[4].
static void pack_entity_snorm16_4( const vec4_t in, int16_t out[4] ) {
	int i;
	for ( i = 0; i < 4; i++ ) {
		float v = in[i];
		if ( v < -1.0f ) v = -1.0f;
		if ( v > 1.0f ) v = 1.0f;
		out[i] = (int16_t)( v * 32767.0f );
	}
}

// MD3's lat/long-packed normal; same formula as tr_light.c's (private) LatLongToNormal, applied to the model-format field instead of a light-grid sample.
static void DecodeMD3Normal( short packed, vec3_t out ) {
	float lat = (float)( ( (unsigned short)packed >> 8 ) & 0xff ) * ( 2.0f * (float)M_PI / 256.0f );
	float lng = (float)( (unsigned short)packed & 0xff ) * ( 2.0f * (float)M_PI / 256.0f );
	out[0] = cosf( lat ) * sinf( lng );
	out[1] = sinf( lat ) * sinf( lng );
	out[2] = cosf( lng );
}

// MDC compressed delta frames pack a coarser normal (one of bytedirs[]'s 162 directions) in ofsVec's top byte instead of repeating the base frame's lat/long one.
static void DecodeMDCNormal( const md3XyzNormal_t *base, const mdcXyzCompressed_t *comp, vec3_t out ) {
	if ( comp ) {
		ByteToDir( (int)( ( comp->ofsVec >> 24 ) & 0xff ), out );
	} else {
		DecodeMD3Normal( base->normal, out );
	}
}

// Standard per-triangle tangent accumulation (Lengyel's method) + Gram-Schmidt orthogonalization against the normal; handedness in outTangents[i][3] matches rend2's convention (its shader reconstructs bitangent as cross(normal, tangent) * handedness). One frame's worth at a time -- UVs/triangles are frame-invariant, only positions/normals vary, so the load-time buffer build calls this once per frame.
static void ComputeFrameTangents( const vec3_t *positions, const vec3_t *normals, const vec2_t *uvs,
									const md3Triangle_t *triangles, int numTriangles, int numVerts, vec4_t *outTangents ) {
	vec3_t *tan1 = calloc( (size_t)numVerts, sizeof( vec3_t ) );
	vec3_t *tan2 = calloc( (size_t)numVerts, sizeof( vec3_t ) );
	int i;

	for ( i = 0; i < numTriangles; i++ ) {
		int i0 = triangles[i].indexes[0], i1 = triangles[i].indexes[1], i2 = triangles[i].indexes[2];
		vec3_t edge1, edge2, sdir, tdir;
		float du1, dv1, du2, dv2, denom, r;

		VectorSubtract( positions[i1], positions[i0], edge1 );
		VectorSubtract( positions[i2], positions[i0], edge2 );
		du1 = uvs[i1][0] - uvs[i0][0]; dv1 = uvs[i1][1] - uvs[i0][1];
		du2 = uvs[i2][0] - uvs[i0][0]; dv2 = uvs[i2][1] - uvs[i0][1];

		denom = du1 * dv2 - du2 * dv1;
		r = ( denom != 0.0f ) ? ( 1.0f / denom ) : 0.0f;

		sdir[0] = ( dv2 * edge1[0] - dv1 * edge2[0] ) * r;
		sdir[1] = ( dv2 * edge1[1] - dv1 * edge2[1] ) * r;
		sdir[2] = ( dv2 * edge1[2] - dv1 * edge2[2] ) * r;
		tdir[0] = ( du1 * edge2[0] - du2 * edge1[0] ) * r;
		tdir[1] = ( du1 * edge2[1] - du2 * edge1[1] ) * r;
		tdir[2] = ( du1 * edge2[2] - du2 * edge1[2] ) * r;

		VectorAdd( tan1[i0], sdir, tan1[i0] ); VectorAdd( tan1[i1], sdir, tan1[i1] ); VectorAdd( tan1[i2], sdir, tan1[i2] );
		VectorAdd( tan2[i0], tdir, tan2[i0] ); VectorAdd( tan2[i1], tdir, tan2[i1] ); VectorAdd( tan2[i2], tdir, tan2[i2] );
	}

	for ( i = 0; i < numVerts; i++ ) {
		vec3_t t, crossNT;
		float d, len, handedness;

		VectorCopy( tan1[i], t );
		d = DotProduct( normals[i], t );
		VectorMA( t, -d, normals[i], t );
		len = VectorNormalize( t );

		if ( len < 1e-6f ) {
			// Degenerate (isolated vertex, or a UV-degenerate triangle fan) -- any tangent perpendicular to the normal works equally well.
			vec3_t arbitrary = { 1.0f, 0.0f, 0.0f };
			if ( fabsf( normals[i][0] ) > 0.9f ) {
				arbitrary[0] = 0.0f; arbitrary[1] = 1.0f; arbitrary[2] = 0.0f;
			}
			CrossProduct( normals[i], arbitrary, t );
			VectorNormalize( t );
		}

		CrossProduct( normals[i], t, crossNT );
		handedness = ( DotProduct( crossNT, tan2[i] ) < 0.0f ) ? -1.0f : 1.0f;

		outTangents[i][0] = t[0]; outTangents[i][1] = t[1]; outTangents[i][2] = t[2]; outTangents[i][3] = handedness;
	}

	free( tan1 );
	free( tan2 );
}

// Builds one MD3 surface's static GPU buffer: numFrames * numVerts entries, position/normal/tangent decoded once per frame (not per-draw).
static void BuildMD3SurfaceAnimBuffer( const md3Surface_t *surf, entitySurfAnimBuffer_t *out ) {
	int numVerts = surf->numVerts, numFrames = surf->numFrames;
	const md3XyzNormal_t *xyzBase = (const md3XyzNormal_t *)( (const byte *)surf + surf->ofsXyzNormals );
	const md3St_t *stSrc = (const md3St_t *)( (const byte *)surf + surf->ofsSt );
	const md3Triangle_t *tris = (const md3Triangle_t *)( (const byte *)surf + surf->ofsTriangles );
	vkEntityFrameVert_t *staging = malloc( (size_t)numVerts * (size_t)numFrames * sizeof( vkEntityFrameVert_t ) );
	vec3_t *positions = malloc( (size_t)numVerts * sizeof( vec3_t ) );
	vec3_t *normals = malloc( (size_t)numVerts * sizeof( vec3_t ) );
	vec4_t *tangents = malloc( (size_t)numVerts * sizeof( vec4_t ) );
	vec2_t *uvs = malloc( (size_t)numVerts * sizeof( vec2_t ) );
	int f, i;

	for ( i = 0; i < numVerts; i++ ) {
		uvs[i][0] = stSrc[i].st[0];
		uvs[i][1] = stSrc[i].st[1];
	}

	for ( f = 0; f < numFrames; f++ ) {
		const md3XyzNormal_t *xyz = xyzBase + (size_t)f * numVerts;

		for ( i = 0; i < numVerts; i++ ) {
			positions[i][0] = (float)xyz[i].xyz[0] * MD3_XYZ_SCALE;
			positions[i][1] = (float)xyz[i].xyz[1] * MD3_XYZ_SCALE;
			positions[i][2] = (float)xyz[i].xyz[2] * MD3_XYZ_SCALE;
			DecodeMD3Normal( xyz[i].normal, normals[i] );
		}

		ComputeFrameTangents( positions, normals, uvs, tris, surf->numTriangles, numVerts, tangents );

		for ( i = 0; i < numVerts; i++ ) {
			vkEntityFrameVert_t *dst = &staging[(size_t)f * numVerts + i];
			dst->pos[0] = positions[i][0]; dst->pos[1] = positions[i][1]; dst->pos[2] = positions[i][2];
			pack_entity_snorm16_3( normals[i], dst->normal );
			pack_entity_snorm16_4( tangents[i], dst->tangent );
		}
	}

	vk_create_device_local_buffer( staging, (VkDeviceSize)numVerts * numFrames * sizeof( vkEntityFrameVert_t ),
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &out->buffer, &out->memory );
	out->numVertsPerFrame = (uint32_t)numVerts;

	free( staging ); free( positions ); free( normals ); free( tangents ); free( uvs );
}

// Same as BuildMD3SurfaceAnimBuffer, but decodes each actual frame's base+optional-compressed-delta position/normal first (same math as DecodeMDCVertex/DecodeMDCNormal below), so the resulting buffer is dense (numFrames * numVerts) like MD3's -- no base/delta indirection needed at draw time.
static void BuildMDCSurfaceAnimBuffer( const mdcSurface_t *surf, int numFrames, entitySurfAnimBuffer_t *out ) {
	int numVerts = surf->numVerts;
	const short *frameBaseFrames = (const short *)( (const byte *)surf + surf->ofsFrameBaseFrames );
	const short *frameCompFrames = (const short *)( (const byte *)surf + surf->ofsFrameCompFrames );
	const md3XyzNormal_t *baseArray = (const md3XyzNormal_t *)( (const byte *)surf + surf->ofsXyzNormals );
	const mdcXyzCompressed_t *compArray = (const mdcXyzCompressed_t *)( (const byte *)surf + surf->ofsXyzCompressed );
	const md3St_t *stSrc = (const md3St_t *)( (const byte *)surf + surf->ofsSt );
	const md3Triangle_t *tris = (const md3Triangle_t *)( (const byte *)surf + surf->ofsTriangles );
	qboolean hasComp = surf->numCompFrames > 0;
	vkEntityFrameVert_t *staging = malloc( (size_t)numVerts * (size_t)numFrames * sizeof( vkEntityFrameVert_t ) );
	vec3_t *positions = malloc( (size_t)numVerts * sizeof( vec3_t ) );
	vec3_t *normals = malloc( (size_t)numVerts * sizeof( vec3_t ) );
	vec4_t *tangents = malloc( (size_t)numVerts * sizeof( vec4_t ) );
	vec2_t *uvs = malloc( (size_t)numVerts * sizeof( vec2_t ) );
	int f, i;

	for ( i = 0; i < numVerts; i++ ) {
		uvs[i][0] = stSrc[i].st[0];
		uvs[i][1] = stSrc[i].st[1];
	}

	for ( f = 0; f < numFrames; f++ ) {
		int base = frameBaseFrames[f];
		int comp = hasComp ? frameCompFrames[f] : -1;

		for ( i = 0; i < numVerts; i++ ) {
			const md3XyzNormal_t *baseVert = &baseArray[(size_t)base * numVerts + i];
			const mdcXyzCompressed_t *compVert = comp >= 0 ? &compArray[(size_t)comp * numVerts + i] : NULL;
			DecodeMDCVertex( baseVert, compVert, positions[i] );
			DecodeMDCNormal( baseVert, compVert, normals[i] );
		}

		ComputeFrameTangents( positions, normals, uvs, tris, surf->numTriangles, numVerts, tangents );

		for ( i = 0; i < numVerts; i++ ) {
			vkEntityFrameVert_t *dst = &staging[(size_t)f * numVerts + i];
			dst->pos[0] = positions[i][0]; dst->pos[1] = positions[i][1]; dst->pos[2] = positions[i][2];
			pack_entity_snorm16_3( normals[i], dst->normal );
			pack_entity_snorm16_4( tangents[i], dst->tangent );
		}
	}

	vk_create_device_local_buffer( staging, (VkDeviceSize)numVerts * numFrames * sizeof( vkEntityFrameVert_t ),
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &out->buffer, &out->memory );
	out->numVertsPerFrame = (uint32_t)numVerts;

	free( staging ); free( positions ); free( normals ); free( tangents ); free( uvs );
}

// A real asset's per-vertex weight count rarely exceeds this; if it ever did, ReduceBoneWeights would just see a truncated list (extra influences past this cap silently ignored, not just deprioritized).
#define MAX_VERTEX_WEIGHTS_SCRATCH 16

// Reduces a variable-length (boneIndex, boneWeight, offset) list to the top 4 by weight magnitude, renormalized to sum 255 (unorm8) -- standard GPU-skinning reduction, applied per-slot here since offset (unlike a single shared bind-pose position) genuinely differs per bone influence, see vkEntitySkinnedVert_t's comment. Real MDS/MDM content rarely exceeds 4 influences per vertex; when it does, the smallest contributions are the ones dropped.
static void ReduceBoneWeights( const int *boneIndex, const float *boneWeight, const vec3_t *offset, int numWeights,
								 byte outIndex[4], byte outWeight[4], float outOffset[4][3] ) {
	int top[4] = { 0, 0, 0, 0 };
	float topW[4] = { -1.0f, -1.0f, -1.0f, -1.0f };
	int i, k;
	float sum;

	for ( i = 0; i < numWeights; i++ ) {
		float w = boneWeight[i];
		for ( k = 0; k < 4; k++ ) {
			if ( w > topW[k] ) {
				int j;
				for ( j = 3; j > k; j-- ) {
					topW[j] = topW[j - 1]; top[j] = top[j - 1];
				}
				topW[k] = w; top[k] = i;
				break;
			}
		}
	}

	sum = 0.0f;
	for ( k = 0; k < 4; k++ ) {
		if ( topW[k] < 0.0f ) {
			topW[k] = 0.0f;
		}
		sum += topW[k];
	}
	if ( sum <= 0.0f ) {
		sum = 1.0f;
	}

	for ( k = 0; k < 4; k++ ) {
		if ( topW[k] > 0.0f ) {
			int srcIdx = top[k];
			outIndex[k] = (byte)boneIndex[srcIdx];
			outOffset[k][0] = offset[srcIdx][0]; outOffset[k][1] = offset[srcIdx][1]; outOffset[k][2] = offset[srcIdx][2];
		} else {
			outIndex[k] = 0;
			outOffset[k][0] = outOffset[k][1] = outOffset[k][2] = 0.0f;
		}
		outWeight[k] = (byte)( ( topW[k] / sum ) * 255.0f + 0.5f );
	}
}

// Builds one MDS surface's static GPU buffer. bindPositions (fed only to ComputeFrameTangents, for a geometrically reasonable tangent direction) is the same weight*offset sum R_SkinMDMSurface's own "no skeleton linked yet" fallback already treats as this format's bind pose -- not the actual per-vertex GPU data, which needs each weight slot's own offset (see vkEntitySkinnedVert_t).
static void BuildMDSSurfaceSkinBuffer( const mdsSurface_t *surf, entitySurfAnimBuffer_t *out ) {
	int numVerts = surf->numVerts;
	const mdsVertex_t *vSrc = (const mdsVertex_t *)( (const byte *)surf + surf->ofsVerts );
	const mdsTriangle_t *tris = (const mdsTriangle_t *)( (const byte *)surf + surf->ofsTriangles );
	vkEntitySkinnedVert_t *staging = malloc( (size_t)numVerts * sizeof( vkEntitySkinnedVert_t ) );
	vec3_t *bindPositions = malloc( (size_t)numVerts * sizeof( vec3_t ) );
	vec3_t *normals = malloc( (size_t)numVerts * sizeof( vec3_t ) );
	vec2_t *uvs = malloc( (size_t)numVerts * sizeof( vec2_t ) );
	vec4_t *tangents = malloc( (size_t)numVerts * sizeof( vec4_t ) );
	const mdsVertex_t *v;
	int i;

	v = vSrc;
	for ( i = 0; i < numVerts; i++ ) {
		int k;
		VectorCopy( v->normal, normals[i] );
		uvs[i][0] = v->texCoords[0]; uvs[i][1] = v->texCoords[1];
		VectorClear( bindPositions[i] );
		for ( k = 0; k < v->numWeights; k++ ) {
			VectorMA( bindPositions[i], v->weights[k].boneWeight, v->weights[k].offset, bindPositions[i] );
		}
		v = (const mdsVertex_t *)&v->weights[v->numWeights];
	}

	ComputeFrameTangents( bindPositions, normals, uvs, (const md3Triangle_t *)tris, surf->numTriangles, numVerts, tangents );

	v = vSrc;
	for ( i = 0; i < numVerts; i++ ) {
		vkEntitySkinnedVert_t *dst = &staging[i];
		int boneIdx[MAX_VERTEX_WEIGHTS_SCRATCH];
		float boneW[MAX_VERTEX_WEIGHTS_SCRATCH];
		vec3_t boneOff[MAX_VERTEX_WEIGHTS_SCRATCH];
		int numW = v->numWeights < MAX_VERTEX_WEIGHTS_SCRATCH ? v->numWeights : MAX_VERTEX_WEIGHTS_SCRATCH;
		float outOffset[4][3];
		int k;

		for ( k = 0; k < numW; k++ ) {
			boneIdx[k] = v->weights[k].boneIndex;
			boneW[k] = v->weights[k].boneWeight;
			VectorCopy( v->weights[k].offset, boneOff[k] );
		}

		pack_entity_snorm16_3( normals[i], dst->normal );
		pack_entity_snorm16_4( tangents[i], dst->tangent );
		ReduceBoneWeights( boneIdx, boneW, boneOff, numW, dst->boneIndex, dst->boneWeight, outOffset );
		for ( k = 0; k < 4; k++ ) {
			dst->offset[k][0] = outOffset[k][0]; dst->offset[k][1] = outOffset[k][1]; dst->offset[k][2] = outOffset[k][2];
		}

		v = (const mdsVertex_t *)&v->weights[v->numWeights];
	}

	vk_create_device_local_buffer( staging, (VkDeviceSize)numVerts * sizeof( vkEntitySkinnedVert_t ),
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &out->buffer, &out->memory );
	out->numVertsPerFrame = (uint32_t)numVerts; // naming is vkEntityFrameVert_t's; skinned data has just one entry per vertex, reusing the same wrapper struct

	free( staging ); free( bindPositions ); free( normals ); free( uvs ); free( tangents );
}

// Same as BuildMDSSurfaceSkinBuffer but for MDM's mdmVertex_t/mdmWeight_t/mdmTriangle_t (identical layout, distinct types -- same duplication LerpMDSSurfaceVerts/LerpMDMSurfaceVerts already use elsewhere in this file).
static void BuildMDMSurfaceSkinBuffer( const mdmSurface_t *surf, entitySurfAnimBuffer_t *out ) {
	int numVerts = surf->numVerts;
	const mdmVertex_t *vSrc = (const mdmVertex_t *)( (const byte *)surf + surf->ofsVerts );
	const mdmTriangle_t *tris = (const mdmTriangle_t *)( (const byte *)surf + surf->ofsTriangles );
	vkEntitySkinnedVert_t *staging = malloc( (size_t)numVerts * sizeof( vkEntitySkinnedVert_t ) );
	vec3_t *bindPositions = malloc( (size_t)numVerts * sizeof( vec3_t ) );
	vec3_t *normals = malloc( (size_t)numVerts * sizeof( vec3_t ) );
	vec2_t *uvs = malloc( (size_t)numVerts * sizeof( vec2_t ) );
	vec4_t *tangents = malloc( (size_t)numVerts * sizeof( vec4_t ) );
	const mdmVertex_t *v;
	int i;

	v = vSrc;
	for ( i = 0; i < numVerts; i++ ) {
		int k;
		VectorCopy( v->normal, normals[i] );
		uvs[i][0] = v->texCoords[0]; uvs[i][1] = v->texCoords[1];
		VectorClear( bindPositions[i] );
		for ( k = 0; k < v->numWeights; k++ ) {
			VectorMA( bindPositions[i], v->weights[k].boneWeight, v->weights[k].offset, bindPositions[i] );
		}
		v = (const mdmVertex_t *)&v->weights[v->numWeights];
	}

	ComputeFrameTangents( bindPositions, normals, uvs, (const md3Triangle_t *)tris, surf->numTriangles, numVerts, tangents );

	v = vSrc;
	for ( i = 0; i < numVerts; i++ ) {
		vkEntitySkinnedVert_t *dst = &staging[i];
		int boneIdx[MAX_VERTEX_WEIGHTS_SCRATCH];
		float boneW[MAX_VERTEX_WEIGHTS_SCRATCH];
		vec3_t boneOff[MAX_VERTEX_WEIGHTS_SCRATCH];
		int numW = v->numWeights < MAX_VERTEX_WEIGHTS_SCRATCH ? v->numWeights : MAX_VERTEX_WEIGHTS_SCRATCH;
		float outOffset[4][3];
		int k;

		for ( k = 0; k < numW; k++ ) {
			boneIdx[k] = v->weights[k].boneIndex;
			boneW[k] = v->weights[k].boneWeight;
			VectorCopy( v->weights[k].offset, boneOff[k] );
		}

		pack_entity_snorm16_3( normals[i], dst->normal );
		pack_entity_snorm16_4( tangents[i], dst->tangent );
		ReduceBoneWeights( boneIdx, boneW, boneOff, numW, dst->boneIndex, dst->boneWeight, outOffset );
		for ( k = 0; k < 4; k++ ) {
			dst->offset[k][0] = outOffset[k][0]; dst->offset[k][1] = outOffset[k][1]; dst->offset[k][2] = outOffset[k][2];
		}

		v = (const mdmVertex_t *)&v->weights[v->numWeights];
	}

	vk_create_device_local_buffer( staging, (VkDeviceSize)numVerts * sizeof( vkEntitySkinnedVert_t ),
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &out->buffer, &out->memory );
	out->numVertsPerFrame = (uint32_t)numVerts;

	free( staging ); free( bindPositions ); free( normals ); free( uvs ); free( tangents );
}

void vk_entity_register_model( model_t *mod ) {
	entityModelAnimData_t *animData;
	int numSurfaces, s;

	if ( mod->type == MOD_MD3 && mod->md3 ) {
		numSurfaces = mod->md3->numSurfaces;
	} else if ( mod->type == MOD_MDC && mod->mdc ) {
		numSurfaces = mod->mdc->numSurfaces;
	} else if ( mod->type == MOD_MDS && mod->mds ) {
		numSurfaces = mod->mds->numSurfaces;
	} else if ( mod->type == MOD_MDM && mod->mdm ) {
		numSurfaces = mod->mdm->numSurfaces;
	} else {
		return; // everything else (MOD_MDX, MOD_BRUSH, MOD_OTHER) has no per-frame mesh data this applies to
	}
	if ( numSurfaces <= 0 ) {
		return;
	}

	animData = ri.Hunk_Alloc( sizeof( entityModelAnimData_t ), h_low );
	animData->numSurfaces = numSurfaces;
	animData->surfaces = ri.Hunk_Alloc( (size_t)numSurfaces * sizeof( entitySurfAnimBuffer_t ), h_low );

	if ( mod->type == MOD_MD3 ) {
		const md3Surface_t *surf = (const md3Surface_t *)( (const byte *)mod->md3 + mod->md3->ofsSurfaces );
		for ( s = 0; s < numSurfaces; s++ ) {
			BuildMD3SurfaceAnimBuffer( surf, &animData->surfaces[s] );
			surf = (const md3Surface_t *)( (const byte *)surf + surf->ofsEnd );
		}
	} else if ( mod->type == MOD_MDC ) {
		const mdcSurface_t *surf = (const mdcSurface_t *)( (const byte *)mod->mdc + mod->mdc->ofsSurfaces );
		for ( s = 0; s < numSurfaces; s++ ) {
			BuildMDCSurfaceAnimBuffer( surf, mod->mdc->numFrames, &animData->surfaces[s] );
			surf = (const mdcSurface_t *)( (const byte *)surf + surf->ofsEnd );
		}
	} else if ( mod->type == MOD_MDS ) {
		const mdsSurface_t *surf = (const mdsSurface_t *)( (const byte *)mod->mds + mod->mds->ofsSurfaces );
		for ( s = 0; s < numSurfaces; s++ ) {
			BuildMDSSurfaceSkinBuffer( surf, &animData->surfaces[s] );
			surf = (const mdsSurface_t *)( (const byte *)surf + surf->ofsEnd );
		}
	} else {
		const mdmSurface_t *surf = (const mdmSurface_t *)( (const byte *)mod->mdm + mod->mdm->ofsSurfaces );
		for ( s = 0; s < numSurfaces; s++ ) {
			BuildMDMSurfaceSkinBuffer( surf, &animData->surfaces[s] );
			surf = (const mdmSurface_t *)( (const byte *)surf + surf->ofsEnd );
		}
	}

	mod->vkAnimData = animData;
}

// Frees every registered model's GPU anim buffer; called from vk_entity_shutdown before R_InitModels wipes model_t (there's no finer-grained per-model free in this engine -- see the plan's research notes).
static void FreeModelAnimData( model_t *mod ) {
	entityModelAnimData_t *animData = (entityModelAnimData_t *)mod->vkAnimData;
	int s;

	if ( !animData ) {
		return;
	}
	for ( s = 0; s < animData->numSurfaces; s++ ) {
		qvkDestroyBuffer( vk.device, animData->surfaces[s].buffer, NULL );
		qvkFreeMemory( vk.device, animData->surfaces[s].memory, NULL );
	}
	mod->vkAnimData = NULL; // the hunk memory itself isn't freed, matching every other model allocation
}

// Each actual frame maps to a base frame (full position) plus an optional compressed delta against it.
static void LerpMDCSurfaceVerts( const refEntity_t *ent, const mdcSurface_t *surf, int frame, int oldframe, float backlerp, entityBaseVert_t *out ) {
	const short *frameBaseFrames = (const short *)( (const byte *)surf + surf->ofsFrameBaseFrames );
	const short *frameCompFrames = (const short *)( (const byte *)surf + surf->ofsFrameCompFrames );
	const md3XyzNormal_t *baseArray = (const md3XyzNormal_t *)( (const byte *)surf + surf->ofsXyzNormals );
	const mdcXyzCompressed_t *compArray = (const mdcXyzCompressed_t *)( (const byte *)surf + surf->ofsXyzCompressed );
	const md3St_t *st = (const md3St_t *)( (const byte *)surf + surf->ofsSt );
	qboolean hasComp = surf->numCompFrames > 0;
	int newBase = frameBaseFrames[frame];
	int oldBase = frameBaseFrames[oldframe];
	int newComp = hasComp ? frameCompFrames[frame] : -1;
	int oldComp = hasComp ? frameCompFrames[oldframe] : -1;
	float frontlerp = 1.0f - backlerp;
	int i;

	for ( i = 0; i < surf->numVerts; i++ ) {
		vec3_t newXyz, oldXyz, local, newN, oldN;
		const mdcXyzCompressed_t *newCompVert = newComp >= 0 ? &compArray[(size_t)newComp * surf->numVerts + i] : NULL;
		const mdcXyzCompressed_t *oldCompVert = oldComp >= 0 ? &compArray[(size_t)oldComp * surf->numVerts + i] : NULL;
		const md3XyzNormal_t *newBaseVert = &baseArray[(size_t)newBase * surf->numVerts + i];
		const md3XyzNormal_t *oldBaseVert = &baseArray[(size_t)oldBase * surf->numVerts + i];

		DecodeMDCVertex( newBaseVert, newCompVert, newXyz );
		DecodeMDCVertex( oldBaseVert, oldCompVert, oldXyz );

		local[0] = frontlerp * newXyz[0] + backlerp * oldXyz[0];
		local[1] = frontlerp * newXyz[1] + backlerp * oldXyz[1];
		local[2] = frontlerp * newXyz[2] + backlerp * oldXyz[2];

		out[i].pos[0] = ent->origin[0] + local[0] * ent->axis[0][0] + local[1] * ent->axis[1][0] + local[2] * ent->axis[2][0];
		out[i].pos[1] = ent->origin[1] + local[0] * ent->axis[0][1] + local[1] * ent->axis[1][1] + local[2] * ent->axis[2][1];
		out[i].pos[2] = ent->origin[2] + local[0] * ent->axis[0][2] + local[1] * ent->axis[1][2] + local[2] * ent->axis[2][2];

		out[i].uv[0] = st[i].st[0];
		out[i].uv[1] = st[i].st[1];

		out[i].localPos[0] = local[0]; out[i].localPos[1] = local[1]; out[i].localPos[2] = local[2];

		DecodeMDCNormal( newBaseVert, newCompVert, newN );
		DecodeMDCNormal( oldBaseVert, oldCompVert, oldN );
		out[i].normal[0] = frontlerp * newN[0] + backlerp * oldN[0];
		out[i].normal[1] = frontlerp * newN[1] + backlerp * oldN[1];
		out[i].normal[2] = frontlerp * newN[2] + backlerp * oldN[2];
		VectorNormalize( out[i].normal );
	}
}

// Skins into s_skinScratch (model-local), then places by the entity's axis/origin like the MD3/MDC frame-morph verts.
static void LerpMDSSurfaceVerts( const refEntity_t *ent, const mdsHeader_t *mds, const mdsSurface_t *surf, entityBaseVert_t *out ) {
	const mdsVertex_t *v = (const mdsVertex_t *)( (const byte *)surf + surf->ofsVerts );
	int i;

	R_SkinMDSSurface( mds, surf, ent, s_skinScratch );

	for ( i = 0; i < surf->numVerts; i++ ) {
		const float *local = s_skinScratch[i];

		out[i].pos[0] = ent->origin[0] + local[0] * ent->axis[0][0] + local[1] * ent->axis[1][0] + local[2] * ent->axis[2][0];
		out[i].pos[1] = ent->origin[1] + local[0] * ent->axis[0][1] + local[1] * ent->axis[1][1] + local[2] * ent->axis[2][1];
		out[i].pos[2] = ent->origin[2] + local[0] * ent->axis[0][2] + local[1] * ent->axis[1][2] + local[2] * ent->axis[2][2];

		out[i].uv[0] = v->texCoords[0];
		out[i].uv[1] = v->texCoords[1];

		v = (const mdsVertex_t *)&v->weights[v->numWeights];
	}
}

static void LerpMDMSurfaceVerts( const refEntity_t *ent, const mdmSurface_t *surf, entityBaseVert_t *out ) {
	const mdmVertex_t *v = (const mdmVertex_t *)( (const byte *)surf + surf->ofsVerts );
	int i;

	R_SkinMDMSurface( surf, ent, s_skinScratch );

	for ( i = 0; i < surf->numVerts; i++ ) {
		const float *local = s_skinScratch[i];

		out[i].pos[0] = ent->origin[0] + local[0] * ent->axis[0][0] + local[1] * ent->axis[1][0] + local[2] * ent->axis[2][0];
		out[i].pos[1] = ent->origin[1] + local[0] * ent->axis[0][1] + local[1] * ent->axis[1][1] + local[2] * ent->axis[2][1];
		out[i].pos[2] = ent->origin[2] + local[0] * ent->axis[0][2] + local[1] * ent->axis[1][2] + local[2] * ent->axis[2][2];

		out[i].uv[0] = v->texCoords[0];
		out[i].uv[1] = v->texCoords[1];

		v = (const mdmVertex_t *)&v->weights[v->numWeights];
	}
}

// Per-entity ambient/directed light + direction from the light grid plus dynamic lights, for the GPU per-vertex diffuse lighting CGEN_LIGHTING_DIFFUSE stages now use (entity_lerp.vert/entity_skin.vert compute ambient + max(dot(normal,lightDir),0)*directed, matching code/renderer's RB_CalcDiffuseColor) -- replaces the old flat "same tint for every vertex" approximation. Values are 0-255-ish scale, not pre-clamped; lightDir is world-space, normalized.
static void ComputeEntityLighting( const refEntity_t *ent, vec3_t outAmbient, vec3_t outDirected, vec3_t outLightDir ) {
	vec3_t origin;
	qboolean hasGrid;
	const dlight_t *dlights;
	int numDlights, i;

	if ( ent->renderfx & RF_LIGHTING_ORIGIN ) {
		VectorCopy( ent->lightingOrigin, origin );
	} else {
		VectorCopy( ent->origin, origin );
	}

	hasGrid = R_LightForPoint( origin, outAmbient, outDirected, outLightDir );
	dlights = R_GetSceneDlights( &numDlights );

	if ( !hasGrid && numDlights == 0 ) {
		VectorSet( outAmbient, 150.0f, 150.0f, 150.0f );
		VectorSet( outDirected, 150.0f, 150.0f, 150.0f );
		return;
	}

	if ( ent->renderfx & RF_MINLIGHT ) {
		outAmbient[0] += 32.0f;
		outAmbient[1] += 32.0f;
		outAmbient[2] += 32.0f;
	}

	for ( i = 0; i < numDlights; i++ ) {
		vec3_t dir;
		float dist, power, d;

		VectorSubtract( dlights[i].origin, origin, dir );
		dist = VectorNormalize( dir );
		power = DLIGHT_AT_RADIUS * ( dlights[i].radius * dlights[i].radius );
		if ( dist < DLIGHT_MINIMUM_RADIUS ) {
			dist = DLIGHT_MINIMUM_RADIUS;
		}
		d = power / ( dist * dist );
		VectorMA( outDirected, d, dlights[i].color, outDirected );
	}
}

// customShader overrides every surface; customSkin looks the surface name up in a .skin file; otherwise each surface uses its own baked-in shader.
static qhandle_t ResolveShaderDirect( qhandle_t customShader, qhandle_t customSkin, const char *surfaceName, qhandle_t bakedShader ) {
	if ( customShader ) {
		return customShader;
	}
	if ( customSkin ) {
		return R_SkinShaderForSurface( R_GetSkinByHandle( customSkin ), surfaceName, 0 );
	}
	return bakedShader;
}

static qhandle_t ResolveShader( qhandle_t customShader, qhandle_t customSkin, const char *surfaceName, int numShaders, const void *shadersPtr ) {
	qhandle_t baked = ( numShaders > 0 ) ? (qhandle_t)( (const md3Shader_t *)shadersPtr )->shaderIndex : 0;
	return ResolveShaderDirect( customShader, customSkin, surfaceName, baked );
}

static void WriteIndices( int numTriangles, const void *trianglesPtr, uint32_t idxWrite ) {
	const md3Triangle_t *tri = (const md3Triangle_t *)trianglesPtr;
	int i;

	for ( i = 0; i < numTriangles; i++, tri++ ) {
		s_indexMapped[idxWrite + (uint32_t)i * 3 + 0] = (uint32_t)tri->indexes[0];
		s_indexMapped[idxWrite + (uint32_t)i * 3 + 1] = (uint32_t)tri->indexes[1];
		s_indexMapped[idxWrite + (uint32_t)i * 3 + 2] = (uint32_t)tri->indexes[2];
	}
}

// GPU vertex-lerp path (MD3/MDC, see vk_entity_register_model): position/normal/tangent come from the model's static per-surface buffer, so only color/uv are written here into the smaller vkEntityDynamicVert_t/vkEntityDynamicMTVert_t buffers -- deformVertexes/rgbGen/alphaGen/tcMod are still evaluated fresh every frame, same as every other per-frame-expanded system in this renderer (vk_world.c/vk_poly.c).
static qboolean ReserveLerpSurface( int numVerts, int numStages, int numTriangles, uint32_t vertWrite, uint32_t idxWrite ) {
	return vertWrite + (uint32_t)numVerts * (uint32_t)numStages <= ENTITY_MAX_VERTS &&
		   idxWrite + (uint32_t)numTriangles * 3 <= ENTITY_MAX_INDEXES;
}

static void CalcStageBaseST( const shaderStage_t *stage, const entityBaseVert_t *v, const float *localViewOrigin, float outBaseST[2] ) {
	if ( stage->tcGen == TCGEN_ENVIRONMENT && localViewOrigin ) {
		R_CalcEnvironmentTexCoord( v->normal, localViewOrigin, v->localPos, outBaseST );
	} else {
		outBaseST[0] = v->uv[0];
		outBaseST[1] = v->uv[1];
	}
}

static void WriteLerpStageVertices( const shader_t *shader, const byte *entityColor, const float *shaderTexCoord,
									  const float *localViewOrigin, int numVerts, const entityBaseVert_t *base, vkEntityDynamicVert_t *out ) {
	int v, s;
	static const byte baseColor[4] = { 255, 255, 255, 255 };

	for ( v = 0; v < numVerts; v++ ) {
		float deformed[3];
		R_DeformPosition( shader, base[v].pos, NULL, 0.0f, deformed ); // no per-vertex normal here -- wave/bulge pass through, same as before

		for ( s = 0; s < shader->numStages; s++ ) {
			const shaderStage_t *stage = &shader->stages[s];
			vkEntityDynamicVert_t *dst = &out[(uint32_t)s * (uint32_t)numVerts + (uint32_t)v];
			byte color[4];
			float st[2], baseST[2];

			R_CalcStageColorAlpha( stage, entityColor, baseColor, color );
			CalcStageBaseST( stage, &base[v], localViewOrigin, baseST );
			R_CalcStageTexCoord( stage, baseST[0], baseST[1], deformed, shaderTexCoord, st );

			dst->color[0] = color[0]; dst->color[1] = color[1]; dst->color[2] = color[2]; dst->color[3] = color[3];
			dst->uv[0] = st[0]; dst->uv[1] = st[1];
		}
	}
}

static void WriteLerpMultiTexVertices( const shader_t *shader, const byte *entityColor, const float *shaderTexCoord,
										 const float *localViewOrigin, int numVerts, const entityBaseVert_t *base, vkEntityDynamicMTVert_t *out ) {
	const shaderStage_t *stage0 = &shader->stages[0];
	const shaderStage_t *stage1 = &shader->stages[1];
	static const byte baseColor[4] = { 255, 255, 255, 255 };
	int v;

	for ( v = 0; v < numVerts; v++ ) {
		float deformed[3];
		byte color[4];
		float st0[2], st1[2], baseST0[2], baseST1[2];

		R_DeformPosition( shader, base[v].pos, NULL, 0.0f, deformed ); // no per-vertex normal here -- wave/bulge pass through, same as before
		R_CalcStageColorAlpha( stage0, entityColor, baseColor, color );
		CalcStageBaseST( stage0, &base[v], localViewOrigin, baseST0 );
		CalcStageBaseST( stage1, &base[v], localViewOrigin, baseST1 );
		R_CalcStageTexCoord( stage0, baseST0[0], baseST0[1], deformed, shaderTexCoord, st0 );
		R_CalcStageTexCoord( stage1, baseST1[0], baseST1[1], deformed, shaderTexCoord, st1 );

		out[v].color[0] = color[0]; out[v].color[1] = color[1]; out[v].color[2] = color[2]; out[v].color[3] = color[3];
		out[v].uv0[0] = st0[0]; out[v].uv0[1] = st0[1];
		out[v].uv1[0] = st1[0]; out[v].uv1[1] = st1[1];
	}
}

// rgb (0-255-ish, not yet clamped) -> unorm4x8 bit pattern matching GLSL's unpackUnorm4x8 byte order (byte0=r..byte3=a).
static uint32_t PackColorUnorm4x8( const vec3_t color ) {
	int r = (int)color[0], g = (int)color[1], b = (int)color[2];
	r = r < 0 ? 0 : ( r > 255 ? 255 : r );
	g = g < 0 ? 0 : ( g > 255 ? 255 : g );
	b = b < 0 ? 0 : ( b > 255 ? 255 : b );
	return (uint32_t)r | ( (uint32_t)g << 8 ) | ( (uint32_t)b << 16 ) | ( 255u << 24 );
}

// Unit direction -> the (lat, long) radian pair entity_lerp.vert/entity_skin.vert decode back via vec3(cos(lat)*sin(lng), sin(lat)*sin(lng), cos(lng)) -- the inverse of tr_light.c's LatLongToNormal.
static void EncodeDirectionRadians( const vec3_t dir, float *outLat, float *outLng ) {
	float z = dir[2] < -1.0f ? -1.0f : ( dir[2] > 1.0f ? 1.0f : dir[2] );
	*outLng = acosf( z );
	*outLat = atan2f( dir[1], dir[0] );
}

// Combines the camera VP with this entity's model transform (cameraMVP * entityModel) and transforms the camera into the entity's local space (exact for a rigid axis/origin transform -- same world-space distance) so entity_lerp.vert/.frag can reuse single_texture.vert/.frag's existing 128-byte push-constant layout unchanged: camPos.xyz becomes the local-space camera, and the otherwise-unused camPos.w/fogColor.a/clipPlane.yzw slots carry the CGEN_LIGHTING_DIFFUSE data (ambient/directed packed unorm4x8, light direction as entity-local-space lat/long radians) the GPU needs for real per-vertex diffuse lighting -- see ComputeEntityLighting. clipPlane.x still carries backlerp (entities never clip). Valid for both the single-tex and multitex pipelines -- they read the same pushed bytes, just declare the trailing fields under different names (see entity_lerp.vert/entity_lerp_multitex.vert).
static void BuildLerpPushConstants( const refEntity_t *ent, const float cameraMvp[16], const float fogPush[12],
									  const vec3_t ambient, const vec3_t directed, const vec3_t lightDirWorld, float out[32] ) {
	float entityModel[16];
	vec3_t delta, lightDirLocal;
	uint32_t ambientPacked, directedPacked;
	float lat, lng;

	entityModel[0] = ent->axis[0][0]; entityModel[1] = ent->axis[0][1]; entityModel[2] = ent->axis[0][2]; entityModel[3] = 0.0f;
	entityModel[4] = ent->axis[1][0]; entityModel[5] = ent->axis[1][1]; entityModel[6] = ent->axis[1][2]; entityModel[7] = 0.0f;
	entityModel[8] = ent->axis[2][0]; entityModel[9] = ent->axis[2][1]; entityModel[10] = ent->axis[2][2]; entityModel[11] = 0.0f;
	entityModel[12] = ent->origin[0]; entityModel[13] = ent->origin[1]; entityModel[14] = ent->origin[2]; entityModel[15] = 1.0f;

	R_MultMatrix( cameraMvp, entityModel, out );

	delta[0] = fogPush[0] - ent->origin[0];
	delta[1] = fogPush[1] - ent->origin[1];
	delta[2] = fogPush[2] - ent->origin[2];
	out[16] = DotProduct( ent->axis[0], delta );
	out[17] = DotProduct( ent->axis[1], delta );
	out[18] = DotProduct( ent->axis[2], delta );

	ambientPacked = PackColorUnorm4x8( ambient );
	memcpy( &out[19], &ambientPacked, sizeof( float ) );

	out[20] = fogPush[4]; out[21] = fogPush[5]; out[22] = fogPush[6];
	directedPacked = PackColorUnorm4x8( directed );
	memcpy( &out[23], &directedPacked, sizeof( float ) );

	out[24] = fogPush[8]; out[25] = fogPush[9]; out[26] = fogPush[10]; out[27] = fogPush[11];

	out[28] = ent->backlerp;
	lightDirLocal[0] = DotProduct( ent->axis[0], lightDirWorld );
	lightDirLocal[1] = DotProduct( ent->axis[1], lightDirWorld );
	lightDirLocal[2] = DotProduct( ent->axis[2], lightDirWorld );
	EncodeDirectionRadians( lightDirLocal, &lat, &lng );
	out[29] = lat; out[30] = lng; out[31] = 0.0f;
}

// Issues this surface's draw(s) immediately: binding 0/1 are the model's static per-surface buffer at frame/oldframe's byte offsets (position only -- see vkEntityFrameVert_t), binding 2 is this frame's freshly-evaluated color/uv. A shared vertexOffset can't give bindings 0/1 and binding 2 different per-stage offsets at once, so binding 2 gets rebound between stages instead, with vertexOffset left at 0 throughout. push_data must already hold this entity's combined mvp/localCamPos/backlerp (see BuildLerpPushConstants) and gets pushed once per surface since it's shared across the surface's multitex-or-per-stage draws; curDepthHack is shared with the deferred draw phase at the bottom of vk_entity_draw_all so the view-weapon depth hack doesn't go stale crossing between the two phases.
static void DrawLerpSurface( const shader_t *shaderPtr, const refEntity_t *ent,
							   int numVerts, int numTriangles, const void *trianglesPtr, const entityBaseVert_t *baseVerts,
							   const entitySurfAnimBuffer_t *animBuf, VkDeviceSize frameOffset, VkDeviceSize oldframeOffset,
							   const float push_data[32], const float *localViewOrigin, qboolean *curDepthHack ) {
	VkBuffer staticBufs[2];
	VkDeviceSize staticOffsets[2];
	qboolean depthHack = ( ent->renderfx & RF_DEPTHHACK ) != 0;
	int combineMode;

	if ( !shaderPtr || shaderPtr->numStages < 1 ) {
		return;
	}

	if ( depthHack != *curDepthHack ) {
		SetDepthRange( depthHack );
		*curDepthHack = depthHack;
	}

	staticBufs[0] = animBuf->buffer; staticOffsets[0] = frameOffset;
	staticBufs[1] = animBuf->buffer; staticOffsets[1] = oldframeOffset;
	qvkCmdBindVertexBuffers( vk.command_buffer, 0, 2, staticBufs, staticOffsets );
	qvkCmdPushConstants( vk.command_buffer, vk.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 32 * sizeof( float ), push_data );

	if ( vk_canCollapseMultiTex( shaderPtr, &combineMode ) ) {
		image_t *image0 = R_GetStageImage( &shaderPtr->stages[0] );
		image_t *image1 = R_GetStageImage( &shaderPtr->stages[1] );
		VkDeviceSize dynOffset;
		VkPipeline pipeline;
		VkDescriptorSet descSet;

		if ( !image0 || !image1 ) {
			return;
		}
		if ( !ReserveLerpSurface( numVerts, 1, numTriangles, s_lerpMtVertWrite, s_idxWrite ) ) {
			static qboolean warned = qfalse;
			if ( !warned ) {
				ri.Printf( PRINT_WARNING, "vk_entity: GPU vertex-lerp multitex scratch buffer full, dropping remaining entity surfaces this frame\n" );
				warned = qtrue;
			}
			return;
		}

		WriteLerpMultiTexVertices( shaderPtr, ent->shaderRGBA, ent->shaderTexCoord, localViewOrigin, numVerts, baseVerts,
			&s_lerpMtVertexMapped[s_lerpMtVertRegionBase + s_lerpMtVertWrite] );
		WriteIndices( numTriangles, trianglesPtr, s_idxRegionBase + s_idxWrite );
		qvkCmdBindIndexBuffer( vk.command_buffer, s_indexBuffer, (VkDeviceSize)( s_idxRegionBase + s_idxWrite ) * sizeof( uint32_t ), VK_INDEX_TYPE_UINT32 );

		dynOffset = (VkDeviceSize)( s_lerpMtVertRegionBase + s_lerpMtVertWrite ) * sizeof( vkEntityDynamicMTVert_t );
		pipeline = vk_getPipelineEntityLerpMultiTex( combineMode, vk_cullModeForShader( shaderPtr->cullType ), shaderPtr->stages[0].alphaTestFunc,
			shaderPtr->stages[0].rgbGen == CGEN_LIGHTING_DIFFUSE );
		descSet = vk_getMultiTextureDescriptor( image0, image1 );

		qvkCmdBindPipeline( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline );
		qvkCmdBindVertexBuffers( vk.command_buffer, 2, 1, &s_lerpMtVertexBuffer, &dynOffset );
		qvkCmdBindDescriptorSets( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.lightmap_pipeline_layout, 0, 1, &descSet, 0, NULL );
		qvkCmdDrawIndexed( vk.command_buffer, (uint32_t)numTriangles * 3, 1, 0, 0, 0 );

		s_lerpMtVertWrite += (uint32_t)numVerts;
		s_idxWrite += (uint32_t)numTriangles * 3;
		return;
	}

	if ( !ReserveLerpSurface( numVerts, shaderPtr->numStages, numTriangles, s_lerpVertWrite, s_idxWrite ) ) {
		static qboolean warned = qfalse;
		if ( !warned ) {
			ri.Printf( PRINT_WARNING, "vk_entity: GPU vertex-lerp scratch buffer full, dropping remaining entity surfaces this frame\n" );
			warned = qtrue;
		}
		return;
	}

	{
		int s;
		uint32_t idxBase = s_idxWrite;

		WriteLerpStageVertices( shaderPtr, ent->shaderRGBA, ent->shaderTexCoord, localViewOrigin, numVerts, baseVerts,
			&s_lerpVertexMapped[s_lerpVertRegionBase + s_lerpVertWrite] );
		WriteIndices( numTriangles, trianglesPtr, s_idxRegionBase + idxBase );
		qvkCmdBindIndexBuffer( vk.command_buffer, s_indexBuffer, (VkDeviceSize)( s_idxRegionBase + idxBase ) * sizeof( uint32_t ), VK_INDEX_TYPE_UINT32 );

		for ( s = 0; s < shaderPtr->numStages; s++ ) {
			const shaderStage_t *stage = &shaderPtr->stages[s];
			image_t *stageImage = R_GetStageImage( stage );
			VkDeviceSize dynOffset;
			VkPipeline pipeline;

			if ( !stageImage || stageImage->vk_descriptor == VK_NULL_HANDLE ) {
				continue;
			}

			dynOffset = (VkDeviceSize)( s_lerpVertRegionBase + s_lerpVertWrite + (uint32_t)s * (uint32_t)numVerts ) * sizeof( vkEntityDynamicVert_t );
			pipeline = vk_getPipelineEntityLerp( stage->blendEnabled, stage->srcBlend, stage->dstBlend,
				vk_cullModeForShader( shaderPtr->cullType ), stage->alphaTestFunc, shaderPtr->polygonOffset,
				stage->rgbGen == CGEN_LIGHTING_DIFFUSE );

			qvkCmdBindPipeline( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline );
			qvkCmdBindVertexBuffers( vk.command_buffer, 2, 1, &s_lerpVertexBuffer, &dynOffset );
			qvkCmdBindDescriptorSets( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout, 0, 1, &stageImage->vk_descriptor, 0, NULL );
			qvkCmdDrawIndexed( vk.command_buffer, (uint32_t)numTriangles * 3, 1, 0, 0, 0 );
		}

		s_lerpVertWrite += (uint32_t)numVerts * (uint32_t)shaderPtr->numStages;
		s_idxWrite += (uint32_t)numTriangles * 3;
	}
}

// Writes refent's bone matrices into this frame's next bone-UBO slot and returns the dynamic byte
// offset to bind set 1 at for it, shared across all of this entity's surfaces (bones don't vary per
// surface). qfalse only when this frame's skinned-draw budget (MAX_SKINNED_ENTITY_DRAWS) is used up;
// an MDM with no skeleton linked yet gets an identity-matrix set instead of failing, so the weighted
// transform sum in the shader reduces to the same bind-pose sum R_SkinMDMSurface's own CPU fallback
// already uses (VectorMA accumulation) -- see vk_entity_register_model's MDM build for where that
// bind-pose data itself comes from.
static qboolean WriteBoneMatricesMDS( const mdsHeader_t *mds, const refEntity_t *ent, uint32_t *outDynOffset ) {
	float mats[MDX_MAX_BONES][16];

	if ( s_boneUboWrite >= MAX_SKINNED_ENTITY_DRAWS ) {
		static qboolean warned = qfalse;
		if ( !warned ) {
			ri.Printf( PRINT_WARNING, "vk_entity: MAX_SKINNED_ENTITY_DRAWS hit, dropping remaining skinned entities this frame\n" );
			warned = qtrue;
		}
		return qfalse;
	}

	R_CalcAllBoneMatricesMDS( mds, ent, mats );
	memcpy( s_boneUboMapped + s_boneUboRegionBase + (VkDeviceSize)s_boneUboWrite * s_boneUboStride, mats, sizeof( mats ) );
	*outDynOffset = (uint32_t)( s_boneUboRegionBase + (VkDeviceSize)s_boneUboWrite * s_boneUboStride );
	s_boneUboWrite++;
	return qtrue;
}

static qboolean WriteBoneMatricesMDM( const refEntity_t *ent, uint32_t *outDynOffset ) {
	float mats[MDX_MAX_BONES][16];

	if ( s_boneUboWrite >= MAX_SKINNED_ENTITY_DRAWS ) {
		static qboolean warned = qfalse;
		if ( !warned ) {
			ri.Printf( PRINT_WARNING, "vk_entity: MAX_SKINNED_ENTITY_DRAWS hit, dropping remaining skinned entities this frame\n" );
			warned = qtrue;
		}
		return qfalse;
	}

	if ( !R_CalcAllBoneMatricesMDM( ent, mats ) ) {
		static const float identity[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
		int i;
		for ( i = 0; i < MDX_MAX_BONES; i++ ) {
			memcpy( mats[i], identity, sizeof( identity ) );
		}
	}

	memcpy( s_boneUboMapped + s_boneUboRegionBase + (VkDeviceSize)s_boneUboWrite * s_boneUboStride, mats, sizeof( mats ) );
	*outDynOffset = (uint32_t)( s_boneUboRegionBase + (VkDeviceSize)s_boneUboWrite * s_boneUboStride );
	s_boneUboWrite++;
	return qtrue;
}

// Issues this surface's draw(s) immediately, mirroring DrawLerpSurface but for the GPU bone-skinning
// path: binding 0 is the model's static bind-pose buffer (one entry per vertex, no frame/oldframe
// duplication -- bones animate it, not frame-blending), binding 1 is this frame's dynamic color/uv
// (shared buffers/cursors with the lerp path -- it's the same generic rgbGen/alphaGen/tcMod data
// either way), and set 1 is the bone-matrix UBO at boneUboDynOffset (shared across this entity's
// surfaces, written once by WriteBoneMatricesMDS/MDM before the surface loop starts).
static void DrawSkinSurface( const shader_t *shaderPtr, const refEntity_t *ent,
							   int numVerts, int numTriangles, const void *trianglesPtr, const entityBaseVert_t *baseVerts,
							   const entitySurfAnimBuffer_t *skinBuf, const float push_data[32],
							   uint32_t boneUboDynOffset, qboolean *curDepthHack ) {
	VkDeviceSize staticOffset = 0;
	qboolean depthHack = ( ent->renderfx & RF_DEPTHHACK ) != 0;
	int combineMode;

	if ( !shaderPtr || shaderPtr->numStages < 1 ) {
		return;
	}

	if ( depthHack != *curDepthHack ) {
		SetDepthRange( depthHack );
		*curDepthHack = depthHack;
	}

	qvkCmdBindVertexBuffers( vk.command_buffer, 0, 1, &skinBuf->buffer, &staticOffset );
	// vk.skin_pipeline_layout/skin_lightmap_pipeline_layout declare the same push-constant range, so one push here covers draws through either (same compatibility DrawLerpSurface already relies on for vk.pipeline_layout/lightmap_pipeline_layout).
	qvkCmdPushConstants( vk.command_buffer, vk.skin_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 32 * sizeof( float ), push_data );

	if ( vk_canCollapseMultiTex( shaderPtr, &combineMode ) ) {
		image_t *image0 = R_GetStageImage( &shaderPtr->stages[0] );
		image_t *image1 = R_GetStageImage( &shaderPtr->stages[1] );
		VkDeviceSize dynOffset;
		VkPipeline pipeline;
		VkDescriptorSet texDescSet;

		if ( !image0 || !image1 ) {
			return;
		}
		if ( !ReserveLerpSurface( numVerts, 1, numTriangles, s_lerpMtVertWrite, s_idxWrite ) ) {
			static qboolean warned = qfalse;
			if ( !warned ) {
				ri.Printf( PRINT_WARNING, "vk_entity: GPU skin multitex scratch buffer full, dropping remaining entity surfaces this frame\n" );
				warned = qtrue;
			}
			return;
		}

		WriteLerpMultiTexVertices( shaderPtr, ent->shaderRGBA, ent->shaderTexCoord, NULL /* no env-mapped normal/localPos on the MDS/MDM skin path */, numVerts, baseVerts,
			&s_lerpMtVertexMapped[s_lerpMtVertRegionBase + s_lerpMtVertWrite] );
		WriteIndices( numTriangles, trianglesPtr, s_idxRegionBase + s_idxWrite );
		qvkCmdBindIndexBuffer( vk.command_buffer, s_indexBuffer, (VkDeviceSize)( s_idxRegionBase + s_idxWrite ) * sizeof( uint32_t ), VK_INDEX_TYPE_UINT32 );

		dynOffset = (VkDeviceSize)( s_lerpMtVertRegionBase + s_lerpMtVertWrite ) * sizeof( vkEntityDynamicMTVert_t );
		pipeline = vk_getPipelineEntitySkinMultiTex( combineMode, vk_cullModeForShader( shaderPtr->cullType ), shaderPtr->stages[0].alphaTestFunc,
			shaderPtr->stages[0].rgbGen == CGEN_LIGHTING_DIFFUSE );
		texDescSet = vk_getMultiTextureDescriptor( image0, image1 );

		qvkCmdBindPipeline( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline );
		qvkCmdBindVertexBuffers( vk.command_buffer, 1, 1, &s_lerpMtVertexBuffer, &dynOffset );
		qvkCmdBindDescriptorSets( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.skin_lightmap_pipeline_layout, 0, 1, &texDescSet, 0, NULL );
		qvkCmdBindDescriptorSets( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.skin_lightmap_pipeline_layout, 1, 1, &s_skinDescriptorSet, 1, &boneUboDynOffset );
		qvkCmdDrawIndexed( vk.command_buffer, (uint32_t)numTriangles * 3, 1, 0, 0, 0 );

		s_lerpMtVertWrite += (uint32_t)numVerts;
		s_idxWrite += (uint32_t)numTriangles * 3;
		return;
	}

	if ( !ReserveLerpSurface( numVerts, shaderPtr->numStages, numTriangles, s_lerpVertWrite, s_idxWrite ) ) {
		static qboolean warned = qfalse;
		if ( !warned ) {
			ri.Printf( PRINT_WARNING, "vk_entity: GPU skin scratch buffer full, dropping remaining entity surfaces this frame\n" );
			warned = qtrue;
		}
		return;
	}

	{
		int s;
		uint32_t idxBase = s_idxWrite;

		WriteLerpStageVertices( shaderPtr, ent->shaderRGBA, ent->shaderTexCoord, NULL /* no env-mapped normal/localPos on the MDS/MDM skin path */, numVerts, baseVerts,
			&s_lerpVertexMapped[s_lerpVertRegionBase + s_lerpVertWrite] );
		WriteIndices( numTriangles, trianglesPtr, s_idxRegionBase + idxBase );
		qvkCmdBindIndexBuffer( vk.command_buffer, s_indexBuffer, (VkDeviceSize)( s_idxRegionBase + idxBase ) * sizeof( uint32_t ), VK_INDEX_TYPE_UINT32 );

		for ( s = 0; s < shaderPtr->numStages; s++ ) {
			const shaderStage_t *stage = &shaderPtr->stages[s];
			image_t *stageImage = R_GetStageImage( stage );
			VkDeviceSize dynOffset;
			VkPipeline pipeline;

			if ( !stageImage || stageImage->vk_descriptor == VK_NULL_HANDLE ) {
				continue;
			}

			dynOffset = (VkDeviceSize)( s_lerpVertRegionBase + s_lerpVertWrite + (uint32_t)s * (uint32_t)numVerts ) * sizeof( vkEntityDynamicVert_t );
			pipeline = vk_getPipelineEntitySkin( stage->blendEnabled, stage->srcBlend, stage->dstBlend,
				vk_cullModeForShader( shaderPtr->cullType ), stage->alphaTestFunc, shaderPtr->polygonOffset,
				stage->rgbGen == CGEN_LIGHTING_DIFFUSE );

			qvkCmdBindPipeline( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline );
			qvkCmdBindVertexBuffers( vk.command_buffer, 1, 1, &s_lerpVertexBuffer, &dynOffset );
			qvkCmdBindDescriptorSets( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.skin_pipeline_layout, 0, 1, &stageImage->vk_descriptor, 0, NULL );
			qvkCmdBindDescriptorSets( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.skin_pipeline_layout, 1, 1, &s_skinDescriptorSet, 1, &boneUboDynOffset );
			qvkCmdDrawIndexed( vk.command_buffer, (uint32_t)numTriangles * 3, 1, 0, 0, 0 );
		}

		s_lerpVertWrite += (uint32_t)numVerts * (uint32_t)shaderPtr->numStages;
		s_idxWrite += (uint32_t)numTriangles * 3;
	}
}

// Temporary diagnostic for the weapon-visibility investigation: set r_vk_debugWeapon 1 and check rtcwconsole.log. Remove once root-caused.
static cvar_t *r_vk_debugWeapon;
static int s_weaponDbgFrame = 0;

void vk_entity_draw_all( const refEntity_t *entities, int numEntities, const float mvp[16], const float fogPush[12] ) {
	int e, s;
	qboolean dbgFrame;
	// Shared across every entity's immediate lerp/skin draws below, and the final reset at the bottom of this function, so the view-weapon depth hack never goes stale between entities.
	qboolean curDepthHack = qfalse;

	if ( !r_vk_debugWeapon ) {
		r_vk_debugWeapon = ri.Cvar_Get( "r_vk_debugWeapon", "0", 0 );
	}
	s_weaponDbgFrame++;
	dbgFrame = r_vk_debugWeapon->integer && ( s_weaponDbgFrame % 10 ) == 0;

	for ( e = 0; e < numEntities; e++ ) {
		const refEntity_t *ent = &entities[e];
		model_t *model = R_GetModelByHandle( ent->hModel );
		vec3_t ambient, directed, lightDirWorld, localViewOrigin, camDelta;
		qboolean dbgWeapon = dbgFrame; // logs every entity, not just RF_DEPTHHACK ones -- held items like mines don't all set it
		qboolean castsShadow;

		if ( !model ) {
			continue;
		}
		ComputeEntityLighting( ent, ambient, directed, lightDirWorld );

		VectorSubtract( fogPush, ent->origin, camDelta );
		localViewOrigin[0] = DotProduct( ent->axis[0], camDelta );
		localViewOrigin[1] = DotProduct( ent->axis[1], camDelta );
		localViewOrigin[2] = DotProduct( ent->axis[2], camDelta );

		castsShadow = vk_shadows_entity_casts( ent );
		if ( castsShadow ) {
			vk_shadows_begin_entity();
		}

		if ( dbgWeapon ) {
			ri.Printf( PRINT_ALL, "vk_entity dbg: e=%d '%s' type=%d renderfx=%d frame=%d oldframe=%d backlerp=%.2f origin=(%.1f %.1f %.1f) idxWrite=%u\n",
				e, model->name, model->type, ent->renderfx, ent->frame, ent->oldframe, ent->backlerp,
				ent->origin[0], ent->origin[1], ent->origin[2], s_idxWrite );
		}

		if ( model->type == MOD_MD3 && model->md3 ) {
			const md3Surface_t *surf = (const md3Surface_t *)( (const byte *)model->md3 + model->md3->ofsSurfaces );
			entityModelAnimData_t *animData = (entityModelAnimData_t *)model->vkAnimData;
			int frame = ent->frame, oldframe = ent->oldframe;
			float push_data[32];
			if ( frame < 0 || frame >= model->md3->numFrames ) frame = 0;
			if ( oldframe < 0 || oldframe >= model->md3->numFrames ) oldframe = 0;
			if ( animData ) {
				BuildLerpPushConstants( ent, mvp, fogPush, ambient, directed, lightDirWorld, push_data );
			}

			for ( s = 0; s < model->md3->numSurfaces; s++ ) {
				qhandle_t shaderHandle = ResolveShader( ent->customShader, ent->customSkin, surf->name,
					surf->numShaders, (const byte *)surf + surf->ofsShaders );
				shader_t *shaderPtr = R_GetShaderByHandle( shaderHandle );

				if ( surf->numVerts > SKIN_SCRATCH_VERTS ) {
					goto nextMD3;
				}
				if ( !Q_stricmp( surf->name, "h_blink" ) && ( !( ent->renderfx & RF_BLINK ) || shaderHandle == 0 ) ) {
					goto nextMD3;
				}

				if ( dbgWeapon ) {
					ri.Printf( PRINT_ALL, "  MD3 surf '%s' numVerts=%d numTriangles=%d numShaders=%d shader='%s' stages=%d idxWrite=%u\n",
						surf->name, surf->numVerts, surf->numTriangles, surf->numShaders,
						shaderPtr ? shaderPtr->name : "(null)", shaderPtr ? shaderPtr->numStages : -1, s_idxWrite );
				}

				// Kept for tcMod turb/deformVertexes move (both need the real world-space position) and shadow volumes below -- DrawLerpSurface does the actual render-path position blend on the GPU now, so this is no longer uploaded anywhere.
				LerpSurfaceVerts( ent, surf, frame, oldframe, ent->backlerp, s_baseScratch );

				if ( animData && s < animData->numSurfaces ) {
					const entitySurfAnimBuffer_t *animBuf = &animData->surfaces[s];
					VkDeviceSize frameOffset = (VkDeviceSize)frame * animBuf->numVertsPerFrame * sizeof( vkEntityFrameVert_t );
					VkDeviceSize oldframeOffset = (VkDeviceSize)oldframe * animBuf->numVertsPerFrame * sizeof( vkEntityFrameVert_t );
					DrawLerpSurface( shaderPtr, ent, surf->numVerts, surf->numTriangles,
						(const byte *)surf + surf->ofsTriangles, s_baseScratch, animBuf, frameOffset, oldframeOffset,
						push_data, localViewOrigin, &curDepthHack );
				}
				if ( castsShadow && shaderPtr && shaderPtr->numStages >= 1 && !shaderPtr->stages[0].blendEnabled ) {
					vk_shadows_add_surface( s_baseScratch[0].pos, sizeof( s_baseScratch[0] ), surf->numVerts,
						(const byte *)surf + surf->ofsTriangles, surf->numTriangles );
				}

			nextMD3:
				surf = (const md3Surface_t *)( (const byte *)surf + surf->ofsEnd );
			}
		} else if ( model->type == MOD_MDC && model->mdc ) {
			const mdcSurface_t *surf = (const mdcSurface_t *)( (const byte *)model->mdc + model->mdc->ofsSurfaces );
			entityModelAnimData_t *animData = (entityModelAnimData_t *)model->vkAnimData;
			int frame = ent->frame, oldframe = ent->oldframe;
			float push_data[32];
			if ( frame < 0 || frame >= model->mdc->numFrames ) frame = 0;
			if ( oldframe < 0 || oldframe >= model->mdc->numFrames ) oldframe = 0;
			if ( animData ) {
				BuildLerpPushConstants( ent, mvp, fogPush, ambient, directed, lightDirWorld, push_data );
			}

			for ( s = 0; s < model->mdc->numSurfaces; s++ ) {
				qhandle_t shaderHandle = ResolveShader( ent->customShader, ent->customSkin, surf->name,
					surf->numShaders, (const byte *)surf + surf->ofsShaders );
				shader_t *shaderPtr = R_GetShaderByHandle( shaderHandle );

				if ( surf->numVerts > SKIN_SCRATCH_VERTS ) {
					goto nextMDC;
				}
				// See the MD3 branch's comment above re: skipping an unresolved blink shader too.
				if ( !Q_stricmp( surf->name, "h_blink" ) && ( !( ent->renderfx & RF_BLINK ) || shaderHandle == 0 ) ) {
					goto nextMDC;
				}

				if ( dbgWeapon ) {
					ri.Printf( PRINT_ALL, "  MDC surf '%s' numVerts=%d numTriangles=%d numShaders=%d numCompFrames=%d shader='%s' stages=%d idxWrite=%u\n",
						surf->name, surf->numVerts, surf->numTriangles, surf->numShaders, surf->numCompFrames,
						shaderPtr ? shaderPtr->name : "(null)", shaderPtr ? shaderPtr->numStages : -1, s_idxWrite );
				}

				LerpMDCSurfaceVerts( ent, surf, frame, oldframe, ent->backlerp, s_baseScratch ); // see the MD3 branch's comment above

				if ( animData && s < animData->numSurfaces ) {
					const entitySurfAnimBuffer_t *animBuf = &animData->surfaces[s];
					VkDeviceSize frameOffset = (VkDeviceSize)frame * animBuf->numVertsPerFrame * sizeof( vkEntityFrameVert_t );
					VkDeviceSize oldframeOffset = (VkDeviceSize)oldframe * animBuf->numVertsPerFrame * sizeof( vkEntityFrameVert_t );
					DrawLerpSurface( shaderPtr, ent, surf->numVerts, surf->numTriangles,
						(const byte *)surf + surf->ofsTriangles, s_baseScratch, animBuf, frameOffset, oldframeOffset,
						push_data, localViewOrigin, &curDepthHack );
				}
				if ( castsShadow && shaderPtr && shaderPtr->numStages >= 1 && !shaderPtr->stages[0].blendEnabled ) {
					vk_shadows_add_surface( s_baseScratch[0].pos, sizeof( s_baseScratch[0] ), surf->numVerts,
						(const byte *)surf + surf->ofsTriangles, surf->numTriangles );
				}

			nextMDC:
				surf = (const mdcSurface_t *)( (const byte *)surf + surf->ofsEnd );
			}
		} else if ( model->type == MOD_MDS && model->mds ) {
			const mdsSurface_t *surf = (const mdsSurface_t *)( (const byte *)model->mds + model->mds->ofsSurfaces );
			entityModelAnimData_t *animData = (entityModelAnimData_t *)model->vkAnimData;
			float push_data[32];
			uint32_t boneUboDynOffset = 0;
			qboolean haveBones = qfalse;

			if ( animData ) {
				BuildLerpPushConstants( ent, mvp, fogPush, ambient, directed, lightDirWorld, push_data ); // backlerp (push_data[28]) is unused by entity_skin.vert, harmless
				haveBones = WriteBoneMatricesMDS( model->mds, ent, &boneUboDynOffset );
			}

			for ( s = 0; s < model->mds->numSurfaces; s++ ) {
				qhandle_t shaderHandle = ResolveShaderDirect( ent->customShader, ent->customSkin, surf->name, surf->shaderIndex );
				shader_t *shaderPtr = R_GetShaderByHandle( shaderHandle );

				if ( surf->numVerts > SKIN_SCRATCH_VERTS ) {
					goto nextMDS;
				}
				// See the MD3 branch's comment above re: skipping an unresolved blink shader too.
				if ( !Q_stricmp( surf->name, "h_blink" ) && ( !( ent->renderfx & RF_BLINK ) || shaderHandle == 0 ) ) {
					goto nextMDS;
				}

				if ( dbgWeapon ) {
					ri.Printf( PRINT_ALL, "  MDS surf '%s' numVerts=%d numTriangles=%d shader='%s' stages=%d idxWrite=%u\n",
						surf->name, surf->numVerts, surf->numTriangles,
						shaderPtr ? shaderPtr->name : "(null)", shaderPtr ? shaderPtr->numStages : -1, s_idxWrite );
				}

				// Kept for tcMod turb/deformVertexes move and shadow volumes below -- DrawSkinSurface does the actual render-path skin on the GPU now, so this is no longer uploaded anywhere.
				LerpMDSSurfaceVerts( ent, model->mds, surf, s_baseScratch );

				if ( haveBones && s < animData->numSurfaces ) {
					DrawSkinSurface( shaderPtr, ent, surf->numVerts, surf->numTriangles,
						(const byte *)surf + surf->ofsTriangles, s_baseScratch, &animData->surfaces[s],
						push_data, boneUboDynOffset, &curDepthHack );
				}
				if ( castsShadow && shaderPtr && shaderPtr->numStages >= 1 && !shaderPtr->stages[0].blendEnabled ) {
					vk_shadows_add_surface( s_baseScratch[0].pos, sizeof( s_baseScratch[0] ), surf->numVerts,
						(const byte *)surf + surf->ofsTriangles, surf->numTriangles );
				}

			nextMDS:
				surf = (const mdsSurface_t *)( (const byte *)surf + surf->ofsEnd );
			}
		} else if ( model->type == MOD_MDM && model->mdm ) {
			const mdmSurface_t *surf = (const mdmSurface_t *)( (const byte *)model->mdm + model->mdm->ofsSurfaces );
			entityModelAnimData_t *animData = (entityModelAnimData_t *)model->vkAnimData;
			float push_data[32];
			uint32_t boneUboDynOffset = 0;
			qboolean haveBones = qfalse;

			if ( animData ) {
				BuildLerpPushConstants( ent, mvp, fogPush, ambient, directed, lightDirWorld, push_data );
				haveBones = WriteBoneMatricesMDM( ent, &boneUboDynOffset ); // identity bones (bind pose) if no MDX skeleton is linked yet -- see WriteBoneMatricesMDM
			}

			for ( s = 0; s < model->mdm->numSurfaces; s++ ) {
				qhandle_t shaderHandle = ResolveShaderDirect( ent->customShader, ent->customSkin, surf->name, surf->shaderIndex );
				shader_t *shaderPtr = R_GetShaderByHandle( shaderHandle );

				if ( surf->numVerts > SKIN_SCRATCH_VERTS ) {
					goto nextMDM;
				}
				// See the MD3 branch's comment above re: skipping an unresolved blink shader too.
				if ( !Q_stricmp( surf->name, "h_blink" ) && ( !( ent->renderfx & RF_BLINK ) || shaderHandle == 0 ) ) {
					goto nextMDM;
				}

				if ( dbgWeapon ) {
					ri.Printf( PRINT_ALL, "  MDM surf '%s' numVerts=%d numTriangles=%d shader='%s' stages=%d idxWrite=%u\n",
						surf->name, surf->numVerts, surf->numTriangles,
						shaderPtr ? shaderPtr->name : "(null)", shaderPtr ? shaderPtr->numStages : -1, s_idxWrite );
				}

				LerpMDMSurfaceVerts( ent, surf, s_baseScratch ); // see the MDS branch's comment above

				if ( haveBones && s < animData->numSurfaces ) {
					DrawSkinSurface( shaderPtr, ent, surf->numVerts, surf->numTriangles,
						(const byte *)surf + surf->ofsTriangles, s_baseScratch, &animData->surfaces[s],
						push_data, boneUboDynOffset, &curDepthHack );
				}
				if ( castsShadow && shaderPtr && shaderPtr->numStages >= 1 && !shaderPtr->stages[0].blendEnabled ) {
					vk_shadows_add_surface( s_baseScratch[0].pos, sizeof( s_baseScratch[0] ), surf->numVerts,
						(const byte *)surf + surf->ofsTriangles, surf->numTriangles );
				}

			nextMDM:
				surf = (const mdmSurface_t *)( (const byte *)surf + surf->ofsEnd );
			}
		} else if ( model->type == MOD_BRUSH ) {
			// Geometry lives in tr_world, so draw it directly instead.
			vk_world_draw_brushmodel( ent, model->bmodelIndex, mvp, fogPush );
		}

		if ( castsShadow ) {
			vk_shadows_finish_entity( ent, mvp, fogPush );
		}
	}

	// All 4 model types now draw immediately (DrawLerpSurface/DrawSkinSurface above), so there's nothing deferred left to flush here -- just leave the viewport normal for whatever draws next (polys, 2D) if an immediate draw left it in the weapon-hack range.
	if ( curDepthHack ) {
		SetDepthRange( qfalse );
	}
}
