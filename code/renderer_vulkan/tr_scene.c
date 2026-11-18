// tr_scene.c -- per-frame scene state, see tr_scene.h.

#include "tr_local.h"
#include "tr_scene.h"

#define MAX_SCENE_DLIGHTS 32

static refEntity_t s_entities[MAX_REFENTITIES];
static int s_numEntities;

static dlight_t s_dlights[MAX_SCENE_DLIGHTS];
static int s_numDlights;

void R_ClearScene( void ) {
	s_numEntities = 0;
	s_numDlights = 0;
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

const refEntity_t *R_GetSceneEntities( int *outCount ) {
	*outCount = s_numEntities;
	return s_entities;
}

const dlight_t *R_GetSceneDlights( int *outCount ) {
	*outCount = s_numDlights;
	return s_dlights;
}
