// tr_patch.c -- adaptive bezier tessellation for MST_PATCH surfaces, see tr_patch.h.

#include <string.h>
#include <math.h>

#include "tr_local.h"
#include "tr_patch.h"

static cvar_t *r_subdivisions;

#define MIN_SUBDIVISIONS 1
#define MAX_SUBDIVISIONS 16

void R_InitPatches( void ) {
	r_subdivisions = ri.Cvar_Get( "r_subdivisions", "4", CVAR_LATCH );
	ri.Cvar_CheckRange( r_subdivisions, MIN_SUBDIVISIONS, MAX_SUBDIVISIONS, qtrue );
}

// A valid Q3 patch control grid is always odd-sized in both dimensions (a run of shared 3x3 biquad patches).
static qboolean R_PatchGridIsValid( int patchWidth, int patchHeight ) {
	return patchWidth >= 3 && patchHeight >= 3 &&
		   ( ( patchWidth - 1 ) % 2 ) == 0 && ( ( patchHeight - 1 ) % 2 ) == 0;
}

// Curve's "sagitta": how far its t=0.5 Bezier point bulges away from the straight p0-p2 chord.
static float Sagitta( const vec3_t p0, const vec3_t p1, const vec3_t p2 ) {
	vec3_t mid, chordDir;
	float d, len;
	int k;

	for ( k = 0; k < 3; k++ ) {
		mid[k] = ( p0[k] + p1[k] * 2.0f + p2[k] ) * 0.25f;
	}
	VectorSubtract( mid, p0, mid );
	VectorSubtract( p2, p0, chordDir );
	len = VectorNormalize( chordDir );
	if ( len < 0.001f ) {
		return 0.0f; // degenerate (coincident chord endpoints)
	}
	d = DotProduct( mid, chordDir );
	VectorMA( mid, -d, chordDir, mid );
	return VectorLength( mid );
}

// r_subdivisions is a world-unit error tolerance here, not a literal segment count.
static int R_ComputeAdaptiveSubdiv( const drawVert_t *controlVerts, int patchWidth, int patchHeight ) {
	int numPatchesX = ( patchWidth - 1 ) / 2;
	int numPatchesY = ( patchHeight - 1 ) / 2;
	float tolerance = r_subdivisions->value;
	float maxSagitta = 0.0f;
	int px, py, r, c, subdiv;

	for ( py = 0; py < numPatchesY; py++ ) {
		for ( px = 0; px < numPatchesX; px++ ) {
			for ( r = 0; r < 3; r++ ) {
				const float *p0 = controlVerts[( py * 2 + r ) * patchWidth + ( px * 2 + 0 )].xyz;
				const float *p1 = controlVerts[( py * 2 + r ) * patchWidth + ( px * 2 + 1 )].xyz;
				const float *p2 = controlVerts[( py * 2 + r ) * patchWidth + ( px * 2 + 2 )].xyz;
				float s = Sagitta( p0, p1, p2 );
				if ( s > maxSagitta ) maxSagitta = s;
			}
			for ( c = 0; c < 3; c++ ) {
				const float *p0 = controlVerts[( py * 2 + 0 ) * patchWidth + ( px * 2 + c )].xyz;
				const float *p1 = controlVerts[( py * 2 + 1 ) * patchWidth + ( px * 2 + c )].xyz;
				const float *p2 = controlVerts[( py * 2 + 2 ) * patchWidth + ( px * 2 + c )].xyz;
				float s = Sagitta( p0, p1, p2 );
				if ( s > maxSagitta ) maxSagitta = s;
			}
		}
	}

	if ( tolerance <= 0.0f || maxSagitta <= tolerance ) {
		subdiv = 1;
	} else {
		// Each binary split quarters the deviation, so k splits (2^k segments) need 4^k >= sagitta/tolerance.
		float ratio = maxSagitta / tolerance;
		int k = (int)ceilf( logf( ratio ) / logf( 4.0f ) );
		if ( k < 0 ) k = 0;
		subdiv = 1 << k;
	}

	if ( subdiv < MIN_SUBDIVISIONS ) subdiv = MIN_SUBDIVISIONS;
	if ( subdiv > MAX_SUBDIVISIONS ) subdiv = MAX_SUBDIVISIONS;
	return subdiv;
}

int R_PatchTessVertCount( const drawVert_t *controlVerts, int patchWidth, int patchHeight ) {
	int subdiv, numPatchesX, numPatchesY, vertsPerSide;

	if ( !R_PatchGridIsValid( patchWidth, patchHeight ) ) {
		return 0;
	}

	subdiv = R_ComputeAdaptiveSubdiv( controlVerts, patchWidth, patchHeight );
	numPatchesX = ( patchWidth - 1 ) / 2;
	numPatchesY = ( patchHeight - 1 ) / 2;
	vertsPerSide = subdiv + 1;

	return numPatchesX * numPatchesY * vertsPerSide * vertsPerSide;
}

int R_PatchTessIndexCount( const drawVert_t *controlVerts, int patchWidth, int patchHeight ) {
	int subdiv, numPatchesX, numPatchesY;

	if ( !R_PatchGridIsValid( patchWidth, patchHeight ) ) {
		return 0;
	}

	subdiv = R_ComputeAdaptiveSubdiv( controlVerts, patchWidth, patchHeight );
	numPatchesX = ( patchWidth - 1 ) / 2;
	numPatchesY = ( patchHeight - 1 ) / 2;

	return numPatchesX * numPatchesY * subdiv * subdiv * 6;
}

static void BezierBasis( float t, float b[3] ) {
	float it = 1.0f - t;
	b[0] = it * it;
	b[1] = 2.0f * t * it;
	b[2] = t * t;
}

// Biquadratic bezier blend of a 3x3 control patch's drawVert attributes at (u,v).
static void EvalPatchVert( const drawVert_t *cp[3][3], float u, float v, drawVert_t *out ) {
	float bu[3], bv[3];
	float xyz[3] = { 0, 0, 0 }, st[2] = { 0, 0 }, lm[2] = { 0, 0 }, normal[3] = { 0, 0, 0 }, color[4] = { 0, 0, 0, 0 };
	int r, c, k, ic;

	BezierBasis( u, bu );
	BezierBasis( v, bv );

	for ( r = 0; r < 3; r++ ) {
		for ( c = 0; c < 3; c++ ) {
			float w = bv[r] * bu[c];
			const drawVert_t *p = cp[r][c];

			for ( k = 0; k < 3; k++ ) xyz[k] += w * p->xyz[k];
			for ( k = 0; k < 2; k++ ) st[k] += w * p->st[k];
			for ( k = 0; k < 2; k++ ) lm[k] += w * p->lightmap[k];
			for ( k = 0; k < 3; k++ ) normal[k] += w * p->normal[k];
			for ( k = 0; k < 4; k++ ) color[k] += w * (float)p->color[k];
		}
	}

	VectorCopy( xyz, out->xyz );
	out->st[0] = st[0]; out->st[1] = st[1];
	out->lightmap[0] = lm[0]; out->lightmap[1] = lm[1];
	VectorCopy( normal, out->normal );
	VectorNormalize( out->normal );

	for ( k = 0; k < 4; k++ ) {
		ic = (int)( color[k] + 0.5f );
		if ( ic < 0 ) ic = 0;
		if ( ic > 255 ) ic = 255;
		out->color[k] = (byte)ic;
	}
}

// Overlapping 3x3 sub-patches share edge control points, so adjacent tessellated edges line up with no seam.
void R_TessellatePatch( const drawVert_t *controlVerts, int patchWidth, int patchHeight,
						 drawVert_t *outVerts, int *outIndexes ) {
	int subdiv = R_ComputeAdaptiveSubdiv( controlVerts, patchWidth, patchHeight );
	int numPatchesX = ( patchWidth - 1 ) / 2;
	int numPatchesY = ( patchHeight - 1 ) / 2;
	int vertsPerSide = subdiv + 1;
	int vertBase = 0, idxBase = 0;
	int px, py, i, j, r, c;

	for ( py = 0; py < numPatchesY; py++ ) {
		for ( px = 0; px < numPatchesX; px++ ) {
			const drawVert_t *cp[3][3];

			for ( r = 0; r < 3; r++ ) {
				for ( c = 0; c < 3; c++ ) {
					cp[r][c] = &controlVerts[( py * 2 + r ) * patchWidth + ( px * 2 + c )];
				}
			}

			for ( j = 0; j <= subdiv; j++ ) {
				float v = (float)j / (float)subdiv;
				for ( i = 0; i <= subdiv; i++ ) {
					float u = (float)i / (float)subdiv;
					EvalPatchVert( cp, u, v, &outVerts[vertBase + j * vertsPerSide + i] );
				}
			}

			for ( j = 0; j < subdiv; j++ ) {
				for ( i = 0; i < subdiv; i++ ) {
					int v00 = vertBase + j * vertsPerSide + i;
					int v10 = v00 + 1;
					int v01 = v00 + vertsPerSide;
					int v11 = v01 + 1;

					outIndexes[idxBase++] = v00;
					outIndexes[idxBase++] = v01;
					outIndexes[idxBase++] = v10;
					outIndexes[idxBase++] = v10;
					outIndexes[idxBase++] = v01;
					outIndexes[idxBase++] = v11;
				}
			}

			vertBase += vertsPerSide * vertsPerSide;
		}
	}
}
