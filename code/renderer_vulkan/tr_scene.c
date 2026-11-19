// tr_scene.c -- per-frame scene state, see tr_scene.h.

#include <string.h>

#include "tr_local.h"
#include "tr_scene.h"

#define MAX_SCENE_DLIGHTS 32

static refEntity_t s_entities[MAX_REFENTITIES];
static int s_numEntities;

static dlight_t s_dlights[MAX_SCENE_DLIGHTS];
static int s_numDlights;

static scenePoly_t s_polys[MAX_SCENE_POLYS];
static int s_numPolys;
static polyVert_t s_polyVerts[MAX_SCENE_POLY_VERTS];
static int s_numPolyVerts;

void R_ClearScene( void ) {
	s_numEntities = 0;
	s_numDlights = 0;
	s_numPolys = 0;
	s_numPolyVerts = 0;
}

void R_AddRefEntityToScene( const refEntity_t *ent ) {
	// RF_THIRD_PERSON entities (the player's own body, etc.) are meant for mirrors/portals only, which we don't have.
	if ( ent->renderfx & RF_THIRD_PERSON ) {
		return;
	}
	if ( s_numEntities >= MAX_REFENTITIES ) {
		return; // dropped, same cap the opengl1 renderer enforces
	}
	s_entities[s_numEntities++] = *ent;
}

void R_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b ) {
	dlight_t *dl;

	if ( intensity <= 0.0f || s_numDlights >= MAX_SCENE_DLIGHTS ) {
		return;
	}

	dl = &s_dlights[s_numDlights++];
	VectorCopy( org, dl->origin );
	dl->radius = intensity;
	dl->color[0] = r;
	dl->color[1] = g;
	dl->color[2] = b;
}

// numVerts copies of the same poly, same contract as RE_AddPolyToScene's loop in the reference renderer.
void RE_AddPolysToScene( qhandle_t hShader, int numVerts, const polyVert_t *verts, int numPolys ) {
	int i;
	for ( i = 0; i < numPolys; i++ ) {
		RE_AddPolyToScene( hShader, numVerts, verts + (size_t)i * numVerts );
	}
}

void RE_AddPolyToScene( qhandle_t hShader, int numVerts, const polyVert_t *verts ) {
	scenePoly_t *poly;

	if ( !hShader || numVerts <= 0 ) {
		return;
	}
	if ( s_numPolys >= MAX_SCENE_POLYS || s_numPolyVerts + numVerts > MAX_SCENE_POLY_VERTS ) {
		return; // dropped -- out of room this frame
	}

	poly = &s_polys[s_numPolys++];
	poly->shader = hShader;
	poly->firstVert = s_numPolyVerts;
	poly->numVerts = numVerts;

	memcpy( &s_polyVerts[s_numPolyVerts], verts, (size_t)numVerts * sizeof( *verts ) );
	s_numPolyVerts += numVerts;
}

const refEntity_t *R_GetSceneEntities( int *outCount ) {
	*outCount = s_numEntities;
	return s_entities;
}

const dlight_t *R_GetSceneDlights( int *outCount ) {
	*outCount = s_numDlights;
	return s_dlights;
}

const scenePoly_t *R_GetScenePolys( int *outCount ) {
	*outCount = s_numPolys;
	return s_polys;
}

const polyVert_t *R_GetScenePolyVerts( void ) {
	return s_polyVerts;
}

static qboolean s_skyboxPortalActive = qfalse;

void R_ResetSkyboxPortal( void ) {
	s_skyboxPortalActive = qfalse;
}

void R_NoteSkyboxPortalScene( int rdflags ) {
	if ( ( rdflags & RDF_SKYBOXPORTAL ) && ( rdflags & RDF_DRAWSKYBOX ) ) {
		s_skyboxPortalActive = qtrue;
	}
}

qboolean R_SkyboxPortalActive( void ) {
	return s_skyboxPortalActive;
}
