// navgen_winding.cpp -- ported from code/qcommon/cm_polylib.c; see navgen_winding.h.

#include "navgen_winding.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
============
AllocWinding
============
*/
winding_t *AllocWinding( int points ) {
	int s = sizeof( vec_t ) * 3 * points + sizeof( int );
	winding_t *w = (winding_t *)malloc( s );
	memset( w, 0, s );
	return w;
}

/*
===========
FreeWinding
===========
*/
void FreeWinding( winding_t *w ) {
	free( w );
}

/*
====================
RemoveColinearPoints
====================
*/
void RemoveColinearPoints( winding_t *w ) {
	int i, j, k;
	vec3_t v1, v2;
	int nump;
	vec3_t p[MAX_POINTS_ON_WINDING];

	nump = 0;
	for ( i = 0; i < w->numpoints; i++ )
	{
		j = ( i + 1 ) % w->numpoints;
		k = ( i + w->numpoints - 1 ) % w->numpoints;
		VectorSubtract( w->p[j], w->p[i], v1 );
		VectorSubtract( w->p[i], w->p[k], v2 );
		VectorNormalize2( v1, v1 );
		VectorNormalize2( v2, v2 );
		if ( DotProduct( v1, v2 ) < 0.999 ) {
			VectorCopy( w->p[i], p[nump] );
			nump++;
		}
	}

	if ( nump == w->numpoints ) {
		return;
	}

	w->numpoints = nump;
	memcpy( w->p, p, nump * sizeof( p[0] ) );
}

/*
=============
WindingBounds
=============
*/
void WindingBounds( winding_t *w, vec3_t mins, vec3_t maxs ) {
	vec_t v;
	int i, j;

	mins[0] = mins[1] = mins[2] = MAX_MAP_BOUNDS;
	maxs[0] = maxs[1] = maxs[2] = -MAX_MAP_BOUNDS;

	for ( i = 0; i < w->numpoints; i++ )
	{
		for ( j = 0; j < 3; j++ )
		{
			v = w->p[i][j];
			if ( v < mins[j] ) {
				mins[j] = v;
			}
			if ( v > maxs[j] ) {
				maxs[j] = v;
			}
		}
	}
}

/*
===================
BaseWindingForPlane
===================
*/
winding_t *BaseWindingForPlane( vec3_t normal, vec_t dist ) {
	int i, x;
	vec_t max, v;
	vec3_t org, vright, vup;
	winding_t *w;

	max = -MAX_MAP_BOUNDS;
	x = -1;
	for ( i = 0; i < 3; i++ )
	{
		v = (vec_t)fabs( normal[i] );
		if ( v > max ) {
			x = i;
			max = v;
		}
	}
	if ( x == -1 ) {
		fprintf( stderr, "BaseWindingForPlane: no axis found\n" );
		exit( 1 );
	}

	VectorCopy( vec3_origin, vup );
	switch ( x )
	{
	case 0:
	case 1:
		vup[2] = 1;
		break;
	case 2:
		vup[0] = 1;
		break;
	}

	v = DotProduct( vup, normal );
	VectorMA( vup, -v, normal, vup );
	VectorNormalize2( vup, vup );

	VectorScale( normal, dist, org );

	CrossProduct( vup, normal, vright );

	VectorScale( vup, MAX_MAP_BOUNDS, vup );
	VectorScale( vright, MAX_MAP_BOUNDS, vright );

	w = AllocWinding( 4 );

	VectorSubtract( org, vright, w->p[0] );
	VectorAdd( w->p[0], vup, w->p[0] );

	VectorAdd( org, vright, w->p[1] );
	VectorAdd( w->p[1], vup, w->p[1] );

	VectorAdd( org, vright, w->p[2] );
	VectorSubtract( w->p[2], vup, w->p[2] );

	VectorSubtract( org, vright, w->p[3] );
	VectorSubtract( w->p[3], vup, w->p[3] );

	w->numpoints = 4;

	return w;
}

/*
==================
ChopWindingInPlace
==================
*/
void ChopWindingInPlace( winding_t **inout, vec3_t normal, vec_t dist, vec_t epsilon ) {
	winding_t *in;
	vec_t dists[MAX_POINTS_ON_WINDING + 4] = { 0 };
	int sides[MAX_POINTS_ON_WINDING + 4] = { 0 };
	int counts[3];
	vec_t dot;
	int i, j;
	vec_t *p1, *p2;
	vec3_t mid;
	winding_t *f;
	int maxpts;

	in = *inout;
	counts[0] = counts[1] = counts[2] = 0;

	for ( i = 0; i < in->numpoints; i++ )
	{
		dot = DotProduct( in->p[i], normal );
		dot -= dist;
		dists[i] = dot;
		if ( dot > epsilon ) {
			sides[i] = 0; // SIDE_FRONT
		} else if ( dot < -epsilon ) {
			sides[i] = 1; // SIDE_BACK
		} else {
			sides[i] = 2; // SIDE_ON
		}
		counts[sides[i]]++;
	}
	sides[i] = sides[0];
	dists[i] = dists[0];

	if ( !counts[0] ) {
		FreeWinding( in );
		*inout = NULL;
		return;
	}
	if ( !counts[1] ) {
		return;
	}

	maxpts = in->numpoints + 4;
	f = AllocWinding( maxpts );

	for ( i = 0; i < in->numpoints; i++ )
	{
		p1 = in->p[i];

		if ( sides[i] == 2 ) {
			VectorCopy( p1, f->p[f->numpoints] );
			f->numpoints++;
			continue;
		}

		if ( sides[i] == 0 ) {
			VectorCopy( p1, f->p[f->numpoints] );
			f->numpoints++;
		}

		if ( sides[i + 1] == 2 || sides[i + 1] == sides[i] ) {
			continue;
		}

		p2 = in->p[( i + 1 ) % in->numpoints];

		dot = dists[i] / ( dists[i] - dists[i + 1] );
		for ( j = 0; j < 3; j++ )
		{
			if ( normal[j] == 1 ) {
				mid[j] = dist;
			} else if ( normal[j] == -1 ) {
				mid[j] = -dist;
			} else {
				mid[j] = p1[j] + dot * ( p2[j] - p1[j] );
			}
		}

		VectorCopy( mid, f->p[f->numpoints] );
		f->numpoints++;
	}

	if ( f->numpoints > maxpts || f->numpoints > MAX_POINTS_ON_WINDING ) {
		fprintf( stderr, "ChopWindingInPlace: point count exceeded\n" );
		exit( 1 );
	}

	FreeWinding( in );
	*inout = f;
}
