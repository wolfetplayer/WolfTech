// navgen_winding.h -- winding math ported from code/qcommon/cm_polylib.c.

#ifndef __NAVGEN_WINDING_H
#define __NAVGEN_WINDING_H

#include "navgen_types.h"

#define MAX_POINTS_ON_WINDING   64
#define MAX_MAP_BOUNDS          ( 128 * 1024 )
#define NAVGEN_ON_EPSILON       0.1f

typedef struct {
	int numpoints;
	vec3_t p[4]; // allocated larger than this; see AllocWinding
} winding_t;

winding_t *AllocWinding( int points );
void FreeWinding( winding_t *w );
void RemoveColinearPoints( winding_t *w );
void WindingBounds( winding_t *w, vec3_t mins, vec3_t maxs );
winding_t *BaseWindingForPlane( vec3_t normal, vec_t dist );
void ChopWindingInPlace( winding_t **inout, vec3_t normal, vec_t dist, vec_t epsilon );

#endif // __NAVGEN_WINDING_H
