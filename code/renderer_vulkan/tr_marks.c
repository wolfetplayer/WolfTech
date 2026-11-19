// tr_marks.c -- see tr_marks.h; ported from code/renderer's tr_marks.c (GPLv2-or-later), simplified: no SF_FACE/SF_GRID/SF_TRIANGLES split (bspSurface_t is already pre-triangulated), a flat linear surface scan instead of R_BoxSurfaces_r's BSP walk, and always the "old mapping" raw-xyz output (cgame computes planar UVs itself) -- see R_MarkFragments.

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

// Clips one triangle against all numPlanes, appending the surviving fragment (raw xyz, no ST -- see tr_marks.h).
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

int R_MarkFragments( int orientation, const vec3_t *points, const vec3_t projection,
					  int maxPoints, vec3_t pointBuffer, int maxFragments, markFragment_t *fragmentBuffer ) {
	const int numPoints = 4; // cgame's CG_ImpactMark always passes a 4-corner quad
	vec3_t projectionDir, negProjection;
	vec3_t normals[6];
	float dists[6];
	float radius, clipDepth;
	vec3_t mins, maxs;
	int returnedPoints = 0, returnedFragments = 0;
	int i, surfIdx;

	(void)orientation; // only meaningful for the surface-fitted UV mapping this port doesn't do, see tr_marks.c's header comment

	if ( maxFragments < 0 ) {
		maxFragments = -maxFragments; // upstream's "old mapping" request -- this port always behaves that way
	}
	if ( !tr_world.loaded || maxPoints <= 0 || maxFragments <= 0 ) {
		return 0;
	}

	radius = VectorNormalize2( projection, projectionDir ) * 0.5f;
	if ( radius <= 0.0f ) {
		return 0;
	}
	clipDepth = radius * 11.0f; // matches code/renderer's old-mapping clip depth

	ClearBounds( mins, maxs );
	for ( i = 0; i < numPoints; i++ ) {
		vec3_t temp;
		AddPointToBounds( points[i], mins, maxs );
		VectorMA( points[i], clipDepth, projectionDir, temp );
		AddPointToBounds( temp, mins, maxs );
		VectorMA( points[i], -clipDepth, projectionDir, temp );
		AddPointToBounds( temp, mins, maxs );
	}

	// Self-corrects each edge plane's sign by requiring the quad's own center to be kept by it,
	// rather than trusting the cross-product order to come out inward-pointing (it measured as not).
	{
		vec3_t center;
		VectorClear( center );
		for ( i = 0; i < numPoints; i++ ) {
			VectorAdd( center, points[i], center );
		}
		VectorScale( center, 1.0f / numPoints, center );

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
	dists[numPoints] = DotProduct( normals[numPoints], points[0] ) - clipDepth;
	VectorNegate( projectionDir, normals[numPoints + 1] );
	dists[numPoints + 1] = DotProduct( normals[numPoints + 1], points[0] ) - clipDepth;

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

			R_AddMarkFragment( triangle, numPoints + 2, normals, dists, maxPoints, (float *)pointBuffer,
							   maxFragments, fragmentBuffer, &returnedPoints, &returnedFragments );
			if ( returnedFragments >= maxFragments ) {
				return returnedFragments;
			}
		}
	}

	return returnedFragments;
}
