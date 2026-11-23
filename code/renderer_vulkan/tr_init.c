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
#include "vk_image.h"
#include "tr_shader.h"
#include "tr_main.h"
#include "tr_bsp.h"
#include "tr_world.h"
#include "tr_patch.h"
#include "tr_model.h"
#include "tr_bones.h"
#include "tr_skin.h"
#include "tr_light.h"
#include "tr_fog.h"
#include "tr_shade_calc.h"
#include "tr_scene.h"
#include "vk_draw2d.h"
#include "vk_draw3d.h"
#include "vk_entity.h"
#include "vk_sky.h"
#include "vk_poly.h"
#include "vk_world.h"
#include "vk_flares.h"
#include "vk_shadows.h"
#include "vk_screenshot.h"
#include "tr_portal.h"
#include "tr_marks.h"

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

static cvar_t *r_showImages;

// Replaces the normal scene with a grid of every loaded texture, for spotting texture-load/thrashing problems.
static void R_DrawShowImages( void ) {
	int width, height, numImages, i;
	float cellW, cellH;

	R_GetWinResolution( &width, &height );
	cellW = (float)width / 40.0f;
	cellH = (float)height / 30.0f;

	numImages = R_GetNumImages();
	for ( i = 0; i < numImages; i++ ) {
		image_t *image = R_GetImageByIndex( i );
		float x = (float)( i % 40 ) * cellW;
		float y = (float)( i / 40 ) * cellH;

		if ( y + cellH > (float)height ) {
			break; // grid is full; the rest just wouldn't be visible
		}
		vk_draw2d_raw_image( x, y, cellW, cellH, image );
	}
}

/*
===============
RE_Shutdown
===============
*/
static void RE_Shutdown( qboolean destroyWindow ) {
	qboolean wasInitialized = vk.isInitialized;

	ri.Printf( PRINT_ALL, "RE_Shutdown( %i )\n", destroyWindow );

	if ( wasInitialized ) {
		vk_screenshot_shutdown();
		vk_flush_pending_uploads(); // don't leave a batch dangling if shutdown lands mid-registration

		// must tear down before vk_shutdown() frees the descriptor pool
		R_DoneFreeType();
		R_ShutdownShaders();
		R_ShutdownImages();

		vk_shutdown();

		tr_world.loaded = qfalse;

		if ( destroyWindow ) {
			vk_destroyWindow();
		}
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
		R_InitPortals();
		R_InitPatches();
		R_InitModels();
		R_InitSkins();
		R_InitFog();
		vk_screenshot_init();
		r_showImages = ri.Cvar_Get( "r_showImages", "0", CVAR_TEMP );
		s_testCubeInit = qfalse;
	}

	R_GetGlConfig( config );

	ri.Printf( PRINT_ALL, "RE_BeginRegistration\n" );
}

// Real .mdx/.mdm loading and model registry live in tr_model.c.

// Real .skin parsing lives in tr_skin.c.

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
	vk_flush_pending_uploads();
	ri.Printf( PRINT_ALL, "...tr_init checkpoint: RE_EndRegistration\n" );
}

static void RE_ClearScene( void ) {
	R_ClearScene();
}

static void RE_AddRefEntityToScene( const refEntity_t *re ) {
	R_AddRefEntityToScene( re );
}

// Real light grid sampling lives in tr_light.c.

// Real poly-scene storage (RE_AddPolyToScene/RE_AddPolysToScene) lives in tr_scene.c; drawing lives in vk_poly.c.

static void RE_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b, int overdraw ) {
	(void)overdraw; // REF_FORCE_DLIGHT/REF_JUNIOR_DLIGHT/REF_MUZZLEFLASH_DLIGHT tiers aren't implemented -- every light always applies
	R_AddLightToScene( org, intensity, r, g, b );
}

static void RE_AddCoronaToScene( const vec3_t org, float r, float g, float b, float scale, int id, int flags ) {
	R_AddCoronaToScene( org, r, g, b, scale, id, flags );
}

// The guts of RE_RenderScene -- see tr_main.h; tr_portal.c calls this recursively for a mirror/portal's destination view.
void R_RenderViewInternal( const refdef_t *fd ) {
	float viewMatrix[16], projMatrix[16], viewProj[16], fogPush[12];
	qboolean isSkyboxPortalScene = ( fd->rdflags & RDF_SKYBOXPORTAL ) != 0;

	R_SetFogFrameTime( fd->time );
	R_SetShaderTime( fd->time );
	R_SetBSPFogForPoint( fd->vieworg );
	R_GetFogPushConstants( fd->vieworg, fogPush );

	R_SetupViewMatrix( fd, viewMatrix );
	R_SetupProjectionMatrix( fd, projMatrix );
	R_MultMatrix( projMatrix, viewMatrix, viewProj );

	if ( !( fd->rdflags & RDF_NOWORLDMODEL ) ) {
		// Some cgame builds call the portal scene after the real scene, not before -- track actual per-frame arrival order instead of assuming it (see tr_scene.h).
		if ( isSkyboxPortalScene && R_MainSceneRenderedThisFrame() ) {
			return; // real scene already drew its own sky below -- this would only stamp the wrong camera over it
		}

		R_MarkVisibleSurfaces( fd );
		if ( R_TryRenderPortalView( fd ) ) {
			R_MarkVisibleSurfaces( fd ); // the recursive call above overwrote tr_world.c's shared visibility buffer
		}

		if ( isSkyboxPortalScene ) {
			vk_sky_draw( fd->vieworg, viewProj, fogPush, R_ComputeZFar( fd ) );
			R_NotePortalSceneRendered();
		} else if ( R_PortalSceneRenderedThisFrame() ) {
			vk_clear_depth_only(); // reset depth for the real scene, but leave color so the sky room's background survives
		} else {
			vk_sky_draw( fd->vieworg, viewProj, fogPush, R_ComputeZFar( fd ) ); // portal hasn't rendered yet this frame (or this map has none)
		}
		vk_world_draw( viewProj, fogPush );

		if ( !isSkyboxPortalScene ) {
			R_NoteMainSceneRendered();
		}
	}

	vk_draw3d_test_cube( s_testCubeOrigin, 40.0f, viewProj, fogPush );

	{
		int numEntities;
		const refEntity_t *entities = R_GetSceneEntities( &numEntities );
		vk_entity_draw_all( entities, numEntities, viewProj, fogPush );
		vk_draw3d_entity_markers( entities, numEntities, 16.0f, viewProj, fogPush );
	}

	// One-shot per real frame: only draws if some entity above actually stamped the stencil buffer.
	vk_shadows_draw_darken( viewProj, fogPush );

	vk_poly_draw( viewProj, fogPush );

	// Coronas are real-world effects (torches, treasure glow, fuses) -- skip the sky room and any mirror/portal recursion.
	if ( !( fd->rdflags & RDF_NOWORLDMODEL ) && !isSkyboxPortalScene && !R_InPortalView() ) {
		vk_flares_draw( fd, viewProj, fogPush );
	}
}

// Sanity test cube: fixed in world space on first run, so walking around it proves the camera matrices track correctly.
static void RE_RenderScene( const refdef_t *fd ) {
	if ( !vk.isInitialized || !s_frameActive ) {
		return;
	}

	FIRST_CALL_CHECKPOINT( "RE_RenderScene (any)" );

	if ( !s_testCubeInit ) {
		VectorMA( fd->vieworg, 200.0f, fd->viewaxis[0], s_testCubeOrigin );
		s_testCubeInit = qtrue;
	}

	if ( r_showImages->integer ) {
		R_DrawShowImages();
		return;
	}

	if ( !( fd->rdflags & RDF_NOWORLDMODEL ) && !s_firstRealSceneLogged ) {
		ri.Printf( PRINT_ALL, "...tr_init checkpoint: first real-world RE_RenderScene call\n" );
		s_firstRealSceneLogged = qtrue;
	}

	R_RenderViewInternal( fd );

	R_AdvanceSceneWindow(); // see tr_scene.h -- windows off this call's entities/dlights/polys for the next one
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
	if ( !vk.isInitialized || !s_frameActive ) {
		return;
	}
	vk_draw2d_quad_rotated( x, y, w, h, s1, t1, s2, t2, R_GetShaderByHandle( hShader ), angle );
}

static void RE_StretchPicGradient( float x, float y, float w, float h, float s1, float t1, float s2, float t2,
									qhandle_t hShader, const float *gradientColor, int gradientType ) {
	(void)gradientType; // the reference's own backend ignores this too -- always fades top-to-bottom, see vk_draw2d_quad_gradient
	if ( !vk.isInitialized || !s_frameActive ) {
		return;
	}
	vk_draw2d_quad_gradient( x, y, w, h, s1, t1, s2, t2, R_GetShaderByHandle( hShader ), gradientColor );
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

	vk_flush_pending_uploads(); // any texture registered since the last frame must be uploaded before this one samples it

	s_frameActive = vk_begin_frame();
	if ( !s_frameActive ) {
		return; // swapchain was just recreated, nothing to record this frame
	}

	vk_flares_begin_frame(); // resets this frame's occlusion queries -- vkCmdResetQueryPool needs to run outside the render pass

	vk_begin_render_pass( 0.0f, 0.0f, 0.0f, 1.0f );
	vk_draw2d_begin_frame();

	// reset once per real frame, not per scene -- see e.g. vk_entity_begin_frame's comment
	vk_world_begin_frame();
	vk_entity_begin_frame();
	vk_poly_begin_frame();
	vk_shadows_begin_frame();
	R_BeginFrameSkyboxOrder();
}

static void RE_EndFrame( int *frontEndMsec, int *backEndMsec ) {
	if ( vk.isInitialized && s_frameActive ) {
		vk_end_render_pass();
		vk_end_frame();
		s_frameActive = qfalse;
		vk_screenshot_process_pending();
	}

	if ( frontEndMsec ) {
		*frontEndMsec = 0;
	}
	if ( backEndMsec ) {
		*backEndMsec = 0;
	}
}

// Real tag lookup (R_LerpTag) lives in tr_bones.c; real decal clipping (R_MarkFragments) lives in tr_marks.c.

// Frame-0 local-space bounds, matching code/renderer and code/rend2's R_ModelBounds semantics.
static void R_ModelBounds( qhandle_t hModel, vec3_t mins, vec3_t maxs ) {
	model_t *mod = R_GetModelByHandle( hModel );

	if ( mod ) {
		if ( mod->type == MOD_BRUSH ) {
			if ( mod->bmodelIndex >= 0 && mod->bmodelIndex < tr_world.numSubmodels ) {
				VectorCopy( tr_world.submodels[mod->bmodelIndex].mins, mins );
				VectorCopy( tr_world.submodels[mod->bmodelIndex].maxs, maxs );
			} else {
				VectorClear( mins );
				VectorClear( maxs );
			}
			return;
		}
		if ( mod->md3 ) {
			md3Frame_t *frame = (md3Frame_t *)( (byte *)mod->md3 + mod->md3->ofsFrames );
			VectorCopy( frame->bounds[0], mins );
			VectorCopy( frame->bounds[1], maxs );
			return;
		}
		if ( mod->mdc ) {
			md3Frame_t *frame = (md3Frame_t *)( (byte *)mod->mdc + mod->mdc->ofsFrames );
			VectorCopy( frame->bounds[0], mins );
			VectorCopy( frame->bounds[1], maxs );
			return;
		}
		if ( mod->mds ) {
			mdsFrame_t *frame = (mdsFrame_t *)( (byte *)mod->mds + mod->mds->ofsFrames );
			VectorCopy( frame->bounds[0], mins );
			VectorCopy( frame->bounds[1], maxs );
			return;
		}
		if ( mod->mdx ) {
			mdxFrame_t *frame = (mdxFrame_t *)( (byte *)mod->mdx + mod->mdx->ofsFrames );
			VectorCopy( frame->bounds[0], mins );
			VectorCopy( frame->bounds[1], maxs );
			return;
		}
		// mdm carries no frame data of its own -- bounds live on its separate MDX skeleton, not reachable from a bare handle
	}

	VectorClear( mins );
	VectorClear( maxs );
}

// Real R_RemapShader lives in tr_shader.c.

static void RB_ZombieFXAddNewHit( int entityNum, const vec3_t hitPos, const vec3_t hitDir ) {
	FIRST_CALL_CHECKPOINT( "RB_ZombieFXAddNewHit" );
	(void)entityNum; (void)hitPos; (void)hitDir;
}

// Hands cgame the BSP's raw entity-lump text one COM_Parse token at a time; wraps back to the start at EOF.
static qboolean R_GetEntityToken( char *buffer, int size ) {
	const char *s = COM_Parse( &tr_world.entityParsePoint );

	Q_strncpyz( buffer, s, size );
	if ( !tr_world.entityParsePoint && !s[0] ) {
		tr_world.entityParsePoint = tr_world.entityString;
		return qfalse;
	}
	return qtrue;
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
