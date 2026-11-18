// tr_scene.c -- per-frame scene entity list, see tr_scene.h.

#include "tr_local.h"
#include "tr_scene.h"

static refEntity_t s_entities[MAX_REFENTITIES];
static int s_numEntities;

void R_ClearScene( void ) {
	s_numEntities = 0;
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

const refEntity_t *R_GetSceneEntities( int *outCount ) {
	*outCount = s_numEntities;
	return s_entities;
}
