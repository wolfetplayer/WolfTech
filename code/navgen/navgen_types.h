// navgen_types.h -- minimal, self-contained types shared with qcommon by
// layout only. Deliberately does not include q_shared.h: navgen is a
// standalone tool built the way code/bspc is, not linked into the engine.

#ifndef __NAVGEN_TYPES_H
#define __NAVGEN_TYPES_H

#include <math.h>

typedef unsigned char byte;
typedef enum { qfalse, qtrue } qboolean;
typedef float vec_t;
typedef vec_t vec3_t[3];

#define VectorCopy( a, b )      ( ( b )[0] = ( a )[0], ( b )[1] = ( a )[1], ( b )[2] = ( a )[2] )
#define VectorClear( a )        ( ( a )[0] = ( a )[1] = ( a )[2] = 0 )
#define VectorAdd( a, b, c )    ( ( c )[0] = ( a )[0] + ( b )[0], ( c )[1] = ( a )[1] + ( b )[1], ( c )[2] = ( a )[2] + ( b )[2] )
#define VectorSubtract( a, b, c )  ( ( c )[0] = ( a )[0] - ( b )[0], ( c )[1] = ( a )[1] - ( b )[1], ( c )[2] = ( a )[2] - ( b )[2] )
#define VectorScale( v, s, o )  ( ( o )[0] = ( v )[0] * ( s ), ( o )[1] = ( v )[1] * ( s ), ( o )[2] = ( v )[2] * ( s ) )
#define VectorMA( v, s, b, o )  ( ( o )[0] = ( v )[0] + ( b )[0] * ( s ), ( o )[1] = ( v )[1] + ( b )[1] * ( s ), ( o )[2] = ( v )[2] + ( b )[2] * ( s ) )
#define DotProduct( a, b )      ( ( a )[0] * ( b )[0] + ( a )[1] * ( b )[1] + ( a )[2] * ( b )[2] )
#define CrossProduct( a, b, c ) ( ( c )[0] = ( a )[1] * ( b )[2] - ( a )[2] * ( b )[1], ( c )[1] = ( a )[2] * ( b )[0] - ( a )[0] * ( b )[2], ( c )[2] = ( a )[0] * ( b )[1] - ( a )[1] * ( b )[0] )

static const vec3_t vec3_origin = { 0, 0, 0 };

static inline vec_t VectorLength( const vec3_t v ) {
	return (vec_t)sqrt( DotProduct( v, v ) );
}

// normalizes in place, returns the pre-normalization length
static inline vec_t VectorNormalize2( const vec3_t v, vec3_t out ) {
	vec_t length = VectorLength( v );
	if ( length ) {
		vec_t ilength = 1 / length;
		out[0] = v[0] * ilength;
		out[1] = v[1] * ilength;
		out[2] = v[2] * ilength;
	} else {
		VectorClear( out );
	}
	return length;
}

#endif // __NAVGEN_TYPES_H
