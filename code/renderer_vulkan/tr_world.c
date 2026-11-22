// tr_world.c -- marks which world surfaces are visible this frame (BSP point-in-leaf + PVS + frustum).

#include <stdlib.h>
#include <string.h>

#include "tr_local.h"
#include "tr_main.h"
#include "tr_bsp.h"
#include "tr_world.h"

static qboolean *s_visibleSurfaces;
static int s_numAllocated;

static int R_PointInLeaf( const vec3_t p ) {
	int nodeIndex = 0;

	if ( tr_world.numNodes == 0 ) {
		return 0;
	}

	while ( nodeIndex >= 0 ) {
		const dnode_t *node = &tr_world.nodes[nodeIndex];
		const dplane_t *plane = &tr_world.planes[node->planeNum];
		float d = DotProduct( p, plane->normal ) - plane->dist;
		nodeIndex = ( d >= 0.0f ) ? node->children[0] : node->children[1];
	}

	return -nodeIndex - 1;
}

static qboolean R_ClusterIsVisible( int camCluster, int cluster ) {
	const byte *row;

	if ( camCluster < 0 || !tr_world.visData || tr_world.clusterBytes == 0 ) {
		return qtrue; // no PVS data to cull by
	}
	if ( cluster < 0 ) {
		return qfalse; // solid/outside leaf
	}

	row = tr_world.visData + camCluster * tr_world.clusterBytes;
	return ( row[cluster >> 3] & ( 1 << ( cluster & 7 ) ) ) != 0;
}

// Scans every leaf rather than descending the node tree -- a few thousand checks a frame is cheap.
void R_MarkVisibleSurfaces( const refdef_t *fd ) {
	cullPlane_t frustum[NUM_FRUSTUM_PLANES];
	int camLeaf, camCluster;
	int i, j;

	if ( !tr_world.loaded || tr_world.numSurfaces == 0 ) {
		return;
	}

	if ( s_numAllocated != tr_world.numSurfaces ) {
		free( s_visibleSurfaces );
		s_visibleSurfaces = malloc( tr_world.numSurfaces * sizeof( qboolean ) );
		s_numAllocated = tr_world.numSurfaces;
	}
	memset( s_visibleSurfaces, 0, s_numAllocated * sizeof( qboolean ) );

	camLeaf = R_PointInLeaf( fd->vieworg );
	camCluster = ( camLeaf >= 0 && camLeaf < tr_world.numLeafs ) ? tr_world.leafs[camLeaf].cluster : -1;

	R_SetupFrustumPlanes( fd, frustum );

	for ( i = 0; i < tr_world.numLeafs; i++ ) {
		const dleaf_t *leaf = &tr_world.leafs[i];
		vec3_t mins, maxs;

		if ( !R_ClusterIsVisible( camCluster, leaf->cluster ) ) {
			continue;
		}

		mins[0] = (float)leaf->mins[0]; mins[1] = (float)leaf->mins[1]; mins[2] = (float)leaf->mins[2];
		maxs[0] = (float)leaf->maxs[0]; maxs[1] = (float)leaf->maxs[1]; maxs[2] = (float)leaf->maxs[2];

		if ( !R_CullBoundsVsFrustum( mins, maxs, frustum ) ) {
			continue;
		}

		for ( j = 0; j < leaf->numLeafSurfaces; j++ ) {
			int surfIdx = tr_world.leafSurfaces[leaf->firstLeafSurface + j];
			if ( surfIdx >= 0 && surfIdx < s_numAllocated ) {
				s_visibleSurfaces[surfIdx] = qtrue;
			}
		}
	}
}

qboolean R_SurfaceIsVisible( int surfaceIndex ) {
	if ( !s_visibleSurfaces || surfaceIndex < 0 || surfaceIndex >= s_numAllocated ) {
		return qtrue; // no culling data yet -- default to visible
	}
	return s_visibleSurfaces[surfaceIndex];
}
