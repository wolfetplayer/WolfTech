// tr_marks.c -- see tr_marks.h

#include <math.h>
#include <string.h>

#include "tr_local.h"
#include "tr_bsp.h"
#include "tr_marks.h"

#define MAX_VERTS_ON_POLY 64

#define SIDE_FRONT 0
#define SIDE_BACK 1
#define SIDE_ON 2

// out must have room for two more vertexes than in; ported verbatim from code/renderer/tr_marks.c.
static void R_ChopPolyBehindPlane( int numInPoints, vec3_t inPoints[MAX_VERTS_ON_POLY],
									int *numOutPoints, vec3_t outPoints[MAX_VERTS_ON_POLY],
									const vec3_t normal, float dist, float epsilon ) {
	float dists[MAX_VERTS_ON_POLY + 4];
	int sides[MAX_VERTS_ON_POLY + 4];
	int counts[3];
	int i, j;
	float dot, d;
	float *p1, *p2, *clip;

	if ( numInPoints >= MAX_VERTS_ON_POLY - 2 ) {
		*numOutPoints = 0;
		return; // don't clip if it might overflow
	}

	counts[0] = counts[1] = counts[2] = 0;
	for ( i = 0; i < numInPoints; i++ ) {
		dot = DotProduct( inPoints[i], normal ) - dist;
		dists[i] = dot;
		sides[i] = ( dot > epsilon ) ? SIDE_FRONT : ( dot < -epsilon ) ? SIDE_BACK : SIDE_ON;
		counts[sides[i]]++;
	}
	sides[i] = sides[0];
	dists[i] = dists[0];

	*numOutPoints = 0;
	if ( !counts[SIDE_FRONT] ) {
		return;
	}
	if ( !counts[SIDE_BACK] ) {
		*numOutPoints = numInPoints;
		memcpy( outPoints, inPoints, numInPoints * sizeof( vec3_t ) );
		return;
	}

	for ( i = 0; i < numInPoints; i++ ) {
		p1 = inPoints[i];
		clip = outPoints[*numOutPoints];

		if ( sides[i] == SIDE_ON ) {
			VectorCopy( p1, clip );
			( *numOutPoints )++;
			continue;
		}
		if ( sides[i] == SIDE_FRONT ) {
			VectorCopy( p1, clip );
			( *numOutPoints )++;
			clip = outPoints[*numOutPoints];
		}
		if ( sides[i + 1] == SIDE_ON || sides[i + 1] == sides[i] ) {
			continue;
		}

		p2 = inPoints[( i + 1 ) % numInPoints];
		d = dists[i] - dists[i + 1];
		dot = ( d == 0.0f ) ? 0.0f : dists[i] / d;
		for ( j = 0; j < 3; j++ ) {
			clip[j] = p1[j] + dot * ( p2[j] - p1[j] );
		}
		( *numOutPoints )++;
	}
}

static void R_AddMarkFragment( vec3_t triangle[3], int numPlanes, const vec3_t *normals, const float *dists,
								int maxPoints, float *pointBuffer, int maxFragments, markFragment_t *fragmentBuffer,
								int *returnedPoints, int *returnedFragments ) {
	vec3_t clipPoints[2][MAX_VERTS_ON_POLY];
	int numClipPoints = 3;
	int pingPong = 0;
	int i;
	markFragment_t *mf;

	VectorCopy( triangle[0], clipPoints[0][0] );
	VectorCopy( triangle[1], clipPoints[0][1] );
	VectorCopy( triangle[2], clipPoints[0][2] );

	for ( i = 0; i < numPlanes; i++ ) {
		R_ChopPolyBehindPlane( numClipPoints, clipPoints[pingPong], &numClipPoints, clipPoints[!pingPong], normals[i], dists[i], 0.5f );
		pingPong ^= 1;
		if ( numClipPoints == 0 ) {
			return;
		}
	}

	if ( numClipPoints + *returnedPoints > maxPoints || *returnedFragments >= maxFragments ) {
		return; // not enough space for this fragment
	}

	mf = fragmentBuffer + *returnedFragments;
	mf->firstPoint = *returnedPoints;
	mf->numPoints = numClipPoints;
	for ( i = 0; i < numClipPoints; i++ ) {
		VectorCopy( clipPoints[pingPong][i], pointBuffer + 5 * ( *returnedPoints + i ) );
	}

	*returnedPoints += numClipPoints;
	( *returnedFragments )++;
}

static void ClipSurfaceOldStyle( const bspSurface_t *surf, const vec3_t *normals, const float *dists, int numPlanes,
								  int maxPoints, float *pointBuffer, int maxFragments, markFragment_t *fragmentBuffer,
								  int *returnedPoints, int *returnedFragments ) {
	int k;

	for ( k = 0; k < surf->numIndexes; k += 3 ) {
		vec3_t triangle[3];
		const drawVert_t *v0, *v1, *v2;

		v0 = &tr_world.drawVerts[surf->firstVert + tr_world.drawIndexes[surf->firstIndex + k]];
		v1 = &tr_world.drawVerts[surf->firstVert + tr_world.drawIndexes[surf->firstIndex + k + 1]];
		v2 = &tr_world.drawVerts[surf->firstVert + tr_world.drawIndexes[surf->firstIndex + k + 2]];

		// No facing check here, matching upstream's SF_FACE/old-mapping path (which has one commented out).
		VectorCopy( v0->xyz, triangle[0] );
		VectorCopy( v1->xyz, triangle[1] );
		VectorCopy( v2->xyz, triangle[2] );

		R_AddMarkFragment( triangle, numPlanes, normals, dists, maxPoints, pointBuffer,
						   maxFragments, fragmentBuffer, returnedPoints, returnedFragments );
		if ( *returnedFragments >= maxFragments ) {
			return;
		}
	}
}

static void ClipSurfaceNewMapping( const bspSurface_t *surf, const vec3_t center, const vec3_t projectionDir,
									float radius, int orientation, int maxPoints, float *pointBuffer,
									int maxFragments, markFragment_t *fragmentBuffer,
									int *returnedPoints, int *returnedFragments ) {
	static const float stCorner[4][2] = { { -1, -1 }, { 1, -1 }, { 1, 1 }, { -1, 1 } };
	const float *surfNormal = tr_world.drawVerts[surf->firstVert].normal; // MST_PLANAR -- every vertex shares one normal
	vec3_t axis0, axis1, axis2, rotated;
	vec3_t newCenter;
	vec3_t quadCorners[4];
	vec3_t lnormals[4];
	float ldists[4];
	float surfDist, dot, texCoordScale;
	int j, k;

	surfDist = DotProduct( surfNormal, tr_world.drawVerts[surf->firstVert].xyz );

	// Is this surface actually the (or a) one hit, close enough to the impact to matter?
	dot = DotProduct( center, surfNormal ) - surfDist;
	if ( ( dot < -0.5f && DotProduct( surfNormal, projectionDir ) >= 0.01f ) || fabsf( dot ) > radius ) {
		return;
	}

	VectorMA( center, -dot, projectionDir, newCenter );
	dot = DotProduct( newCenter, surfNormal ) - surfDist;
	VectorMA( newCenter, -dot, surfNormal, newCenter );

	// Build an in-plane tangent basis (axis1/axis2) at the mark's roll angle (orientation).
	VectorCopy( surfNormal, axis0 );
	VectorNormalize( axis0 );
	PerpendicularVector( axis1, axis0 );
	RotatePointAroundVector( rotated, axis0, axis1, (float)orientation );
	VectorCopy( rotated, axis2 );
	CrossProduct( axis0, axis2, axis1 );

	texCoordScale = 0.5f / radius;

	for ( j = 0; j < 4; j++ ) {
		VectorMA( newCenter, stCorner[j][0] * radius, axis1, quadCorners[j] );
		VectorMA( quadCorners[j], stCorner[j][1] * radius, axis2, quadCorners[j] );
	}

	for ( j = 0; j < 4; j++ ) {
		vec3_t edge, toCenter;
		VectorSubtract( quadCorners[( j + 1 ) % 4], quadCorners[j], edge );
		CrossProduct( edge, axis0, lnormals[j] );
		VectorNormalize( lnormals[j] );
		VectorSubtract( newCenter, quadCorners[j], toCenter );
		if ( DotProduct( toCenter, lnormals[j] ) < 0.0f ) {
			VectorNegate( lnormals[j], lnormals[j] );
		}
		ldists[j] = DotProduct( lnormals[j], quadCorners[j] );
	}

	for ( k = 0; k < surf->numIndexes; k += 3 ) {
		vec3_t triangle[3];
		const drawVert_t *v0, *v1, *v2;
		int oldReturnedPoints = *returnedPoints;
		int p;

		v0 = &tr_world.drawVerts[surf->firstVert + tr_world.drawIndexes[surf->firstIndex + k]];
		v1 = &tr_world.drawVerts[surf->firstVert + tr_world.drawIndexes[surf->firstIndex + k + 1]];
		v2 = &tr_world.drawVerts[surf->firstVert + tr_world.drawIndexes[surf->firstIndex + k + 2]];

		VectorCopy( v0->xyz, triangle[0] );
		VectorCopy( v1->xyz, triangle[1] );
		VectorCopy( v2->xyz, triangle[2] );

		R_AddMarkFragment( triangle, 4, lnormals, ldists, maxPoints, pointBuffer,
						   maxFragments, fragmentBuffer, returnedPoints, returnedFragments );

		if ( *returnedPoints != oldReturnedPoints ) {
			// cg_marks.c (CG_ImpactMark) checks for a negative numPoints to mean "ST is already filled in".
			fragmentBuffer[*returnedFragments - 1].numPoints *= -1;
			for ( p = oldReturnedPoints; p < *returnedPoints; p++ ) {
				vec3_t delta;
				float *dst = pointBuffer + 5 * p;
				VectorSubtract( dst, newCenter, delta );
				dst[3] = 0.5f + DotProduct( delta, axis1 ) * texCoordScale;
				dst[4] = 0.5f + DotProduct( delta, axis2 ) * texCoordScale;
			}
		}

		if ( *returnedFragments >= maxFragments ) {
			return;
		}
	}
}

int R_MarkFragments( int orientation, const vec3_t *points, const vec3_t projection,
					  int maxPoints, float *pointBuffer, int maxFragments, markFragment_t *fragmentBuffer ) {
	const int numPoints = 4; // cgame's CG_ImpactMark always passes a 4-corner quad
	vec3_t projectionDir, negProjection;
	vec3_t normals[6];
	float dists[6];
	float radius, aabbClipDepth, clipPlaneDepth;
	vec3_t mins, maxs, center;
	qboolean oldMapping = qfalse;
	int returnedPoints = 0, returnedFragments = 0;
	int i, surfIdx;

	if ( maxFragments < 0 ) {
		maxFragments = -maxFragments;
		oldMapping = qtrue;
	}
	if ( !tr_world.loaded || maxPoints <= 0 || maxFragments <= 0 ) {
		return 0;
	}

	radius = VectorNormalize2( projection, projectionDir ) * 0.5f;
	if ( radius <= 0.0f ) {
		return 0;
	}
	aabbClipDepth = radius * 11.0f; // generous fixed bound for the coarse per-surface AABB reject below only
	clipPlaneDepth = radius * ( 1.0f + ( oldMapping ? 10.0f : 0.0f ) ); // matches reference's radius*(1+oldMapping*10) for the real clip planes

	VectorClear( center );
	for ( i = 0; i < numPoints; i++ ) {
		VectorAdd( center, points[i], center );
	}
	VectorScale( center, 1.0f / numPoints, center );

	ClearBounds( mins, maxs );
	for ( i = 0; i < numPoints; i++ ) {
		vec3_t temp;
		AddPointToBounds( points[i], mins, maxs );
		VectorMA( points[i], aabbClipDepth, projectionDir, temp );
		AddPointToBounds( temp, mins, maxs );
		VectorMA( points[i], -aabbClipDepth, projectionDir, temp );
		AddPointToBounds( temp, mins, maxs );
	}

	{
		VectorNegate( projection, negProjection );
		for ( i = 0; i < numPoints; i++ ) {
			vec3_t edge, toCenter;
			VectorSubtract( points[( i + 1 ) % numPoints], points[i], edge );
			CrossProduct( edge, negProjection, normals[i] );
			VectorNormalize( normals[i] );
			VectorSubtract( center, points[i], toCenter );
			if ( DotProduct( toCenter, normals[i] ) < 0.0f ) {
				VectorNegate( normals[i], normals[i] );
			}
			dists[i] = DotProduct( normals[i], points[i] );
		}
	}
	VectorCopy( projectionDir, normals[numPoints] );
	dists[numPoints] = DotProduct( normals[numPoints], points[0] ) - clipPlaneDepth;
	VectorNegate( projectionDir, normals[numPoints + 1] );
	dists[numPoints + 1] = DotProduct( normals[numPoints + 1], points[0] ) - clipPlaneDepth;

	for ( surfIdx = 0; surfIdx < tr_world.numSurfaces; surfIdx++ ) {
		const bspSurface_t *surf = &tr_world.surfaces[surfIdx];
		const bspShader_t *shader;
		vec3_t surfMins, surfMaxs;
		int k;

		// MST_PATCH stores raw control points here, not triangles -- scoped out, see the header comment.
		if ( surf->surfaceType != MST_PLANAR && surf->surfaceType != MST_TRIANGLE_SOUP ) {
			continue;
		}

		shader = &tr_world.shaders[surf->shaderNum];
		if ( ( shader->surfaceFlags & ( SURF_NOIMPACT | SURF_NOMARKS ) ) || ( shader->contentFlags & CONTENTS_FOG ) ) {
			continue;
		}

		ClearBounds( surfMins, surfMaxs );
		for ( k = 0; k < surf->numVerts; k++ ) {
			AddPointToBounds( tr_world.drawVerts[surf->firstVert + k].xyz, surfMins, surfMaxs );
		}
		if ( surfMins[0] > maxs[0] || surfMaxs[0] < mins[0] || surfMins[1] > maxs[1] || surfMaxs[1] < mins[1] ||
			 surfMins[2] > maxs[2] || surfMaxs[2] < mins[2] ) {
			continue; // surface's bounds don't overlap the decal's -- see the AABB test above
		}

		if ( !oldMapping && surf->surfaceType == MST_PLANAR ) {
			ClipSurfaceNewMapping( surf, center, projectionDir, radius, orientation, maxPoints, pointBuffer,
								   maxFragments, fragmentBuffer, &returnedPoints, &returnedFragments );
		} else {
			ClipSurfaceOldStyle( surf, normals, dists, numPoints + 2, maxPoints, pointBuffer,
								maxFragments, fragmentBuffer, &returnedPoints, &returnedFragments );
		}
		if ( returnedFragments >= maxFragments ) {
			return returnedFragments;
		}
	}

	return returnedFragments;
}
