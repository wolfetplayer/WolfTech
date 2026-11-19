// tr_portal.c -- see tr_portal.h.

#include <string.h>

#include "tr_local.h"
#include "tr_main.h"
#include "tr_shader.h"
#include "tr_scene.h"
#include "vk_world.h"
#include "tr_portal.h"

static cvar_t *r_noportals;

static qboolean s_portalActive = qfalse;
static qboolean s_portalIsMirror = qfalse;
static vec3_t s_portalNormal;
static float s_portalDist;

void R_InitPortals( void ) {
	r_noportals = ri.Cvar_Get( "r_noportals", "0", CVAR_CHEAT );
}

qboolean R_InPortalView( void ) {
	return s_portalActive;
}

qboolean R_InMirrorView( void ) {
	return s_portalActive && s_portalIsMirror;
}

void R_GetActivePortalPlane( vec3_t outNormal, float *outDist ) {
	VectorCopy( s_portalNormal, outNormal );
	*outDist = s_portalDist;
}

// Mirrors code/renderer's R_GetPortalOrientations, trimmed to the world-surface-only case (tr_portal.h).
static qboolean R_FindPortalCamera( const vec3_t planeNormal, float planeDist,
									 vec3_t outCameraOrigin, vec3_t outCameraAxis[3], qboolean *outIsMirror ) {
	vec3_t surfaceAxis[3];
	const refEntity_t *entities;
	int numEntities, i;

	VectorCopy( planeNormal, surfaceAxis[0] );
	PerpendicularVector( surfaceAxis[1], surfaceAxis[0] );
	CrossProduct( surfaceAxis[0], surfaceAxis[1], surfaceAxis[2] );

	entities = R_GetSceneEntities( &numEntities );
	for ( i = 0; i < numEntities; i++ ) {
		const refEntity_t *e = &entities[i];
		float d;

		if ( e->reType != RT_PORTALSURFACE ) {
			continue;
		}

		// Locate the portal entity closest to this plane -- same +-64 unit tolerance as upstream.
		d = DotProduct( e->origin, planeNormal ) - planeDist;
		if ( d > 64.0f || d < -64.0f ) {
			continue;
		}

		if ( e->oldorigin[0] == e->origin[0] && e->oldorigin[1] == e->origin[1] && e->oldorigin[2] == e->origin[2] ) {
			// Simple mirror: camera at the plane's closest point to the origin, normal axis flipped (a proper reflection -- see vk_pipelines.c's mirror cull flip).
			vec3_t surfaceOrigin;
			VectorScale( planeNormal, planeDist, surfaceOrigin );
			VectorCopy( surfaceOrigin, outCameraOrigin );
			VectorSubtract( vec3_origin, surfaceAxis[0], outCameraAxis[0] );
			VectorCopy( surfaceAxis[1], outCameraAxis[1] );
			VectorCopy( surfaceAxis[2], outCameraAxis[2] );
			*outIsMirror = qtrue;
			return qtrue;
		}

		// Remote (periscope-style) portal -- not ported (see tr_portal.h); treated as no match.
	}

	return qfalse;
}

qboolean R_TryRenderPortalView( const refdef_t *fd ) {
	vec3_t planeNormal, planePoint, cameraOrigin, cameraAxis[3];
	float planeDist;
	qboolean isMirror;
	refdef_t portalFd;

	if ( s_portalActive || ( r_noportals && r_noportals->integer ) ) {
		return qfalse;
	}

	if ( !vk_world_findVisiblePortalSurface( planeNormal, &planeDist, planePoint ) ) {
		return qfalse;
	}

	if ( !R_FindPortalCamera( planeNormal, planeDist, cameraOrigin, cameraAxis, &isMirror ) ) {
		return qfalse; // no matching RT_PORTALSURFACE entity yet -- not an error
	}

	portalFd = *fd;
	VectorCopy( cameraOrigin, portalFd.vieworg );
	VectorCopy( cameraAxis[0], portalFd.viewaxis[0] );
	VectorCopy( cameraAxis[1], portalFd.viewaxis[1] );
	VectorCopy( cameraAxis[2], portalFd.viewaxis[2] );

	s_portalActive = qtrue;
	s_portalIsMirror = isMirror;
	// R_SetupPortalPlane (vkQuake3, GPLv2-or-later): clip plane from the destination camera's own axis/origin.
	VectorSubtract( vec3_origin, cameraAxis[0], s_portalNormal );
	s_portalDist = -DotProduct( cameraOrigin, cameraAxis[0] );

	R_RenderViewInternal( &portalFd );

	s_portalActive = qfalse;
	s_portalIsMirror = qfalse;
	return qtrue;
}
