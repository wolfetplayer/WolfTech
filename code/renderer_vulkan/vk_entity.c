// vk_entity.c -- CPU-lerped MD3 entity mesh rendering, see vk_entity.h.

#include <string.h>

#include "VKimpl.h"
#include "vk_instance.h"
#include "vk_pipelines.h"
#include "tr_image.h"
#include "tr_shader.h"
#include "tr_model.h"
#include "vk_entity.h"

#define ENTITY_MAX_VERTS 65536
#define ENTITY_MAX_INDEXES 262144
#define ENTITY_MAX_DRAWS 4096

typedef struct {
	uint32_t vertBase;
	uint32_t idxBase;
	uint32_t idxCount;
	qhandle_t shader;
} entityDrawCall_t;

static VkBuffer s_vertexBuffer;
static VkDeviceMemory s_vertexMemory;
static vk2d_vertex_t *s_vertexMapped;

static VkBuffer s_indexBuffer;
static VkDeviceMemory s_indexMemory;
static uint32_t *s_indexMapped;

static entityDrawCall_t s_draws[ENTITY_MAX_DRAWS];

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

void vk_entity_init( void ) {
	create_mapped_buffer( ENTITY_MAX_VERTS * sizeof( vk2d_vertex_t ), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		&s_vertexBuffer, &s_vertexMemory, (void **)&s_vertexMapped );
	create_mapped_buffer( ENTITY_MAX_INDEXES * sizeof( uint32_t ), VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		&s_indexBuffer, &s_indexMemory, (void **)&s_indexMapped );
}

void vk_entity_shutdown( void ) {
	destroy_mapped_buffer( s_vertexBuffer, s_vertexMemory, s_vertexMapped );
	s_vertexMapped = NULL;
	destroy_mapped_buffer( s_indexBuffer, s_indexMemory, s_indexMapped );
	s_indexMapped = NULL;
}

qboolean vk_entity_has_mesh( const refEntity_t *ent ) {
	model_t *model = R_GetModelByHandle( ent->hModel );
	return model && model->type == MOD_MD3 && model->md3;
}

// xyz is model-local (frontlerp*frame + backlerp*oldframe, MD3_XYZ_SCALE applied), then placed by the entity's axis/origin.
static void LerpSurfaceVerts( const refEntity_t *ent, const md3Surface_t *surf, int frame, int oldframe, float backlerp, vk2d_vertex_t *out ) {
	const md3XyzNormal_t *base = (const md3XyzNormal_t *)( (const byte *)surf + surf->ofsXyzNormals );
	const md3XyzNormal_t *newXyz = base + (size_t)frame * surf->numVerts;
	const md3XyzNormal_t *oldXyz = base + (size_t)oldframe * surf->numVerts;
	const md3St_t *st = (const md3St_t *)( (const byte *)surf + surf->ofsSt );
	float frontlerp = 1.0f - backlerp;
	int i, k;

	for ( i = 0; i < surf->numVerts; i++ ) {
		float local[3];
		for ( k = 0; k < 3; k++ ) {
			local[k] = ( frontlerp * (float)newXyz[i].xyz[k] + backlerp * (float)oldXyz[i].xyz[k] ) * MD3_XYZ_SCALE;
		}

		out[i].pos[0] = ent->origin[0] + local[0] * ent->axis[0][0] + local[1] * ent->axis[1][0] + local[2] * ent->axis[2][0];
		out[i].pos[1] = ent->origin[1] + local[0] * ent->axis[0][1] + local[1] * ent->axis[1][1] + local[2] * ent->axis[2][1];
		out[i].pos[2] = ent->origin[2] + local[0] * ent->axis[0][2] + local[1] * ent->axis[1][2] + local[2] * ent->axis[2][2];

		out[i].color[0] = 255; out[i].color[1] = 255; out[i].color[2] = 255; out[i].color[3] = 255;
		out[i].uv[0] = st[i].st[0];
		out[i].uv[1] = st[i].st[1];
	}
}

void vk_entity_draw_all( const refEntity_t *entities, int numEntities, const float mvp[16] ) {
	uint32_t vertWrite = 0, idxWrite = 0;
	int numDraws = 0;
	int e, s, i;
	static qboolean overflowLogged = qfalse;

	for ( e = 0; e < numEntities; e++ ) {
		const refEntity_t *ent = &entities[e];
		model_t *model = R_GetModelByHandle( ent->hModel );
		const md3Surface_t *surf;
		int frame, oldframe;
		float backlerp;

		if ( !model || model->type != MOD_MD3 || !model->md3 ) {
			continue;
		}

		frame = ent->frame;
		oldframe = ent->oldframe;
		if ( frame < 0 || frame >= model->md3->numFrames ) frame = 0;
		if ( oldframe < 0 || oldframe >= model->md3->numFrames ) oldframe = 0;
		backlerp = ent->backlerp;

		surf = (const md3Surface_t *)( (const byte *)model->md3 + model->md3->ofsSurfaces );
		for ( s = 0; s < model->md3->numSurfaces; s++ ) {
			qhandle_t shader = ent->customShader;

			if ( numDraws >= ENTITY_MAX_DRAWS || vertWrite + (uint32_t)surf->numVerts > ENTITY_MAX_VERTS ||
				 idxWrite + (uint32_t)surf->numTriangles * 3 > ENTITY_MAX_INDEXES ) {
				if ( !overflowLogged ) {
					ri.Printf( PRINT_WARNING, "vk_entity_draw_all: scratch buffer full, dropping remaining entity surfaces this frame\n" );
					overflowLogged = qtrue;
				}
				goto doneCollecting;
			}

			if ( !shader && surf->numShaders > 0 ) {
				const md3Shader_t *surfShader = (const md3Shader_t *)( (const byte *)surf + surf->ofsShaders );
				shader = (qhandle_t)surfShader->shaderIndex;
			}

			LerpSurfaceVerts( ent, surf, frame, oldframe, backlerp, &s_vertexMapped[vertWrite] );

			{
				const md3Triangle_t *tri = (const md3Triangle_t *)( (const byte *)surf + surf->ofsTriangles );
				for ( i = 0; i < surf->numTriangles; i++, tri++ ) {
					s_indexMapped[idxWrite + (uint32_t)i * 3 + 0] = (uint32_t)tri->indexes[0];
					s_indexMapped[idxWrite + (uint32_t)i * 3 + 1] = (uint32_t)tri->indexes[1];
					s_indexMapped[idxWrite + (uint32_t)i * 3 + 2] = (uint32_t)tri->indexes[2];
				}
			}

			s_draws[numDraws].vertBase = vertWrite;
			s_draws[numDraws].idxBase = idxWrite;
			s_draws[numDraws].idxCount = (uint32_t)surf->numTriangles * 3;
			s_draws[numDraws].shader = shader;
			numDraws++;

			vertWrite += (uint32_t)surf->numVerts;
			idxWrite += (uint32_t)surf->numTriangles * 3;

			surf = (const md3Surface_t *)( (const byte *)surf + surf->ofsEnd );
		}
	}

doneCollecting:
	if ( numDraws == 0 ) {
		return;
	}

	{
		float push_data[32];
		VkDeviceSize vb_offset = 0;

		memset( push_data, 0, sizeof( push_data ) );
		memcpy( push_data, mvp, 16 * sizeof( float ) );
		qvkCmdPushConstants( vk.command_buffer, vk.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT,
			0, sizeof( push_data ), push_data );

		qvkCmdBindPipeline( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_3d_opaque );
		qvkCmdBindVertexBuffers( vk.command_buffer, 0, 1, &s_vertexBuffer, &vb_offset );
		qvkCmdBindIndexBuffer( vk.command_buffer, s_indexBuffer, 0, VK_INDEX_TYPE_UINT32 );

		for ( i = 0; i < numDraws; i++ ) {
			shader_t *shaderPtr = R_GetShaderByHandle( s_draws[i].shader );
			if ( !shaderPtr || shaderPtr->numStages < 1 || !shaderPtr->stages[0].image ||
				 shaderPtr->stages[0].image->vk_descriptor == VK_NULL_HANDLE ) {
				continue;
			}

			qvkCmdBindDescriptorSets( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout,
				0, 1, &shaderPtr->stages[0].image->vk_descriptor, 0, NULL );
			qvkCmdDrawIndexed( vk.command_buffer, s_draws[i].idxCount, 1, s_draws[i].idxBase, (int32_t)s_draws[i].vertBase, 0 );
		}
	}
}
