/*
===========================================================================

Return to Castle Wolfenstein single player GPL Source Code
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company.

This file is part of the Return to Castle Wolfenstein single player GPL Source Code (RTCW SP Source Code).

RTCW SP Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

RTCW SP Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RTCW SP Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the RTCW SP Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the RTCW SP Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

// tr_init.c -- refexport_t entry points; 3D scene calls are still stubs, 2D drawing/registration are real.

#include "tr_local.h"
#include "glConfig.h"
#include "vk_instance.h"
#include "vk_init.h"
#include "vk_frame.h"
#include "tr_image.h"
#include "tr_shader.h"
#include "tr_main.h"
#include "tr_bsp.h"
#include "tr_world.h"
#include "vk_draw2d.h"
#include "vk_draw3d.h"
#include "vk_world.h"

refimport_t ri;

// Implemented in tr_font.c.
void RE_RegisterFont( const char *fontName, int pointSize, fontInfo_t *font );
void RE_RegisterFontExtended( const char *fontName, int pointSize, fontInfoExtra_t *font );
void R_InitFreeType( void );
void R_DoneFreeType( void );

// True while this frame's command buffer is open for recording.
static qboolean s_frameActive = qfalse;

// Set by RE_RenderScene the first time it runs after a fresh vk_initialize(); see RE_RenderScene.
static qboolean s_testCubeInit = qfalse;
static vec3_t s_testCubeOrigin;
static qboolean s_firstRealSceneLogged = qfalse;

/*
===============
RE_Shutdown
===============
*/
static void RE_Shutdown( qboolean destroyWindow ) {
	qboolean wasInitialized = vk.isInitialized;

	ri.Printf( PRINT_ALL, "RE_Shutdown( %i )\n", destroyWindow );

	if ( wasInitialized ) {
		// must tear down before vk_shutdown() frees the descriptor pool
		R_DoneFreeType();
		R_ShutdownShaders();
		R_ShutdownImages();

		vk_shutdown();

		// no fast vid_restart path yet -- always recreate the window too
		vk_destroyWindow();
	}

	R_glConfigClear();
}

/*
===============
RE_BeginRegistration

First call brings up the device/window; later calls just hand back glconfig_t.
===============
*/
static void RE_BeginRegistration( glconfig_t *config ) {
	if ( !vk.isInitialized ) {
		R_glConfigInit();
		vk_initialize();
		R_InitImages();
		R_InitShaders();
		R_InitFreeType();
		R_InitMain();
		s_testCubeInit = qfalse;
	}

	R_GetGlConfig( config );

	ri.Printf( PRINT_ALL, "RE_BeginRegistration\n" );
}

// No real model/skin loading yet -- unique nonzero handles so callers can't alias them all to one.
static qhandle_t RE_RegisterModel( const char *name ) {
	static qboolean logged = qfalse;
	static qhandle_t nextHandle = 1;
	if ( !logged ) {
		ri.Printf( PRINT_ALL, "...tr_init checkpoint: first RE_RegisterModel ('%s')\n", name ? name : "(null)" );
		logged = qtrue;
	}
	(void)name;
	return nextHandle++;
}

static qhandle_t RE_RegisterSkin( const char *name ) {
	static qboolean logged = qfalse;
	static qhandle_t nextHandle = 1;
	if ( !logged ) {
		ri.Printf( PRINT_ALL, "...tr_init checkpoint: first RE_RegisterSkin ('%s')\n", name ? name : "(null)" );
		logged = qtrue;
	}
	(void)name;
	return nextHandle++;
}

static qhandle_t RE_RegisterShader( const char *name ) {
	static int callCount = 0;
	callCount++;
	if ( callCount % 200 == 0 ) {
		ri.Printf( PRINT_ALL, "...tr_init checkpoint: RE_RegisterShader call #%d ('%s')\n", callCount, name ? name : "(null)" );
	}
	return RE_RegisterShaderInternal( name, qfalse );
}

static qhandle_t RE_RegisterShaderNoMip( const char *name ) {
	static int callCount = 0;
	callCount++;
	if ( callCount % 200 == 0 ) {
		ri.Printf( PRINT_ALL, "...tr_init checkpoint: RE_RegisterShaderNoMip call #%d ('%s')\n", callCount, name ? name : "(null)" );
	}
	return RE_RegisterShaderInternal( name, qtrue );
}

static qboolean RE_GetSkinModel( qhandle_t skinid, const char *type, char *name ) {
	(void)skinid; (void)type;
	if ( name ) {
		name[0] = '\0';
	}
	return qfalse;
}

static qhandle_t RE_GetShaderFromModel( qhandle_t modelid, int surfnum, int withlightmap ) {
	(void)modelid; (void)surfnum; (void)withlightmap;
	return 0;
}

// Crash-hunt instrumentation -- prints the first time each still-stub entry point is hit.
#define FIRST_CALL_CHECKPOINT( label ) \
	do { \
		static qboolean logged = qfalse; \
		if ( !logged ) { ri.Printf( PRINT_ALL, "...tr_init checkpoint: first " label "\n" ); logged = qtrue; } \
	} while ( 0 )

static void RE_SetWorldVisData( const byte *vis ) {
	FIRST_CALL_CHECKPOINT( "RE_SetWorldVisData" );
	(void)vis;
}

static void RE_EndRegistration( void ) {
	ri.Printf( PRINT_ALL, "...tr_init checkpoint: RE_EndRegistration\n" );
}

static void RE_ClearScene( void ) {
	FIRST_CALL_CHECKPOINT( "RE_ClearScene" );
}

static void RE_AddRefEntityToScene( const refEntity_t *re ) {
	FIRST_CALL_CHECKPOINT( "RE_AddRefEntityToScene" );
	(void)re;
}

static int R_LightForPoint( vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir ) {
	FIRST_CALL_CHECKPOINT( "R_LightForPoint" );
	(void)point;
	VectorClear( ambientLight );
	VectorClear( directedLight );
	VectorSet( lightDir, 0.0f, 0.0f, 1.0f );
	return 0;
}

static void RE_AddPolyToScene( qhandle_t hShader, int numVerts, const polyVert_t *verts ) {
	FIRST_CALL_CHECKPOINT( "RE_AddPolyToScene" );
	(void)hShader; (void)numVerts; (void)verts;
}

static void RE_AddPolysToScene( qhandle_t hShader, int numVerts, const polyVert_t *verts, int numPolys ) {
	FIRST_CALL_CHECKPOINT( "RE_AddPolysToScene" );
	(void)hShader; (void)numVerts; (void)verts; (void)numPolys;
}

static void RE_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b, int overdraw ) {
	FIRST_CALL_CHECKPOINT( "RE_AddLightToScene" );
	(void)org; (void)intensity; (void)r; (void)g; (void)b; (void)overdraw;
}

static void RE_AddCoronaToScene( const vec3_t org, float r, float g, float b, float scale, int id, int flags ) {
	FIRST_CALL_CHECKPOINT( "RE_AddCoronaToScene" );
	(void)org; (void)r; (void)g; (void)b; (void)scale; (void)id; (void)flags;
}

static void R_SetFog( int fogvar, int var1, int var2, float r, float g, float b, float density ) {
	FIRST_CALL_CHECKPOINT( "R_SetFog" );
	(void)fogvar; (void)var1; (void)var2; (void)r; (void)g; (void)b; (void)density;
}

// Sanity test cube: fixed in world space on first run, so walking around it proves the camera matrices track correctly.
static void RE_RenderScene( const refdef_t *fd ) {
	float viewMatrix[16], projMatrix[16], viewProj[16];

	if ( !vk.isInitialized || !s_frameActive ) {
		return;
	}

	FIRST_CALL_CHECKPOINT( "RE_RenderScene (any)" );

	if ( !s_testCubeInit ) {
		VectorMA( fd->vieworg, 200.0f, fd->viewaxis[0], s_testCubeOrigin );
		s_testCubeInit = qtrue;
	}

	if ( !( fd->rdflags & RDF_NOWORLDMODEL ) && !s_firstRealSceneLogged ) {
		ri.Printf( PRINT_ALL, "...tr_init checkpoint: first real-world RE_RenderScene call\n" );
		s_firstRealSceneLogged = qtrue;
	}

	R_SetupViewMatrix( fd, viewMatrix );
	R_SetupProjectionMatrix( fd, projMatrix );
	R_MultMatrix( projMatrix, viewMatrix, viewProj );

	if ( !( fd->rdflags & RDF_NOWORLDMODEL ) ) {
		R_MarkVisibleSurfaces( fd );
		vk_world_draw( viewProj );
	}

	vk_draw3d_test_cube( s_testCubeOrigin, 40.0f, viewProj );
}

static void RE_SetColor( const float *rgba ) {
	vk_draw2d_set_color( rgba );
}

static void RE_StretchPic( float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader ) {
	if ( !vk.isInitialized || !s_frameActive ) {
		return;
	}
	vk_draw2d_quad( x, y, w, h, s1, t1, s2, t2, R_GetShaderByHandle( hShader ) );
}

static void RE_RotatedPic( float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader, float angle ) {
	FIRST_CALL_CHECKPOINT( "RE_RotatedPic" );
	(void)x; (void)y; (void)w; (void)h; (void)s1; (void)t1; (void)s2; (void)t2; (void)hShader; (void)angle;
}

static void RE_StretchPicGradient( float x, float y, float w, float h, float s1, float t1, float s2, float t2,
									qhandle_t hShader, const float *gradientColor, int gradientType ) {
	FIRST_CALL_CHECKPOINT( "RE_StretchPicGradient" );
	(void)x; (void)y; (void)w; (void)h; (void)s1; (void)t1; (void)s2; (void)t2;
	(void)hShader; (void)gradientColor; (void)gradientType;
}

static void RE_StretchPicRounded( float x, float y, float w, float h, float s1, float t1, float s2, float t2,
								   qhandle_t hShader, float radius ) {
	if ( !vk.isInitialized || !s_frameActive ) {
		return;
	}
	vk_draw2d_quad_rounded( x, y, w, h, s1, t1, s2, t2, R_GetShaderByHandle( hShader ), radius );
}

static void RE_StretchPicSkewed( float x, float y, float w, float h, float s1, float t1, float s2, float t2,
								  qhandle_t hShader, float skewLeft, float skewRight ) {
	if ( !vk.isInitialized || !s_frameActive ) {
		return;
	}
	vk_draw2d_quad_skewed( x, y, w, h, s1, t1, s2, t2, R_GetShaderByHandle( hShader ), skewLeft, skewRight );
}

static void RE_StretchRaw( int x, int y, int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty ) {
	FIRST_CALL_CHECKPOINT( "RE_StretchRaw" );
	(void)x; (void)y; (void)w; (void)h; (void)cols; (void)rows; (void)data; (void)client; (void)dirty;
}

static void RE_UploadCinematic( int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty ) {
	FIRST_CALL_CHECKPOINT( "RE_UploadCinematic" );
	(void)w; (void)h; (void)cols; (void)rows; (void)data; (void)client; (void)dirty;
}

static void RE_BeginFrame( stereoFrame_t stereoFrame ) {
	(void)stereoFrame;

	if ( !vk.isInitialized ) {
		return;
	}

	s_frameActive = vk_begin_frame();
	if ( !s_frameActive ) {
		return; // swapchain was just recreated, nothing to record this frame
	}

	vk_begin_render_pass( 0.0f, 0.2f, 0.4f, 1.0f );
	vk_draw2d_begin_frame();
}

static void RE_EndFrame( int *frontEndMsec, int *backEndMsec ) {
	if ( vk.isInitialized && s_frameActive ) {
		vk_end_render_pass();
		vk_end_frame();
		s_frameActive = qfalse;
	}

	if ( frontEndMsec ) {
		*frontEndMsec = 0;
	}
	if ( backEndMsec ) {
		*backEndMsec = 0;
	}
}

static int R_MarkFragments( int numPoints, const vec3_t *points, const vec3_t projection,
							 int maxPoints, vec3_t pointBuffer, int maxFragments, markFragment_t *fragmentBuffer ) {
	FIRST_CALL_CHECKPOINT( "R_MarkFragments" );
	(void)numPoints; (void)points; (void)projection; (void)maxPoints; (void)pointBuffer; (void)maxFragments; (void)fragmentBuffer;
	return 0;
}

static int R_LerpTag( orientation_t *tag, const refEntity_t *refent, const char *tagName, int startIndex ) {
	FIRST_CALL_CHECKPOINT( "R_LerpTag" );
	(void)refent; (void)tagName; (void)startIndex;
	AxisClear( tag->axis );
	VectorClear( tag->origin );
	return -1;
}

static void R_ModelBounds( qhandle_t model, vec3_t mins, vec3_t maxs ) {
	FIRST_CALL_CHECKPOINT( "R_ModelBounds" );
	(void)model;
	VectorClear( mins );
	VectorClear( maxs );
}

static void R_RemapShader( const char *oldShader, const char *newShader, const char *offsetTime ) {
	FIRST_CALL_CHECKPOINT( "R_RemapShader" );
	(void)oldShader; (void)newShader; (void)offsetTime;
}

static void RB_ZombieFXAddNewHit( int entityNum, const vec3_t hitPos, const vec3_t hitDir ) {
	FIRST_CALL_CHECKPOINT( "RB_ZombieFXAddNewHit" );
	(void)entityNum; (void)hitPos; (void)hitDir;
}

static qboolean R_GetEntityToken( char *buffer, int size ) {
	FIRST_CALL_CHECKPOINT( "R_GetEntityToken" );
	if ( buffer && size > 0 ) {
		buffer[0] = '\0';
	}
	return qfalse;
}

static void RE_RenderToTexture( int textureid, int x, int y, int w, int h ) {
	FIRST_CALL_CHECKPOINT( "RE_RenderToTexture" );
	(void)textureid; (void)x; (void)y; (void)w; (void)h;
}

static int R_GetTextureId( const char *imagename ) {
	FIRST_CALL_CHECKPOINT( "R_GetTextureId" );
	(void)imagename;
	return -1;
}

static void RE_TakeVideoFrame( int h, int w, byte *captureBuffer, byte *encodeBuffer, qboolean motionJpeg ) {
	FIRST_CALL_CHECKPOINT( "RE_TakeVideoFrame" );
	(void)h; (void)w; (void)captureBuffer; (void)encodeBuffer; (void)motionJpeg;
}

/*
@@@@@@@@@@@@@@@@@@@@@
GetRefAPI
@@@@@@@@@@@@@@@@@@@@@
*/
#ifdef USE_RENDERER_DLOPEN
Q_EXPORT refexport_t *QDECL GetRefAPI( int apiVersion, refimport_t *rimp ) {
#else
refexport_t *GetRefAPI( int apiVersion, refimport_t *rimp ) {
#endif
	static refexport_t re;

	ri = *rimp;

	memset( &re, 0, sizeof( re ) );

	if ( apiVersion != REF_API_VERSION ) {
		ri.Printf( PRINT_ALL, "Mismatched REF_API_VERSION: expected %i, got %i\n",
				   REF_API_VERSION, apiVersion );
		return NULL;
	}

	re.Shutdown = RE_Shutdown;

	re.BeginRegistration = RE_BeginRegistration;
	re.RegisterModel = RE_RegisterModel;
	re.RegisterSkin = RE_RegisterSkin;
	re.GetSkinModel = RE_GetSkinModel;
	re.GetShaderFromModel = RE_GetShaderFromModel;
	re.RegisterShader = RE_RegisterShader;
	re.RegisterShaderNoMip = RE_RegisterShaderNoMip;
	re.LoadWorld = RE_LoadWorldMap;
	re.SetWorldVisData = RE_SetWorldVisData;
	re.EndRegistration = RE_EndRegistration;

	re.ClearScene = RE_ClearScene;
	re.AddRefEntityToScene = RE_AddRefEntityToScene;
	re.LightForPoint = R_LightForPoint;
	re.AddPolyToScene = RE_AddPolyToScene;
	re.AddPolysToScene = RE_AddPolysToScene;
	re.AddLightToScene = RE_AddLightToScene;
	re.AddCoronaToScene = RE_AddCoronaToScene;
	re.SetFog = R_SetFog;
	re.RenderScene = RE_RenderScene;

	re.SetColor = RE_SetColor;
	re.DrawStretchPic = RE_StretchPic;
	re.DrawRotatedPic = RE_RotatedPic;
	re.DrawStretchPicGradient = RE_StretchPicGradient;
	re.DrawStretchPicRounded = RE_StretchPicRounded;
	re.DrawStretchPicSkewed = RE_StretchPicSkewed;
	re.DrawStretchRaw = RE_StretchRaw;
	re.UploadCinematic = RE_UploadCinematic;

	re.BeginFrame = RE_BeginFrame;
	re.EndFrame = RE_EndFrame;

	re.MarkFragments = R_MarkFragments;
	re.LerpTag = R_LerpTag;
	re.ModelBounds = R_ModelBounds;

	re.RegisterFont = RE_RegisterFont;
	re.RegisterFontExtended = RE_RegisterFontExtended;
	re.RemapShader = R_RemapShader;

	re.ZombieFXAddNewHit = RB_ZombieFXAddNewHit;

	re.GetEntityToken = R_GetEntityToken;

	re.RenderToTexture = RE_RenderToTexture;
	re.GetTextureId = R_GetTextureId;

	re.TakeVideoFrame = RE_TakeVideoFrame;

	ri.Printf( PRINT_ALL, "----- Vulkan renderer module loaded -----\n" );

	return &re;
}
