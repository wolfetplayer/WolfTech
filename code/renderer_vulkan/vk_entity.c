// vk_entity.c -- CPU-skinned entity mesh rendering (MD3/MDC frame-morph, MDS/MDM bone-weighted), see vk_entity.h.

#include <string.h>

#include "VKimpl.h"
#include "vk_instance.h"
#include "vk_pipelines.h"
#include "tr_image.h"
#include "tr_shader.h"
#include "tr_model.h"
#include "tr_skin.h"
#include "tr_bones.h"
#include "tr_light.h"
#include "tr_scene.h"
#include "vk_entity.h"

// Same falloff shape the opengl1 renderer uses for dlight contributions to entity lighting.
#define DLIGHT_AT_RADIUS 16.0f
#define DLIGHT_MINIMUM_RADIUS 16.0f

#define ENTITY_MAX_VERTS 65536
#define ENTITY_MAX_INDEXES 262144
#define ENTITY_MAX_DRAWS 4096

#define MDC_DIST_SCALE 0.05f
#define MDC_MAX_OFS 127.0f

// MDS_MAX_VERTS/MDM_MAX_VERTS are both 6000; sized with headroom for a single surface's skinned positions.
#define SKIN_SCRATCH_VERTS 8192
static vec3_t s_skinScratch[SKIN_SCRATCH_VERTS];

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
	if ( !model ) {
		return qfalse;
	}
	return ( model->type == MOD_MD3 && model->md3 ) || ( model->type == MOD_MDC && model->mdc ) ||
		   ( model->type == MOD_MDS && model->mds ) || ( model->type == MOD_MDM && model->mdm );
}

// xyz is model-local (frontlerp*frame + backlerp*oldframe, MD3_XYZ_SCALE applied), then placed by the entity's axis/origin; tint is from ComputeEntityTint.
static void LerpSurfaceVerts( const refEntity_t *ent, const md3Surface_t *surf, int frame, int oldframe, float backlerp, const byte tint[4], vk2d_vertex_t *out ) {
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

		out[i].color[0] = tint[0]; out[i].color[1] = tint[1]; out[i].color[2] = tint[2]; out[i].color[3] = tint[3];
		out[i].uv[0] = st[i].st[0];
		out[i].uv[1] = st[i].st[1];
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

// Each actual frame maps to a base frame (full position) plus an optional compressed delta against it.
static void LerpMDCSurfaceVerts( const refEntity_t *ent, const mdcSurface_t *surf, int frame, int oldframe, float backlerp, const byte tint[4], vk2d_vertex_t *out ) {
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
		vec3_t newXyz, oldXyz, local;

		DecodeMDCVertex( &baseArray[(size_t)newBase * surf->numVerts + i],
			newComp >= 0 ? &compArray[(size_t)newComp * surf->numVerts + i] : NULL, newXyz );
		DecodeMDCVertex( &baseArray[(size_t)oldBase * surf->numVerts + i],
			oldComp >= 0 ? &compArray[(size_t)oldComp * surf->numVerts + i] : NULL, oldXyz );

		local[0] = frontlerp * newXyz[0] + backlerp * oldXyz[0];
		local[1] = frontlerp * newXyz[1] + backlerp * oldXyz[1];
		local[2] = frontlerp * newXyz[2] + backlerp * oldXyz[2];

		out[i].pos[0] = ent->origin[0] + local[0] * ent->axis[0][0] + local[1] * ent->axis[1][0] + local[2] * ent->axis[2][0];
		out[i].pos[1] = ent->origin[1] + local[0] * ent->axis[0][1] + local[1] * ent->axis[1][1] + local[2] * ent->axis[2][1];
		out[i].pos[2] = ent->origin[2] + local[0] * ent->axis[0][2] + local[1] * ent->axis[1][2] + local[2] * ent->axis[2][2];

		out[i].color[0] = tint[0]; out[i].color[1] = tint[1]; out[i].color[2] = tint[2]; out[i].color[3] = tint[3];
		out[i].uv[0] = st[i].st[0];
		out[i].uv[1] = st[i].st[1];
	}
}

// Skins into s_skinScratch (model-local), then places by the entity's axis/origin like the MD3/MDC frame-morph verts.
static void LerpMDSSurfaceVerts( const refEntity_t *ent, const mdsHeader_t *mds, const mdsSurface_t *surf, const byte tint[4], vk2d_vertex_t *out ) {
	const mdsVertex_t *v = (const mdsVertex_t *)( (const byte *)surf + surf->ofsVerts );
	int i;

	if ( surf->numVerts > SKIN_SCRATCH_VERTS ) {
		return;
	}
	R_SkinMDSSurface( mds, surf, ent, s_skinScratch );

	for ( i = 0; i < surf->numVerts; i++ ) {
		const float *local = s_skinScratch[i];

		out[i].pos[0] = ent->origin[0] + local[0] * ent->axis[0][0] + local[1] * ent->axis[1][0] + local[2] * ent->axis[2][0];
		out[i].pos[1] = ent->origin[1] + local[0] * ent->axis[0][1] + local[1] * ent->axis[1][1] + local[2] * ent->axis[2][1];
		out[i].pos[2] = ent->origin[2] + local[0] * ent->axis[0][2] + local[1] * ent->axis[1][2] + local[2] * ent->axis[2][2];

		out[i].color[0] = tint[0]; out[i].color[1] = tint[1]; out[i].color[2] = tint[2]; out[i].color[3] = tint[3];
		out[i].uv[0] = v->texCoords[0];
		out[i].uv[1] = v->texCoords[1];

		v = (const mdsVertex_t *)&v->weights[v->numWeights];
	}
}

static void LerpMDMSurfaceVerts( const refEntity_t *ent, const mdmSurface_t *surf, const byte tint[4], vk2d_vertex_t *out ) {
	const mdmVertex_t *v = (const mdmVertex_t *)( (const byte *)surf + surf->ofsVerts );
	int i;

	if ( surf->numVerts > SKIN_SCRATCH_VERTS ) {
		return;
	}
	R_SkinMDMSurface( surf, ent, s_skinScratch );

	for ( i = 0; i < surf->numVerts; i++ ) {
		const float *local = s_skinScratch[i];

		out[i].pos[0] = ent->origin[0] + local[0] * ent->axis[0][0] + local[1] * ent->axis[1][0] + local[2] * ent->axis[2][0];
		out[i].pos[1] = ent->origin[1] + local[0] * ent->axis[0][1] + local[1] * ent->axis[1][1] + local[2] * ent->axis[2][1];
		out[i].pos[2] = ent->origin[2] + local[0] * ent->axis[0][2] + local[1] * ent->axis[1][2] + local[2] * ent->axis[2][2];

		out[i].color[0] = tint[0]; out[i].color[1] = tint[1]; out[i].color[2] = tint[2]; out[i].color[3] = tint[3];
		out[i].uv[0] = v->texCoords[0];
		out[i].uv[1] = v->texCoords[1];

		v = (const mdmVertex_t *)&v->weights[v->numWeights];
	}
}

// Flat per-entity tint from the light grid plus dynamic lights -- stands in for real per-vertex lighting.
static void ComputeEntityTint( const refEntity_t *ent, byte outColor[4] ) {
	vec3_t origin, ambient, directed, lightDir;
	qboolean hasGrid;
	const dlight_t *dlights;
	int numDlights, i, r, g, b;

	if ( ent->renderfx & RF_LIGHTING_ORIGIN ) {
		VectorCopy( ent->lightingOrigin, origin );
	} else {
		VectorCopy( ent->origin, origin );
	}

	hasGrid = R_LightForPoint( origin, ambient, directed, lightDir );
	dlights = R_GetSceneDlights( &numDlights );

	if ( !hasGrid && numDlights == 0 ) {
		outColor[0] = outColor[1] = outColor[2] = outColor[3] = 255;
		return;
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
		VectorMA( directed, d, dlights[i].color, directed );
	}

	r = (int)( ambient[0] + directed[0] );
	g = (int)( ambient[1] + directed[1] );
	b = (int)( ambient[2] + directed[2] );

	outColor[0] = (byte)( r < 0 ? 0 : ( r > 255 ? 255 : r ) );
	outColor[1] = (byte)( g < 0 ? 0 : ( g > 255 ? 255 : g ) );
	outColor[2] = (byte)( b < 0 ? 0 : ( b > 255 ? 255 : b ) );
	outColor[3] = 255;
}

// customShader overrides every surface; customSkin looks the surface name up in a .skin file; otherwise each surface uses its own baked-in shader.
static qhandle_t ResolveShaderDirect( qhandle_t customShader, qhandle_t customSkin, const char *surfaceName, qhandle_t bakedShader ) {
	if ( customShader ) {
		return customShader;
	}
	if ( customSkin ) {
		return R_SkinShaderForSurface( R_GetSkinByHandle( customSkin ), surfaceName, bakedShader );
	}
	return bakedShader;
}

static qhandle_t ResolveShader( qhandle_t customShader, qhandle_t customSkin, const char *surfaceName, int numShaders, const void *shadersPtr ) {
	qhandle_t baked = ( numShaders > 0 ) ? (qhandle_t)( (const md3Shader_t *)shadersPtr )->shaderIndex : 0;
	return ResolveShaderDirect( customShader, customSkin, surfaceName, baked );
}

static qboolean ReserveSurface( int numVerts, int numTriangles, uint32_t vertWrite, uint32_t idxWrite, int numDraws ) {
	return numDraws < ENTITY_MAX_DRAWS && vertWrite + (uint32_t)numVerts <= ENTITY_MAX_VERTS &&
		   idxWrite + (uint32_t)numTriangles * 3 <= ENTITY_MAX_INDEXES;
}

// Copies triangle indices (local to this surface's own vertex range, matching vertexOffset at draw time) and records the draw call.
static void CommitSurface( int numVerts, int numTriangles, const void *trianglesPtr, qhandle_t shader,
							uint32_t *vertWrite, uint32_t *idxWrite, int *numDraws ) {
	const md3Triangle_t *tri = (const md3Triangle_t *)trianglesPtr;
	int i;

	for ( i = 0; i < numTriangles; i++, tri++ ) {
		s_indexMapped[*idxWrite + (uint32_t)i * 3 + 0] = (uint32_t)tri->indexes[0];
		s_indexMapped[*idxWrite + (uint32_t)i * 3 + 1] = (uint32_t)tri->indexes[1];
		s_indexMapped[*idxWrite + (uint32_t)i * 3 + 2] = (uint32_t)tri->indexes[2];
	}

	s_draws[*numDraws].vertBase = *vertWrite;
	s_draws[*numDraws].idxBase = *idxWrite;
	s_draws[*numDraws].idxCount = (uint32_t)numTriangles * 3;
	s_draws[*numDraws].shader = shader;
	(*numDraws)++;

	*vertWrite += (uint32_t)numVerts;
	*idxWrite += (uint32_t)numTriangles * 3;
}

void vk_entity_draw_all( const refEntity_t *entities, int numEntities, const float mvp[16], const float fogPush[12] ) {
	uint32_t vertWrite = 0, idxWrite = 0;
	int numDraws = 0;
	int e, s, i;
	static qboolean overflowLogged = qfalse;

	for ( e = 0; e < numEntities; e++ ) {
		const refEntity_t *ent = &entities[e];
		model_t *model = R_GetModelByHandle( ent->hModel );
		byte tint[4];

		if ( !model ) {
			continue;
		}
		ComputeEntityTint( ent, tint );

		if ( model->type == MOD_MD3 && model->md3 ) {
			const md3Surface_t *surf = (const md3Surface_t *)( (const byte *)model->md3 + model->md3->ofsSurfaces );
			int frame = ent->frame, oldframe = ent->oldframe;
			if ( frame < 0 || frame >= model->md3->numFrames ) frame = 0;
			if ( oldframe < 0 || oldframe >= model->md3->numFrames ) oldframe = 0;

			for ( s = 0; s < model->md3->numSurfaces; s++ ) {
				if ( !ReserveSurface( surf->numVerts, surf->numTriangles, vertWrite, idxWrite, numDraws ) ) {
					goto overflow;
				}

				LerpSurfaceVerts( ent, surf, frame, oldframe, ent->backlerp, tint, &s_vertexMapped[vertWrite] );
				CommitSurface( surf->numVerts, surf->numTriangles, (const byte *)surf + surf->ofsTriangles,
					ResolveShader( ent->customShader, ent->customSkin, surf->name, surf->numShaders, (const byte *)surf + surf->ofsShaders ),
					&vertWrite, &idxWrite, &numDraws );

				surf = (const md3Surface_t *)( (const byte *)surf + surf->ofsEnd );
			}
		} else if ( model->type == MOD_MDC && model->mdc ) {
			const mdcSurface_t *surf = (const mdcSurface_t *)( (const byte *)model->mdc + model->mdc->ofsSurfaces );
			int frame = ent->frame, oldframe = ent->oldframe;
			if ( frame < 0 || frame >= model->mdc->numFrames ) frame = 0;
			if ( oldframe < 0 || oldframe >= model->mdc->numFrames ) oldframe = 0;

			for ( s = 0; s < model->mdc->numSurfaces; s++ ) {
				if ( !ReserveSurface( surf->numVerts, surf->numTriangles, vertWrite, idxWrite, numDraws ) ) {
					goto overflow;
				}

				LerpMDCSurfaceVerts( ent, surf, frame, oldframe, ent->backlerp, tint, &s_vertexMapped[vertWrite] );
				CommitSurface( surf->numVerts, surf->numTriangles, (const byte *)surf + surf->ofsTriangles,
					ResolveShader( ent->customShader, ent->customSkin, surf->name, surf->numShaders, (const byte *)surf + surf->ofsShaders ),
					&vertWrite, &idxWrite, &numDraws );

				surf = (const mdcSurface_t *)( (const byte *)surf + surf->ofsEnd );
			}
		} else if ( model->type == MOD_MDS && model->mds ) {
			const mdsSurface_t *surf = (const mdsSurface_t *)( (const byte *)model->mds + model->mds->ofsSurfaces );

			for ( s = 0; s < model->mds->numSurfaces; s++ ) {
				if ( !ReserveSurface( surf->numVerts, surf->numTriangles, vertWrite, idxWrite, numDraws ) ) {
					goto overflow;
				}

				LerpMDSSurfaceVerts( ent, model->mds, surf, tint, &s_vertexMapped[vertWrite] );
				CommitSurface( surf->numVerts, surf->numTriangles, (const byte *)surf + surf->ofsTriangles,
					ResolveShaderDirect( ent->customShader, ent->customSkin, surf->name, surf->shaderIndex ),
					&vertWrite, &idxWrite, &numDraws );

				surf = (const mdsSurface_t *)( (const byte *)surf + surf->ofsEnd );
			}
		} else if ( model->type == MOD_MDM && model->mdm ) {
			const mdmSurface_t *surf = (const mdmSurface_t *)( (const byte *)model->mdm + model->mdm->ofsSurfaces );

			for ( s = 0; s < model->mdm->numSurfaces; s++ ) {
				if ( !ReserveSurface( surf->numVerts, surf->numTriangles, vertWrite, idxWrite, numDraws ) ) {
					goto overflow;
				}

				LerpMDMSurfaceVerts( ent, surf, tint, &s_vertexMapped[vertWrite] );
				CommitSurface( surf->numVerts, surf->numTriangles, (const byte *)surf + surf->ofsTriangles,
					ResolveShaderDirect( ent->customShader, ent->customSkin, surf->name, surf->shaderIndex ),
					&vertWrite, &idxWrite, &numDraws );

				surf = (const mdmSurface_t *)( (const byte *)surf + surf->ofsEnd );
			}
		}
	}

	goto doneCollecting;

overflow:
	if ( !overflowLogged ) {
		ri.Printf( PRINT_WARNING, "vk_entity_draw_all: scratch buffer full, dropping remaining entity surfaces this frame\n" );
		overflowLogged = qtrue;
	}

doneCollecting:
	if ( numDraws == 0 ) {
		return;
	}

	{
		float push_data[32];
		VkDeviceSize vb_offset = 0;
		VkPipeline boundPipeline = VK_NULL_HANDLE;

		memset( push_data, 0, sizeof( push_data ) );
		memcpy( push_data, mvp, 16 * sizeof( float ) );
		memcpy( push_data + 16, fogPush, 12 * sizeof( float ) );
		qvkCmdPushConstants( vk.command_buffer, vk.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
			0, sizeof( push_data ), push_data );

		qvkCmdBindVertexBuffers( vk.command_buffer, 0, 1, &s_vertexBuffer, &vb_offset );
		qvkCmdBindIndexBuffer( vk.command_buffer, s_indexBuffer, 0, VK_INDEX_TYPE_UINT32 );

		for ( i = 0; i < numDraws; i++ ) {
			shader_t *shaderPtr = R_GetShaderByHandle( s_draws[i].shader );
			int s;

			if ( !shaderPtr || shaderPtr->numStages < 1 ) {
				continue;
			}

			// Every stage redraws the same indices/vertices, layered in shader-script order.
			for ( s = 0; s < shaderPtr->numStages; s++ ) {
				const shaderStage_t *stage = &shaderPtr->stages[s];
				VkPipeline pipeline;

				if ( !stage->image || stage->image->vk_descriptor == VK_NULL_HANDLE ) {
					continue;
				}

				pipeline = vk_getPipeline3D( stage->blendEnabled, stage->srcBlend, stage->dstBlend,
					vk_cullModeForShader( shaderPtr->cullType ), stage->alphaTestFunc );
				if ( pipeline != boundPipeline ) {
					qvkCmdBindPipeline( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline );
					boundPipeline = pipeline;
				}

				qvkCmdBindDescriptorSets( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout,
					0, 1, &stage->image->vk_descriptor, 0, NULL );
				qvkCmdDrawIndexed( vk.command_buffer, s_draws[i].idxCount, 1, s_draws[i].idxBase, (int32_t)s_draws[i].vertBase, 0 );
			}
		}
	}
}
