// tr_fog.c -- RTCW's global "wolf fog" (trigger_fog/target_fog/the sky shader's "fogvars") plus BSP fog brushes, see tr_fog.h.

#include <string.h>

#include "tr_local.h"
#include "tr_bsp.h"
#include "tr_fog.h"

#define FOG_MODE_LINEAR 0
#define FOG_MODE_EXP 1

static glfog_t s_fog[NUM_FOGS];
static int s_frameTime;
static int s_activeBSPFog;

void R_InitFog( void ) {
	memset( s_fog, 0, sizeof( s_fog ) );
	s_frameTime = 0;
	s_activeBSPFog = 0;
}

void R_SetBSPFogForPoint( const vec3_t point ) {
	int i;

	s_activeBSPFog = 0;
	if ( !tr_world.loaded ) {
		return;
	}

	for ( i = 1; i < tr_world.numFogs; i++ ) {
		const bspFog_t *fog = &tr_world.fogs[i];
		if ( !fog->active ) {
			continue;
		}
		if ( point[0] < fog->mins[0] || point[0] > fog->maxs[0] ) continue;
		if ( point[1] < fog->mins[1] || point[1] > fog->maxs[1] ) continue;
		if ( point[2] < fog->mins[2] || point[2] > fog->maxs[2] ) continue;
		s_activeBSPFog = i;
		return;
	}
}

void R_SetFogFrameTime( int time ) {
	s_frameTime = time;
}

void R_SetFog( int fogvar, int var1, int var2, float r, float g, float b, float density ) {
	if ( fogvar < 0 || fogvar >= NUM_FOGS ) {
		return;
	}

	if ( fogvar != FOG_CMD_SWITCHFOG ) {
		if ( var1 == 0 && var2 == 0 ) {
			s_fog[fogvar].registered = qfalse;
			return;
		}

		s_fog[fogvar].color[0] = r;
		s_fog[fogvar].color[1] = g;
		s_fog[fogvar].color[2] = b;
		s_fog[fogvar].color[3] = 1.0f;
		s_fog[fogvar].start = (float)var1;
		s_fog[fogvar].end = (float)var2;
		if ( density >= 1.0f ) {
			s_fog[fogvar].mode = FOG_MODE_LINEAR;
			s_fog[fogvar].density = 1.0f;
		} else {
			s_fog[fogvar].mode = FOG_MODE_EXP;
			s_fog[fogvar].density = density;
		}
		s_fog[fogvar].registered = qtrue;
		return;
	}

	// FOG_CMD_SWITCHFOG: var1 names the fog slot to transition to, var2 is the fade time in ms.
	if ( s_fog[var1].registered != qtrue ) {
		return;
	}

	s_fog[FOG_LAST] = s_fog[FOG_CURRENT].registered ? s_fog[FOG_CURRENT] : s_fog[FOG_MAP];

	s_fog[FOG_TARGET] = s_fog[var1];
	s_fog[FOG_TARGET].startTime = s_frameTime;
	s_fog[FOG_TARGET].finishTime = s_frameTime + var2;
}

// Ports R_SetFrameFog's lerp; a transition between dissimilar fog modes snaps straight to the target instead of fading.
static qboolean ResolveCurrentFog( glfog_t *out ) {
	if ( !s_fog[FOG_TARGET].registered ) {
		return qfalse;
	}

	if ( s_fog[FOG_TARGET].finishTime && s_fog[FOG_TARGET].finishTime >= s_frameTime &&
		 s_fog[FOG_LAST].mode == s_fog[FOG_TARGET].mode ) {
		int fadeTime = s_fog[FOG_TARGET].finishTime - s_fog[FOG_TARGET].startTime;
		float lerpPos;

		if ( fadeTime <= 0 ) {
			fadeTime = 1;
		}
		lerpPos = (float)( s_frameTime - s_fog[FOG_TARGET].startTime ) / (float)fadeTime;
		lerpPos = lerpPos < 0.0f ? 0.0f : ( lerpPos > 1.0f ? 1.0f : lerpPos );

		out->start = s_fog[FOG_LAST].start + ( s_fog[FOG_TARGET].start - s_fog[FOG_LAST].start ) * lerpPos;
		out->end = s_fog[FOG_LAST].end + ( s_fog[FOG_TARGET].end - s_fog[FOG_LAST].end ) * lerpPos;
		out->color[0] = s_fog[FOG_LAST].color[0] + ( s_fog[FOG_TARGET].color[0] - s_fog[FOG_LAST].color[0] ) * lerpPos;
		out->color[1] = s_fog[FOG_LAST].color[1] + ( s_fog[FOG_TARGET].color[1] - s_fog[FOG_LAST].color[1] ) * lerpPos;
		out->color[2] = s_fog[FOG_LAST].color[2] + ( s_fog[FOG_TARGET].color[2] - s_fog[FOG_LAST].color[2] ) * lerpPos;
		out->density = s_fog[FOG_TARGET].density;
		out->mode = s_fog[FOG_TARGET].mode;
	} else {
		*out = s_fog[FOG_TARGET];
	}

	return qtrue;
}

void R_GetFogPushConstants( const vec3_t camPos, float out[12] ) {
	glfog_t current;
	qboolean active;

	out[0] = camPos[0]; out[1] = camPos[1]; out[2] = camPos[2]; out[3] = 0.0f;

	if ( s_activeBSPFog ) {
		const bspFog_t *fog = &tr_world.fogs[s_activeBSPFog];
		out[4] = fog->color[0]; out[5] = fog->color[1]; out[6] = fog->color[2]; out[7] = 0.0f;
		out[8] = 0.0f; out[9] = ( fog->depthForOpaque > 0.0f ) ? fog->depthForOpaque : 512.0f; out[10] = 0.0f; // linear
		out[11] = 1.0f;
		return;
	}

	active = ResolveCurrentFog( &current );

	if ( !active ) {
		out[4] = out[5] = out[6] = out[7] = 0.0f;
		out[8] = out[9] = out[10] = 0.0f;
		out[11] = 0.0f; // disabled
		return;
	}

	out[4] = current.color[0]; out[5] = current.color[1]; out[6] = current.color[2]; out[7] = 0.0f;

	if ( current.mode == FOG_MODE_EXP ) {
		out[8] = current.density; out[9] = 0.0f; out[10] = 1.0f; // exp mode
	} else {
		out[8] = current.start; out[9] = current.end; out[10] = 0.0f; // linear mode
	}
	out[11] = 1.0f; // enabled
}
