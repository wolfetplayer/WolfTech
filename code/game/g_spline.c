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

/*
	g_spline.c
	desc: curved mover paths ("info_train_spline_main" + "followspline"), evaluated server-side only
*/

#include "g_local.h"

int numSplinePaths = 0;
splinePath_t splinePaths[MAX_SPLINE_PATHS];

static qboolean splinesBuilt = qfalse;

/*
===============
G_AddSplinePath

  Registers a new spline leg's starting node. Called from
  SP_info_train_spline_main at spawn time. "target" is stored as a name
  only -- it's resolved to another splinePath_t by G_BuildSplinePaths,
  once every info_train_spline_main node has had a chance to register.
===============
*/
splinePath_t *G_AddSplinePath( const char *name, const char *target, vec3_t origin ) {
	splinePath_t *spline;

	if ( numSplinePaths >= MAX_SPLINE_PATHS ) {
		G_Error( "G_AddSplinePath: MAX_SPLINE_PATHS (%i) hit\n", MAX_SPLINE_PATHS );
	}

	spline = &splinePaths[numSplinePaths++];
	memset( spline, 0, sizeof( *spline ) );

	Q_strncpyz( spline->name, name, sizeof( spline->name ) );
	VectorCopy( origin, spline->origin );
	if ( target ) {
		Q_strncpyz( spline->targetName, target, sizeof( spline->targetName ) );
	}

	return spline;
}

/*
===============
G_AddSplineControl

  Adds a curve control point to a leg, by path_corner name. Resolved to an
  origin later, in G_BuildSplinePaths.
===============
*/
void G_AddSplineControl( splinePath_t *spline, const char *name ) {
	if ( spline->numControls >= MAX_SPLINE_CONTROLS ) {
		G_Error( "G_AddSplineControl: MAX_SPLINE_CONTROLS (%i) hit on spline \"%s\"\n", MAX_SPLINE_CONTROLS, spline->name );
	}

	Q_strncpyz( spline->controlNames[spline->numControls], name, MAX_QPATH );
	spline->numControls++;
}

/*
===============
G_FindSplinePathByName

  Internal lookup by name, with no side effects. Used while resolving
  names during the build pass itself.
===============
*/
static splinePath_t *G_FindSplinePathByName( const char *name ) {
	int i;

	for ( i = 0; i < numSplinePaths; i++ ) {
		if ( !Q_stricmp( splinePaths[i].name, name ) ) {
			return &splinePaths[i];
		}
	}

	return NULL;
}

/*
===============
G_CalculateSpline_r

  De Casteljau reduction of this leg's through-points at parameter "tension"
  (0..1). Ported from ETLegacy's BG_CalculateSpline_r.
===============
*/
static void G_CalculateSpline_r( splinePath_t *spline, vec3_t out1, vec3_t out2, float tension ) {
	vec3_t points[MAX_SPLINE_CONTROLS + 2];
	int count = spline->numControls + 2;
	int i;
	vec3_t dist;

	if ( !spline->next ) {
		VectorCopy( spline->origin, out1 );
		VectorCopy( spline->origin, out2 );
		return;
	}

	VectorCopy( spline->origin, points[0] );
	for ( i = 0; i < spline->numControls; i++ ) {
		VectorCopy( spline->controls[i], points[i + 1] );
	}
	VectorCopy( spline->next->origin, points[i + 1] );

	while ( count > 2 ) {
		for ( i = 0; i < count - 1; i++ ) {
			VectorSubtract( points[i + 1], points[i], dist );
			VectorMA( points[i], tension, dist, points[i] );
		}
		count--;
	}

	VectorCopy( points[0], out1 );
	VectorCopy( points[1], out2 );
}

/*
===============
G_ComputeSplineSegments

  Precomputes a MAX_SPLINE_SEGMENTS-slice chord table for this leg, sampled
  at equal parameter steps (matches ETLegacy's own approximation).
===============
*/
static void G_ComputeSplineSegments( splinePath_t *spline ) {
	int i;
	float granularity = 1.0f / (float)MAX_SPLINE_SEGMENTS;
	vec3_t vec[4];
	vec3_t next;

	spline->length = 0;

	for ( i = 0; i < MAX_SPLINE_SEGMENTS; i++ ) {
		G_CalculateSpline_r( spline, vec[0], vec[1], i * granularity );
		VectorSubtract( vec[1], vec[0], spline->segments[i].start );
		VectorMA( vec[0], i * granularity, spline->segments[i].start, spline->segments[i].start );

		G_CalculateSpline_r( spline, vec[2], vec[3], ( i + 1 ) * granularity );
		VectorSubtract( vec[3], vec[2], next );
		VectorMA( vec[2], ( i + 1 ) * granularity, next, next );

		VectorSubtract( next, spline->segments[i].start, spline->segments[i].v_norm );
		spline->segments[i].length = VectorLength( spline->segments[i].v_norm );
		VectorNormalize( spline->segments[i].v_norm );

		spline->length += spline->segments[i].length;
	}
}

/*
===============
G_BuildSplinePaths

  Resolves every leg's "target" and control points, then builds segment
  tables. Runs once, lazily, on first use -- see G_FindSplinePath.
===============
*/
void G_BuildSplinePaths( void ) {
	int i, j;
	splinePath_t *spline, *target;
	gentity_t *corner;

	if ( splinesBuilt ) {
		return;
	}
	splinesBuilt = qtrue;

	for ( i = 0; i < numSplinePaths; i++ ) {
		spline = &splinePaths[i];

		if ( !spline->targetName[0] ) {
			G_Printf( "info_train_spline_main \"%s\" has no target\n", spline->name );
			continue;
		}

		target = G_FindSplinePathByName( spline->targetName );
		if ( !target ) {
			G_Printf( "info_train_spline_main \"%s\" can't find target \"%s\"\n", spline->name, spline->targetName );
			continue;
		}
		spline->next = target;

		for ( j = 0; j < spline->numControls; j++ ) {
			corner = G_Find( NULL, FOFS( targetname ), spline->controlNames[j] );
			if ( !corner ) {
				G_Printf( "info_train_spline_main \"%s\" can't find control point \"%s\"\n", spline->name, spline->controlNames[j] );
				continue;
			}
			VectorCopy( corner->s.origin, spline->controls[j] );
		}
	}

	// segment tables need every leg's "next" resolved first, so this is a second pass
	for ( i = 0; i < numSplinePaths; i++ ) {
		if ( splinePaths[i].next ) {
			G_ComputeSplineSegments( &splinePaths[i] );
		}
	}
}

/*
===============
G_FindSplinePath

  Public lookup by targetname, used by G_ScriptAction_FollowSpline. Builds
  the spline table on first call.
===============
*/
splinePath_t *G_FindSplinePath( const char *name ) {
	G_BuildSplinePaths();
	return G_FindSplinePathByName( name );
}

/*
===============
G_EvaluateSplinePosition

  Writes ent's world-space origin/angles for its current spline leg at
  atTime, via a direct lookup into the precomputed segment table.
===============
*/
void G_EvaluateSplinePosition( gentity_t *ent, int atTime, vec3_t origin, vec3_t angles ) {
	splinePath_t *spline = ent->splinePath;
	float deltaTime;
	int pos;
	float frac;

	if ( !spline || !spline->next ) {
		VectorCopy( ent->r.currentOrigin, origin );
		VectorCopy( ent->r.currentAngles, angles );
		return;
	}

	deltaTime = ent->s.pos.trDuration ? (float)( atTime - ent->s.pos.trTime ) / (float)ent->s.pos.trDuration : 0.0f;
	if ( deltaTime < 0.0f ) {
		deltaTime = 0.0f;
	} else if ( deltaTime > 1.0f ) {
		deltaTime = 1.0f;
	}

	if ( ent->splineBackward ) {
		deltaTime = 1.0f - deltaTime;
	}

	pos = (int)floor( deltaTime * MAX_SPLINE_SEGMENTS );
	if ( pos >= MAX_SPLINE_SEGMENTS ) {
		pos = MAX_SPLINE_SEGMENTS - 1;
		frac = spline->segments[pos].length;
	} else if ( pos < 0 ) {
		pos = 0;
		frac = 0.0f;
	} else {
		frac = ( ( deltaTime * MAX_SPLINE_SEGMENTS ) - pos ) * spline->segments[pos].length;
	}

	VectorMA( spline->segments[pos].start, frac, spline->segments[pos].v_norm, origin );
	vectoangles( spline->segments[pos].v_norm, angles );

	// banking: linear roll lerp over the leg
	angles[ROLL] = ent->splineRollStart + deltaTime * ( ent->splineRollEnd - ent->splineRollStart );
}
