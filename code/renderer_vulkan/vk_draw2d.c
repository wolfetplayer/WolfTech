// vk_draw2d.c -- immediate-mode 2D quad recording.

#include <string.h>
#include <math.h>

#include "VKimpl.h"
#include "vk_instance.h"
#include "vk_pipelines.h"
#include "vk_frame.h"
#include "vk_cmd.h"
#include "glConfig.h"
#include "tr_shade_calc.h"
#include "vk_draw2d.h"

#define VK2D_VERTEX_BUFFER_BYTES ( 1024 * 1024 )
#define VK2D_MAX_VERTS ( VK2D_VERTEX_BUFFER_BYTES / sizeof( vk2d_vertex_t ) )

static VkBuffer s_vertexBuffer;
static VkDeviceMemory s_vertexMemory;
static vk2d_vertex_t *s_mapped;
static uint32_t s_writeIndex;
static uint32_t s_regionBase; // vk_current_frame_slot() * VK2D_MAX_VERTS -- see vk_draw2d_begin_frame

#define VK2D_BINDLESS_MAX_VERTS ( VK2D_VERTEX_BUFFER_BYTES / sizeof( vk2dBindless_vertex_t ) )
static VkBuffer s_bindlessVertexBuffer;
static VkDeviceMemory s_bindlessVertexMemory;
static vk2dBindless_vertex_t *s_bindlessMapped;
static uint32_t s_bindlessWriteIndex;
static uint32_t s_bindlessRegionBase;

static float s_color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

extern uint32_t vk_find_memory_type( uint32_t type_bits, VkMemoryPropertyFlags properties );

void vk_draw2d_init( void ) {
	VkBufferCreateInfo buf_desc;
	buf_desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buf_desc.pNext = NULL;
	buf_desc.flags = 0;
	buf_desc.size = (VkDeviceSize)VK2D_VERTEX_BUFFER_BYTES * VK_FRAMES_IN_FLIGHT;
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

	s_writeIndex = 0;

	if ( vk.bindless ) {
		VkBufferCreateInfo bbuf_desc;
		VkMemoryRequirements bmem_reqs;
		VkMemoryAllocateInfo balloc_info;

		bbuf_desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bbuf_desc.pNext = NULL;
		bbuf_desc.flags = 0;
		bbuf_desc.size = (VkDeviceSize)VK2D_VERTEX_BUFFER_BYTES * VK_FRAMES_IN_FLIGHT;
		bbuf_desc.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		bbuf_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		bbuf_desc.queueFamilyIndexCount = 0;
		bbuf_desc.pQueueFamilyIndices = NULL;
		VK_CHECK( qvkCreateBuffer( vk.device, &bbuf_desc, NULL, &s_bindlessVertexBuffer ) );

		qvkGetBufferMemoryRequirements( vk.device, s_bindlessVertexBuffer, &bmem_reqs );

		balloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		balloc_info.pNext = NULL;
		balloc_info.allocationSize = bmem_reqs.size;
		balloc_info.memoryTypeIndex = vk_find_memory_type( bmem_reqs.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
		VK_CHECK( qvkAllocateMemory( vk.device, &balloc_info, NULL, &s_bindlessVertexMemory ) );
		VK_CHECK( qvkBindBufferMemory( vk.device, s_bindlessVertexBuffer, s_bindlessVertexMemory, 0 ) );

		VK_CHECK( qvkMapMemory( vk.device, s_bindlessVertexMemory, 0, VK_WHOLE_SIZE, 0, (void **)&s_bindlessMapped ) );

		s_bindlessWriteIndex = 0;
	}
}

void vk_draw2d_shutdown( void ) {
	if ( s_mapped ) {
		qvkUnmapMemory( vk.device, s_vertexMemory );
		s_mapped = NULL;
	}
	qvkDestroyBuffer( vk.device, s_vertexBuffer, NULL );
	qvkFreeMemory( vk.device, s_vertexMemory, NULL );

	if ( vk.bindless ) {
		if ( s_bindlessMapped ) {
			qvkUnmapMemory( vk.device, s_bindlessVertexMemory );
			s_bindlessMapped = NULL;
		}
		qvkDestroyBuffer( vk.device, s_bindlessVertexBuffer, NULL );
		qvkFreeMemory( vk.device, s_bindlessVertexMemory, NULL );
	}
}

void vk_draw2d_begin_frame( void ) {
	int width, height;
	R_GetWinResolution( &width, &height );

	s_writeIndex = 0;
	s_regionBase = vk_current_frame_slot() * (uint32_t)VK2D_MAX_VERTS;

	if ( vk.bindless ) {
		s_bindlessWriteIndex = 0;
		s_bindlessRegionBase = vk_current_frame_slot() * (uint32_t)VK2D_BINDLESS_MAX_VERTS;
	}

	VkViewport viewport;
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)width;
	viewport.height = (float)height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	qvkCmdSetViewport( vk.command_buffer, 0, 1, &viewport );

	VkRect2D scissor;
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = (uint32_t)width;
	scissor.extent.height = (uint32_t)height;
	qvkCmdSetScissor( vk.command_buffer, 0, 1, &scissor );
}

void vk_draw2d_set_color( const float *rgba ) {
	if ( rgba ) {
		s_color[0] = rgba[0];
		s_color[1] = rgba[1];
		s_color[2] = rgba[2];
		s_color[3] = rgba[3];
	} else {
		s_color[0] = s_color[1] = s_color[2] = s_color[3] = 1.0f;
	}
}

static byte float_to_byte( float f ) {
	int i = (int)( f * 255.0f + 0.5f );
	if ( i < 0 ) return 0;
	if ( i > 255 ) return 255;
	return (byte)i;
}

#define RPIC_CORNER_SEGS   8
#define RPIC_NUM_BOUNDARY  ( 4 * ( RPIC_CORNER_SEGS + 1 ) )
#define RPIC_FEATHER       1.0f

static void PushBindlessScaleOffset( void ) {
	int width, height;
	float scaleOffset[4];

	R_GetWinResolution( &width, &height );
	scaleOffset[0] = 2.0f / (float)width;
	scaleOffset[1] = 2.0f / (float)height;
	scaleOffset[2] = -1.0f;
	scaleOffset[3] = -1.0f;
	qvkCmdPushConstants( vk.command_buffer, vk.ui_bindless_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		0, sizeof( scaleOffset ), scaleOffset );
}

static qboolean vk_draw2d_record_tris_bindless( const vk2d_vertex_t *verts, int count, shader_t *shader, qboolean forceBlend, image_t *stageImage ) {
	vk2dBindless_vertex_t bverts[RPIC_NUM_BOUNDARY * 9];
	VkPipeline pipeline;
	VkDeviceSize vb_offset;
	int i;

	if ( stageImage->vk_bindless_index < 0 ) {
		return qfalse;
	}
	if ( s_bindlessWriteIndex + (uint32_t)count > VK2D_BINDLESS_MAX_VERTS ) {
		return qtrue; // out of room this frame, drop rather than overrun
	}

	for ( i = 0; i < count; i++ ) {
		bverts[i].pos[0] = verts[i].pos[0];
		bverts[i].pos[1] = verts[i].pos[1];
		bverts[i].pos[2] = verts[i].pos[2];
		memcpy( bverts[i].color, verts[i].color, 4 );
		bverts[i].uv[0] = verts[i].uv[0];
		bverts[i].uv[1] = verts[i].uv[1];
		bverts[i].texture = (uint32_t)stageImage->vk_bindless_index;
	}

	vb_offset = (VkDeviceSize)( s_bindlessRegionBase + s_bindlessWriteIndex ) * sizeof( vk2dBindless_vertex_t );
	memcpy( &s_bindlessMapped[s_bindlessRegionBase + s_bindlessWriteIndex], bverts, (size_t)count * sizeof( vk2dBindless_vertex_t ) );
	s_bindlessWriteIndex += (uint32_t)count;

	PushBindlessScaleOffset();

	pipeline = forceBlend ? vk.pipeline_2d_bindless_blend
		: vk_getPipeline2DBindless( shader->stages[0].blendEnabled, shader->stages[0].srcBlend, shader->stages[0].dstBlend );
	vk_bind_pipeline_cached( pipeline );

	vk_bind_descriptor_set_cached( vk.ui_bindless_pipeline_layout, 0, vk.bindless_set, 0, NULL );

	qvkCmdBindVertexBuffers( vk.command_buffer, 0, 1, &s_bindlessVertexBuffer, &vb_offset );

	qvkCmdDraw( vk.command_buffer, (uint32_t)count, 1, 0, 0 );
	return qtrue;
}

// Non-indexed triangle list, shared by every DrawStretchPic* variant.
static void vk_draw2d_record_tris( const vk2d_vertex_t *verts, int count, shader_t *shader, qboolean forceBlend ) {
	int width, height;
	float clip_space_xform[32];
	VkPipeline pipeline;
	VkDeviceSize vb_offset;
	image_t *stageImage;

	if ( !shader || shader->numStages < 1 ) {
		return;
	}
	stageImage = R_GetStageImage( &shader->stages[0] );
	if ( !stageImage ) {
		return;
	}

	if ( vk.bindless && vk_draw2d_record_tris_bindless( verts, count, shader, forceBlend, stageImage ) ) {
		return;
	}

	if ( s_writeIndex + (uint32_t)count > VK2D_MAX_VERTS ) {
		return; // out of room this frame, drop rather than overrun
	}

	vb_offset = (VkDeviceSize)( s_regionBase + s_writeIndex ) * sizeof( vk2d_vertex_t );
	memcpy( &s_mapped[s_regionBase + s_writeIndex], verts, (size_t)count * sizeof( vk2d_vertex_t ) );
	s_writeIndex += (uint32_t)count;

	R_GetWinResolution( &width, &height );

	memset( clip_space_xform, 0, sizeof( clip_space_xform ) );
	// column-major mat4: screen pixels (0,0 top-left) -> Vulkan NDC, no Y-flip needed
	clip_space_xform[0] = 2.0f / (float)width;   // col0.x
	clip_space_xform[5] = 2.0f / (float)height;  // col1.y
	clip_space_xform[10] = 1.0f;                 // col2.z
	clip_space_xform[12] = -1.0f;                // col3.x
	clip_space_xform[13] = -1.0f;                // col3.y
	clip_space_xform[15] = 1.0f;                 // col3.w

	qvkCmdPushConstants( vk.command_buffer, vk.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		0, sizeof( clip_space_xform ), clip_space_xform );

	pipeline = forceBlend ? vk.pipeline_2d_blend
		: vk_getPipeline2D( shader->stages[0].blendEnabled, shader->stages[0].srcBlend, shader->stages[0].dstBlend );
	vk_bind_pipeline_cached( pipeline );

	vk_bind_descriptor_set_cached( vk.pipeline_layout, 0, stageImage->vk_descriptor, 0, NULL );

	qvkCmdBindVertexBuffers( vk.command_buffer, 0, 1, &s_vertexBuffer, &vb_offset );

	qvkCmdDraw( vk.command_buffer, (uint32_t)count, 1, 0, 0 );
}

// corners/uvs are in top-left, top-right, bottom-right, bottom-left order.
static void vk_draw2d_quad_generic( const float corners[4][2], const float uvs[4][2], shader_t *shader ) {
	vk2d_vertex_t verts[6];
	byte color[4];
	static const int order[6] = { 0, 1, 2, 0, 2, 3 };
	int i;

	color[0] = float_to_byte( s_color[0] );
	color[1] = float_to_byte( s_color[1] );
	color[2] = float_to_byte( s_color[2] );
	color[3] = float_to_byte( s_color[3] );

	// two triangles: (tl,tr,br) and (tl,br,bl), clockwise as seen on screen
	for ( i = 0; i < 6; i++ ) {
		int c = order[i];
		verts[i].pos[0] = corners[c][0];
		verts[i].pos[1] = corners[c][1];
		verts[i].pos[2] = 0.0f;
		verts[i].uv[0] = uvs[c][0];
		verts[i].uv[1] = uvs[c][1];
		memcpy( verts[i].color, color, 4 );
	}

	vk_draw2d_record_tris( verts, 6, shader, qfalse );
}

void vk_draw2d_quad( float x, float y, float w, float h,
					  float s1, float t1, float s2, float t2,
					  shader_t *shader ) {
	float corners[4][2] = {
		{ x, y }, { x + w, y }, { x + w, y + h }, { x, y + h }
	};
	float uvs[4][2] = {
		{ s1, t1 }, { s2, t1 }, { s2, t2 }, { s1, t2 }
	};
	vk_draw2d_quad_generic( corners, uvs, shader );
}

void vk_draw2d_quad_skewed( float x, float y, float w, float h,
							 float s1, float t1, float s2, float t2,
							 shader_t *shader, float skewLeft, float skewRight ) {
	float corners[4][2] = {
		{ x + skewLeft, y }, { x + w + skewRight, y }, { x + w, y + h }, { x, y + h }
	};
	float uvs[4][2] = {
		{ s1, t1 }, { s2, t1 }, { s2, t2 }, { s1, t2 }
	};
	vk_draw2d_quad_generic( corners, uvs, shader );
}

// Diagonal half-size used for both radii, so every corner stays the same distance from center at any angle.
void vk_draw2d_quad_rotated( float x, float y, float w, float h,
							   float s1, float t1, float s2, float t2,
							   shader_t *shader, float angle ) {
	float corners[4][2];
	float uvs[4][2] = { { s1, t1 }, { s2, t1 }, { s2, t2 }, { s1, t2 } };
	float cx = x + w * 0.5f, cy = y + h * 0.5f;
	float r = sqrtf( ( w * 0.5f ) * ( w * 0.5f ) + ( h * 0.5f ) * ( h * 0.5f ) );
	float turn = angle * (float)( 2.0 * M_PI );
	int i;

	for ( i = 0; i < 4; i++ ) {
		float a = turn + (float)i * 0.25f * (float)( 2.0 * M_PI );
		corners[i][0] = cx + cosf( a ) * r;
		corners[i][1] = cy + sinf( a ) * r;
	}

	vk_draw2d_quad_generic( corners, uvs, shader );
}

// Top edge uses the current color, bottom edge uses gradientColor -- always top-to-bottom, matching the reference.
void vk_draw2d_quad_gradient( float x, float y, float w, float h,
								float s1, float t1, float s2, float t2,
								shader_t *shader, const float *gradientColor ) {
	static const float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	static const int order[6] = { 0, 1, 2, 0, 2, 3 };
	float corners[4][2] = { { x, y }, { x + w, y }, { x + w, y + h }, { x, y + h } };
	float uvs[4][2] = { { s1, t1 }, { s2, t1 }, { s2, t2 }, { s1, t2 } };
	byte cornerColors[4][4];
	vk2d_vertex_t verts[6];
	int i;

	if ( !gradientColor ) {
		gradientColor = white;
	}

	for ( i = 0; i < 2; i++ ) {
		cornerColors[i][0] = float_to_byte( s_color[0] );
		cornerColors[i][1] = float_to_byte( s_color[1] );
		cornerColors[i][2] = float_to_byte( s_color[2] );
		cornerColors[i][3] = float_to_byte( s_color[3] );
	}
	for ( i = 2; i < 4; i++ ) {
		cornerColors[i][0] = float_to_byte( gradientColor[0] );
		cornerColors[i][1] = float_to_byte( gradientColor[1] );
		cornerColors[i][2] = float_to_byte( gradientColor[2] );
		cornerColors[i][3] = float_to_byte( gradientColor[3] );
	}

	for ( i = 0; i < 6; i++ ) {
		int c = order[i];
		verts[i].pos[0] = corners[c][0];
		verts[i].pos[1] = corners[c][1];
		verts[i].pos[2] = 0.0f;
		verts[i].uv[0] = uvs[c][0];
		verts[i].uv[1] = uvs[c][1];
		memcpy( verts[i].color, cornerColors[c], 4 );
	}

	vk_draw2d_record_tris( verts, 6, shader, qfalse );
}

void vk_draw2d_quad_rounded( float x, float y, float w, float h,
							  float s1, float t1, float s2, float t2,
							  shader_t *shader, float radius ) {
	// 3 verts for the center fan triangle + 6 for the feather quad's two triangles
	vk2d_vertex_t verts[RPIC_NUM_BOUNDARY * 9];
	float innerX[RPIC_NUM_BOUNDARY], innerY[RPIC_NUM_BOUNDARY];
	float outerX[RPIC_NUM_BOUNDARY], outerY[RPIC_NUM_BOUNDARY];
	float u[RPIC_NUM_BOUNDARY], v[RPIC_NUM_BOUNDARY];
	byte color[4], colorFade[4];
	float cx[4], cy[4], startAngle[4];
	float midx, midy;
	int i, k, n;

	if ( radius > w * 0.5f ) radius = w * 0.5f;
	if ( radius > h * 0.5f ) radius = h * 0.5f;
	if ( radius < 0.0f ) radius = 0.0f;

	// corner circle centers and sweep start angle (degrees), clockwise: TL, TR, BR, BL
	cx[0] = x + radius;       cy[0] = y + radius;       startAngle[0] = 180.0f;
	cx[1] = x + w - radius;   cy[1] = y + radius;       startAngle[1] = 270.0f;
	cx[2] = x + w - radius;   cy[2] = y + h - radius;   startAngle[2] = 0.0f;
	cx[3] = x + radius;       cy[3] = y + h - radius;   startAngle[3] = 90.0f;

	k = 0;
	for ( i = 0; i < 4; i++ ) {
		int seg;
		for ( seg = 0; seg <= RPIC_CORNER_SEGS; seg++, k++ ) {
			float angle = DEG2RAD( startAngle[i] + ( seg * ( 90.0f / RPIC_CORNER_SEGS ) ) );
			float dirx = cosf( angle );
			float diry = sinf( angle );
			float px = cx[i] + dirx * radius;
			float py = cy[i] + diry * radius;

			innerX[k] = px;
			innerY[k] = py;
			outerX[k] = cx[i] + dirx * ( radius + RPIC_FEATHER );
			outerY[k] = cy[i] + diry * ( radius + RPIC_FEATHER );
			u[k] = s1 + ( s2 - s1 ) * ( ( px - x ) / w );
			v[k] = t1 + ( t2 - t1 ) * ( ( py - y ) / h );
		}
	}

	midx = x + w * 0.5f;
	midy = y + h * 0.5f;

	color[0] = float_to_byte( s_color[0] );
	color[1] = float_to_byte( s_color[1] );
	color[2] = float_to_byte( s_color[2] );
	color[3] = float_to_byte( s_color[3] );
	colorFade[0] = color[0];
	colorFade[1] = color[1];
	colorFade[2] = color[2];
	colorFade[3] = 0;

	n = 0;
	for ( i = 0; i < RPIC_NUM_BOUNDARY; i++ ) {
		int j = ( i + 1 ) % RPIC_NUM_BOUNDARY;

		// center fan triangle: center, inner[i], inner[j]
		verts[n].pos[0] = midx; verts[n].pos[1] = midy; verts[n].pos[2] = 0.0f;
		verts[n].uv[0] = ( s1 + s2 ) * 0.5f; verts[n].uv[1] = ( t1 + t2 ) * 0.5f;
		memcpy( verts[n].color, color, 4 ); n++;

		verts[n].pos[0] = innerX[i]; verts[n].pos[1] = innerY[i]; verts[n].pos[2] = 0.0f;
		verts[n].uv[0] = u[i]; verts[n].uv[1] = v[i];
		memcpy( verts[n].color, color, 4 ); n++;

		verts[n].pos[0] = innerX[j]; verts[n].pos[1] = innerY[j]; verts[n].pos[2] = 0.0f;
		verts[n].uv[0] = u[j]; verts[n].uv[1] = v[j];
		memcpy( verts[n].color, color, 4 ); n++;

		// feather quad, triangle 1: inner[i], outer[i], inner[j]
		verts[n].pos[0] = innerX[i]; verts[n].pos[1] = innerY[i]; verts[n].pos[2] = 0.0f;
		verts[n].uv[0] = u[i]; verts[n].uv[1] = v[i];
		memcpy( verts[n].color, color, 4 ); n++;

		verts[n].pos[0] = outerX[i]; verts[n].pos[1] = outerY[i]; verts[n].pos[2] = 0.0f;
		verts[n].uv[0] = u[i]; verts[n].uv[1] = v[i];
		memcpy( verts[n].color, colorFade, 4 ); n++;

		verts[n].pos[0] = innerX[j]; verts[n].pos[1] = innerY[j]; verts[n].pos[2] = 0.0f;
		verts[n].uv[0] = u[j]; verts[n].uv[1] = v[j];
		memcpy( verts[n].color, color, 4 ); n++;

		// feather quad, triangle 2: inner[j], outer[i], outer[j]
		verts[n].pos[0] = innerX[j]; verts[n].pos[1] = innerY[j]; verts[n].pos[2] = 0.0f;
		verts[n].uv[0] = u[j]; verts[n].uv[1] = v[j];
		memcpy( verts[n].color, color, 4 ); n++;

		verts[n].pos[0] = outerX[i]; verts[n].pos[1] = outerY[i]; verts[n].pos[2] = 0.0f;
		verts[n].uv[0] = u[i]; verts[n].uv[1] = v[i];
		memcpy( verts[n].color, colorFade, 4 ); n++;

		verts[n].pos[0] = outerX[j]; verts[n].pos[1] = outerY[j]; verts[n].pos[2] = 0.0f;
		verts[n].uv[0] = u[j]; verts[n].uv[1] = v[j];
		memcpy( verts[n].color, colorFade, 4 ); n++;
	}

	// force blend so the alpha=0 outer ring actually fades
	vk_draw2d_record_tris( verts, n, shader, qtrue );
}

// Same clip-space transform and vertex layout as vk_draw2d_record_tris, but keyed on a raw image_t
// instead of a shader_t -- r_showImages wants every loaded texture, not a registered shader per one.
void vk_draw2d_raw_image( float x, float y, float w, float h, image_t *image ) {
	static const float uvs[4][2] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };
	static const int order[6] = { 0, 1, 2, 0, 2, 3 };
	float corners[4][2] = { { x, y }, { x + w, y }, { x + w, y + h }, { x, y + h } };
	vk2d_vertex_t verts[6];
	float clip_space_xform[32];
	int width, height, i;
	VkDeviceSize vb_offset;

	if ( !image || image->vk_descriptor == VK_NULL_HANDLE || s_writeIndex + 6 > VK2D_MAX_VERTS ) {
		return;
	}

	for ( i = 0; i < 6; i++ ) {
		int c = order[i];
		verts[i].pos[0] = corners[c][0];
		verts[i].pos[1] = corners[c][1];
		verts[i].pos[2] = 0.0f;
		verts[i].uv[0] = uvs[c][0];
		verts[i].uv[1] = uvs[c][1];
		verts[i].color[0] = verts[i].color[1] = verts[i].color[2] = verts[i].color[3] = 255;
	}

	vb_offset = (VkDeviceSize)( s_regionBase + s_writeIndex ) * sizeof( vk2d_vertex_t );
	memcpy( &s_mapped[s_regionBase + s_writeIndex], verts, sizeof( verts ) );
	s_writeIndex += 6;

	R_GetWinResolution( &width, &height );
	memset( clip_space_xform, 0, sizeof( clip_space_xform ) );
	clip_space_xform[0] = 2.0f / (float)width;
	clip_space_xform[5] = 2.0f / (float)height;
	clip_space_xform[10] = 1.0f;
	clip_space_xform[12] = -1.0f;
	clip_space_xform[13] = -1.0f;
	clip_space_xform[15] = 1.0f;
	qvkCmdPushConstants( vk.command_buffer, vk.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		0, sizeof( clip_space_xform ), clip_space_xform );

	vk_bind_pipeline_cached( vk.pipeline_2d_opaque );
	vk_bind_descriptor_set_cached( vk.pipeline_layout, 0, image->vk_descriptor, 0, NULL );
	qvkCmdBindVertexBuffers( vk.command_buffer, 0, 1, &s_vertexBuffer, &vb_offset );
	qvkCmdDraw( vk.command_buffer, 6, 1, 0, 0 );
}
