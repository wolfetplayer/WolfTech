// navgen_patch.cpp -- ported from code/qcommon/cm_patch.c; see navgen_patch.h.

#include "navgen_patch.h"

#define SUBDIVIDE_DISTANCE  16.0f
#define WRAP_POINT_EPSILON  0.1f
#define POINT_EPSILON       0.1f

/*
===================
CM_NeedsSubdivision
===================
*/
static qboolean CM_NeedsSubdivision( vec3_t a, vec3_t b, vec3_t c ) {
	vec3_t cmid, lmid, delta;
	float dist;
	int i;

	for ( i = 0; i < 3; i++ ) {
		lmid[i] = 0.5f * ( a[i] + c[i] );
	}
	for ( i = 0; i < 3; i++ ) {
		cmid[i] = 0.5f * ( 0.5f * ( a[i] + b[i] ) + 0.5f * ( b[i] + c[i] ) );
	}

	VectorSubtract( cmid, lmid, delta );
	dist = VectorLength( delta );

	return dist >= SUBDIVIDE_DISTANCE ? qtrue : qfalse;
}

/*
===========
CM_Subdivide
===========
*/
static void CM_Subdivide( vec3_t a, vec3_t b, vec3_t c, vec3_t out1, vec3_t out2, vec3_t out3 ) {
	int i;
	for ( i = 0; i < 3; i++ ) {
		out1[i] = 0.5f * ( a[i] + b[i] );
		out3[i] = 0.5f * ( b[i] + c[i] );
		out2[i] = 0.5f * ( out1[i] + out3[i] );
	}
}

/*
================
CM_TransposeGrid
================
*/
static void CM_TransposeGrid( cGrid_t *grid ) {
	int i, j, l;
	vec3_t temp;
	qboolean tempWrap;

	if ( grid->width > grid->height ) {
		for ( i = 0; i < grid->height; i++ ) {
			for ( j = i + 1; j < grid->width; j++ ) {
				if ( j < grid->height ) {
					VectorCopy( grid->points[i][j], temp );
					VectorCopy( grid->points[j][i], grid->points[i][j] );
					VectorCopy( temp, grid->points[j][i] );
				} else {
					VectorCopy( grid->points[j][i], grid->points[i][j] );
				}
			}
		}
	} else {
		for ( i = 0; i < grid->width; i++ ) {
			for ( j = i + 1; j < grid->height; j++ ) {
				if ( j < grid->width ) {
					VectorCopy( grid->points[j][i], temp );
					VectorCopy( grid->points[i][j], grid->points[j][i] );
					VectorCopy( temp, grid->points[i][j] );
				} else {
					VectorCopy( grid->points[i][j], grid->points[j][i] );
				}
			}
		}
	}

	l = grid->width;
	grid->width = grid->height;
	grid->height = l;

	tempWrap = grid->wrapWidth;
	grid->wrapWidth = grid->wrapHeight;
	grid->wrapHeight = tempWrap;
}

/*
===================
CM_SetGridWrapWidth
===================
*/
static void CM_SetGridWrapWidth( cGrid_t *grid ) {
	int i, j;
	float d;

	for ( i = 0; i < grid->height; i++ ) {
		for ( j = 0; j < 3; j++ ) {
			d = grid->points[0][i][j] - grid->points[grid->width - 1][i][j];
			if ( d < -WRAP_POINT_EPSILON || d > WRAP_POINT_EPSILON ) {
				break;
			}
		}
		if ( j != 3 ) {
			break;
		}
	}
	grid->wrapWidth = ( i == grid->height ) ? qtrue : qfalse;
}

/*
=======================
CM_SubdivideGridColumns
=======================
*/
static void CM_SubdivideGridColumns( cGrid_t *grid ) {
	int i, j, k;

	for ( i = 0; i < grid->width - 2; ) {
		for ( j = 0; j < grid->height; j++ ) {
			if ( CM_NeedsSubdivision( grid->points[i][j], grid->points[i + 1][j], grid->points[i + 2][j] ) ) {
				break;
			}
		}
		if ( j == grid->height ) {
			for ( j = 0; j < grid->height; j++ ) {
				for ( k = i + 2; k < grid->width; k++ ) {
					VectorCopy( grid->points[k][j], grid->points[k - 1][j] );
				}
			}
			grid->width--;
			i++;
			continue;
		}

		for ( j = 0; j < grid->height; j++ ) {
			vec3_t prev, mid, next;

			VectorCopy( grid->points[i][j], prev );
			VectorCopy( grid->points[i + 1][j], mid );
			VectorCopy( grid->points[i + 2][j], next );

			for ( k = grid->width - 1; k > i + 1; k-- ) {
				VectorCopy( grid->points[k][j], grid->points[k + 2][j] );
			}

			CM_Subdivide( prev, mid, next, grid->points[i + 1][j], grid->points[i + 2][j], grid->points[i + 3][j] );
		}

		grid->width += 2;
	}
}

/*
================
CM_ComparePoints
================
*/
static qboolean CM_ComparePoints( const float *a, const float *b ) {
	float d;

	d = a[0] - b[0];
	if ( d < -POINT_EPSILON || d > POINT_EPSILON ) {
		return qfalse;
	}
	d = a[1] - b[1];
	if ( d < -POINT_EPSILON || d > POINT_EPSILON ) {
		return qfalse;
	}
	d = a[2] - b[2];
	if ( d < -POINT_EPSILON || d > POINT_EPSILON ) {
		return qfalse;
	}
	return qtrue;
}

/*
=========================
CM_RemoveDegenerateColumns
=========================
*/
static void CM_RemoveDegenerateColumns( cGrid_t *grid ) {
	int i, j, k;

	for ( i = 0; i < grid->width - 1; i++ ) {
		for ( j = 0; j < grid->height; j++ ) {
			if ( !CM_ComparePoints( grid->points[i][j], grid->points[i + 1][j] ) ) {
				break;
			}
		}
		if ( j != grid->height ) {
			continue;
		}

		for ( j = 0; j < grid->height; j++ ) {
			for ( k = i + 2; k < grid->width; k++ ) {
				VectorCopy( grid->points[k][j], grid->points[k - 1][j] );
			}
		}
		grid->width--;
		i--;
	}
}

/*
====================
NavGen_SubdividePatch
====================
*/
void NavGen_SubdividePatch( cGrid_t *grid ) {
	CM_SetGridWrapWidth( grid );
	CM_SubdivideGridColumns( grid );
	CM_RemoveDegenerateColumns( grid );

	CM_TransposeGrid( grid );

	CM_SetGridWrapWidth( grid );
	CM_SubdivideGridColumns( grid );
	CM_RemoveDegenerateColumns( grid );
}
