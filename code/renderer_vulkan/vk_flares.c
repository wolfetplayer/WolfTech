// vk_flares.c -- coronas (RE_AddCoronaToScene). See vk_flares.h.
//
// Visibility comes from a 1-sample occlusion query per corona instead of the classic GL1 renderer's
// glReadPixels-the-depth-buffer trick (code/renderer/tr_flares.c's RB_TestFlare): a tiny depth-tested,
// color/depth-write-disabled quad is drawn at the corona's position wrapped in vkCmdBeginQuery/EndQuery, and the
// *previous* frame's result is read back with vkGetQueryPoolResults. That read never blocks: vk_begin_frame()
// already waits on the previous frame's fence before this frame starts recording, so by the time a query is
// read here the GPU work that wrote it has long since finished. Visible/hidden transitions fade linearly over
// FLARE_FADE_MS rather than popping, same intent as the reference's drawIntensity.
//
// The billboard itself is a camera-facing world-space quad drawn through flareShader's real stages (so blend/
// cull/alphaTest/tcMod come from the actual shader script), projected by the GPU like any other world-space
// sprite in this renderer -- not a dedicated screen-space pixel-constant-size overlay pass like the reference.
// That's a deliberate scope cut: it shrinks with distance like every other scene effect here, instead of RTCW's
// distance-independent flareCoeff intensity curve.

#include <string.h>
#include <math.h>

#include "VKimpl.h"
#include "vk_instance.h"
#include "vk_pipelines.h"
#include "tr_shader.h"
#include "tr_shade_calc.h"
#include "tr_scene.h"
#include "tr_image.h"
#include "vk_flares.h"

#define MAX_FLARES MAX_SCENE_CORONAS
#define FLARE_SCRATCH_VERTS ( MAX_FLARES * 6 * 2 ) // one quad (6 verts) each for this frame's probes + billboards

#define FLARE_BASE_WORLD_SIZE 6.0f // world units at corona scale 1.0; shrinks with distance like any other sprite
#define FLARE_PROBE_MIN_HALF_SIZE 1.0f
#define FLARE_FADE_MS 300

typedef struct {
	qboolean inUse;
	int id;
	int lastSeenFrame; // s_flareFrame value this id was last seen in the scene corona list
	qboolean visible; // state of last test
	int fadeStartTime; // fd->time the current visible/hidden state began
	float drawIntensity; // 0..1; may be nonzero while fading out even after visible has gone false
	vec3_t origin;
	vec3_t color;
	float scale;
} flare_t;

static flare_t s_flares[MAX_FLARES];
static int s_flareFrame;

static VkQueryPool s_queryPool;
static qboolean s_queryPoolReady;

static VkBuffer s_vertexBuffer;
static VkDeviceMemory s_vertexMemory;
static vk2d_vertex_t *s_mapped;
static uint32_t s_writeOffset;

static cvar_t *r_flares;

static qhandle_t s_flareShaderHandle;
static qboolean s_flareShaderLooked;

extern uint32_t vk_find_memory_type( uint32_t type_bits, VkMemoryPropertyFlags properties );

void vk_flares_init( void ) {
	VkQueryPoolCreateInfo pool_desc;
	VkBufferCreateInfo buf_desc;
	VkMemoryRequirements mem_reqs;
	VkMemoryAllocateInfo alloc_info;

	r_flares = ri.Cvar_Get( "r_flares", "1", CVAR_ARCHIVE );

	memset( &pool_desc, 0, sizeof( pool_desc ) );
	pool_desc.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
	pool_desc.queryType = VK_QUERY_TYPE_OCCLUSION;
	pool_desc.queryCount = MAX_FLARES;
	VK_CHECK( qvkCreateQueryPool( vk.device, &pool_desc, NULL, &s_queryPool ) );
	s_queryPoolReady = qtrue;

	buf_desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buf_desc.pNext = NULL;
	buf_desc.flags = 0;
	buf_desc.size = FLARE_SCRATCH_VERTS * sizeof( vk2d_vertex_t );
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

	vk_flares_clear();
}

void vk_flares_shutdown( void ) {
	if ( s_queryPoolReady ) {
		qvkDestroyQueryPool( vk.device, s_queryPool, NULL );
		s_queryPoolReady = qfalse;
	}
	if ( s_mapped ) {
		qvkUnmapMemory( vk.device, s_vertexMemory );
		s_mapped = NULL;
	}
	qvkDestroyBuffer( vk.device, s_vertexBuffer, NULL );
	qvkFreeMemory( vk.device, s_vertexMemory, NULL );
}

void vk_flares_clear( void ) {
	memset( s_flares, 0, sizeof( s_flares ) );
	s_flareFrame = 0;
}

void vk_flares_begin_frame( void ) {
	s_writeOffset = 0;
	if ( !s_queryPoolReady ) {
		return;
	}
	qvkCmdResetQueryPool( vk.command_buffer, s_queryPool, 0, MAX_FLARES );
}

static flare_t *FindOrAllocFlare( int id ) {
	int i, oldest = 0;
	flare_t *free_slot = NULL;

	for ( i = 0; i < MAX_FLARES; i++ ) {
		if ( s_flares[i].inUse && s_flares[i].id == id ) {
			return &s_flares[i];
		}
		if ( !s_flares[i].inUse && !free_slot ) {
			free_slot = &s_flares[i];
		}
		if ( s_flares[i].lastSeenFrame < s_flares[oldest].lastSeenFrame ) {
			oldest = i;
		}
	}

	// table full -- steal the stalest slot rather than dropping the newest corona
	free_slot = free_slot ? free_slot : &s_flares[oldest];
	memset( free_slot, 0, sizeof( *free_slot ) );
	return free_slot;
}

static void BuildBillboardCorners( vec3_t out[4], const vec3_t origin, const vec3_t right, const vec3_t up, float halfSize ) {
	vec3_t rightScaled, upScaled;

	VectorScale( right, halfSize, rightScaled );
	VectorScale( up, halfSize, upScaled );

	VectorSubtract( origin, rightScaled, out[0] ); VectorSubtract( out[0], upScaled, out[0] );
	VectorAdd( origin, rightScaled, out[1] );      VectorSubtract( out[1], upScaled, out[1] );
	VectorAdd( origin, rightScaled, out[2] );      VectorAdd( out[2], upScaled, out[2] );
	VectorSubtract( origin, rightScaled, out[3] ); VectorAdd( out[3], upScaled, out[3] );
}

// Fan-triangulates the quad's 4 corners into 6 verts (2 triangles), matching vk_poly.c's non-indexed convention.
static void EmitQuadFan( vk2d_vertex_t *out, const vec3_t corners[4], const byte colors[4][4], const float uvs[4][2] ) {
	static const int order[6] = { 0, 1, 2, 0, 2, 3 };
	int i;

	for ( i = 0; i < 6; i++ ) {
		int c = order[i];
		VectorCopy( corners[c], out[i].pos );
		out[i].color[0] = colors[c][0]; out[i].color[1] = colors[c][1]; out[i].color[2] = colors[c][2]; out[i].color[3] = colors[c][3];
		out[i].uv[0] = uvs[c][0]; out[i].uv[1] = uvs[c][1];
	}
}

static void PushTransform( const float viewProj[16], const float fogPush[12] ) {
	float push_data[32];

	memset( push_data, 0, sizeof( push_data ) );
	memcpy( push_data, viewProj, 16 * sizeof( float ) );
	memcpy( push_data + 16, fogPush, 12 * sizeof( float ) );
	qvkCmdPushConstants( vk.command_buffer, vk.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		0, sizeof( push_data ), push_data );
}

// Depth-tests a tiny quad at the corona's position, wrapped in an occlusion query read back next frame (see
// this file's header comment). Skipped if the corona is behind the camera -- clip-space w<=0 there, not worth
// the projection math just to discard it.
static void IssueOcclusionProbe( const vec3_t origin, const vec3_t right, const vec3_t up, float distance, uint32_t queryIndex ) {
	vec3_t corners[4];
	static const byte white[4][4] = { { 255, 255, 255, 255 }, { 255, 255, 255, 255 }, { 255, 255, 255, 255 }, { 255, 255, 255, 255 } };
	static const float uv[4][2] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };
	float halfSize = distance * 0.01f;

	if ( halfSize < FLARE_PROBE_MIN_HALF_SIZE ) {
		halfSize = FLARE_PROBE_MIN_HALF_SIZE;
	}
	if ( s_writeOffset + 6 > FLARE_SCRATCH_VERTS ) {
		return;
	}

	BuildBillboardCorners( corners, origin, right, up, halfSize );
	EmitQuadFan( &s_mapped[s_writeOffset], corners, white, uv );

	qvkCmdBeginQuery( vk.command_buffer, s_queryPool, queryIndex, 0 );
	qvkCmdDraw( vk.command_buffer, 6, 1, s_writeOffset, 0 );
	qvkCmdEndQuery( vk.command_buffer, s_queryPool, queryIndex );

	s_writeOffset += 6;
}

static qboolean ReadLastProbeResult( uint32_t queryIndex ) {
	uint64_t sampleCount = 0;

	if ( qvkGetQueryPoolResults( vk.device, s_queryPool, queryIndex, 1, sizeof( sampleCount ), &sampleCount,
			sizeof( sampleCount ), VK_QUERY_RESULT_64_BIT ) != VK_SUCCESS ) {
		return qfalse; // not ready yet (e.g. this slot was never issued) -- treat as "not visible" this frame
	}
	return sampleCount > 0;
}

static void UpdateFade( flare_t *f, qboolean nowVisible, int timeMs ) {
	float elapsed, fade;

	if ( nowVisible != f->visible ) {
		f->visible = nowVisible;
		f->fadeStartTime = timeMs;
	}

	elapsed = (float)( timeMs - f->fadeStartTime ) / (float)FLARE_FADE_MS;
	fade = f->visible ? elapsed : 1.0f - elapsed;
	f->drawIntensity = fade < 0.0f ? 0.0f : ( fade > 1.0f ? 1.0f : fade );
}

static void DrawBillboard( const flare_t *f, const vec3_t right, const vec3_t up, VkPipeline *boundPipeline, image_t **boundImage ) {
	shader_t *shader;
	vec3_t corners[4];
	static const float baseUV[4][2] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };
	byte baseColor[4];
	int s, c;
	float halfSize = FLARE_BASE_WORLD_SIZE * f->scale * f->drawIntensity;

	if ( !s_flareShaderLooked ) {
		s_flareShaderHandle = RE_RegisterShaderInternal( "flareShader", qfalse );
		s_flareShaderLooked = qtrue;
	}
	shader = R_GetShaderByHandle( s_flareShaderHandle );

	if ( halfSize <= 0.0f ) {
		return;
	}

	BuildBillboardCorners( corners, f->origin, right, up, halfSize );

	baseColor[0] = (byte)( MIN( MAX( f->color[0], 0.0f ), 1.0f ) * f->drawIntensity * 255.0f );
	baseColor[1] = (byte)( MIN( MAX( f->color[1], 0.0f ), 1.0f ) * f->drawIntensity * 255.0f );
	baseColor[2] = (byte)( MIN( MAX( f->color[2], 0.0f ), 1.0f ) * f->drawIntensity * 255.0f );
	baseColor[3] = 255;

	for ( s = 0; s < shader->numStages; s++ ) {
		const shaderStage_t *stage = &shader->stages[s];
		image_t *stageImage = R_GetStageImage( stage );
		byte stageColors[4][4];
		float stageUVs[4][2];
		VkPipeline pipeline;

		if ( !stageImage || stageImage->vk_descriptor == VK_NULL_HANDLE || s_writeOffset + 6 > FLARE_SCRATCH_VERTS ) {
			continue;
		}

		for ( c = 0; c < 4; c++ ) {
			R_CalcStageColorAlpha( stage, NULL, NULL, baseColor, stageColors[c] );
			R_CalcStageTexCoord( stage, baseUV[c][0], baseUV[c][1], corners[c], NULL, stageUVs[c] );
		}

		EmitQuadFan( &s_mapped[s_writeOffset], corners, stageColors, stageUVs );

		pipeline = vk_getPipeline3D( sizeof( vk2d_vertex_t ), stage->blendEnabled, stage->srcBlend, stage->dstBlend,
			vk_cullModeForShader( shader->cullType ), stage->alphaTestFunc, VK_FALSE, VK_FALSE, shader->polygonOffset );
		if ( pipeline != *boundPipeline ) {
			qvkCmdBindPipeline( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline );
			*boundPipeline = pipeline;
		}
		if ( stageImage != *boundImage ) {
			qvkCmdBindDescriptorSets( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout,
				0, 1, &stageImage->vk_descriptor, 0, NULL );
			*boundImage = stageImage;
		}
		qvkCmdDraw( vk.command_buffer, 6, 1, s_writeOffset, 0 );
		s_writeOffset += 6;
	}
}

void vk_flares_draw( const refdef_t *fd, const float viewProj[16], const float fogPush[12] ) {
	int numCoronas, i;
	const sceneCorona_t *coronas;
	vec3_t right, up;
	VkDeviceSize vb_offset = 0;
	VkPipeline boundPipeline = VK_NULL_HANDLE;
	image_t *boundImage = NULL;

	if ( !r_flares->integer || !s_queryPoolReady ) {
		return;
	}

	coronas = R_GetSceneCoronas( &numCoronas );
	VectorCopy( fd->viewaxis[1], right );
	VectorCopy( fd->viewaxis[2], up );

	s_flareFrame++;

	qvkCmdBindVertexBuffers( vk.command_buffer, 0, 1, &s_vertexBuffer, &vb_offset );

	// Pass 1: this frame's scene coronas -- read back last frame's probe, fade, issue this frame's probe.
	if ( numCoronas > 0 ) {
		PushTransform( viewProj, fogPush );
		qvkCmdBindPipeline( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_occlusion_probe );
		qvkCmdBindDescriptorSets( vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout,
			0, 1, &R_GetDefaultImage()->vk_descriptor, 0, NULL );
	}

	for ( i = 0; i < numCoronas; i++ ) {
		const sceneCorona_t *src = &coronas[i];
		flare_t *f = FindOrAllocFlare( src->id );
		vec3_t toFlare;
		float distance;
		qboolean inFront, callerVisible, nowVisible;
		uint32_t queryIndex = (uint32_t)( f - s_flares );

		VectorSubtract( src->origin, fd->vieworg, toFlare );
		distance = VectorLength( toFlare );
		inFront = DotProduct( toFlare, fd->viewaxis[0] ) > 0.0f;
		callerVisible = ( src->flags & 1 ) != 0;

		if ( !f->inUse || f->lastSeenFrame != s_flareFrame - 1 ) {
			// brand new, or this id dropped out and came back -- start a fresh fade instead of popping
			f->visible = qfalse;
			f->fadeStartTime = fd->time - FLARE_FADE_MS;
		}

		nowVisible = inFront && callerVisible && ReadLastProbeResult( queryIndex );
		UpdateFade( f, nowVisible, fd->time );

		f->inUse = qtrue;
		f->id = src->id;
		f->lastSeenFrame = s_flareFrame;
		VectorCopy( src->origin, f->origin );
		VectorCopy( src->color, f->color );
		f->scale = src->scale;

		// Issued unconditionally (even if the caller's flag says hidden) so a corona that goes hidden still
		// fades out properly next time it's visible again, instead of popping -- see this file's header comment.
		if ( inFront ) {
			IssueOcclusionProbe( src->origin, right, up, distance, queryIndex );
		}
	}

	// Pass 2: ids that stopped being added this frame (left the snapshot/PVS) -- keep fading, don't pop.
	for ( i = 0; i < MAX_FLARES; i++ ) {
		flare_t *f = &s_flares[i];

		if ( !f->inUse || f->lastSeenFrame == s_flareFrame ) {
			continue;
		}
		UpdateFade( f, qfalse, fd->time );
		if ( f->drawIntensity <= 0.0f ) {
			f->inUse = qfalse;
		}
	}

	// Pass 3: draw whatever's still got nonzero intensity, through flareShader's real stages.
	for ( i = 0; i < MAX_FLARES; i++ ) {
		const flare_t *f = &s_flares[i];
		if ( f->inUse && f->drawIntensity > 0.0f ) {
			DrawBillboard( f, right, up, &boundPipeline, &boundImage );
		}
	}
}
