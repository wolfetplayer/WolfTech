// nav_debug.cpp -- live in-game navmesh/path visualization via the engine's bot debug-polygon channel (sv_bot.c).

#include "nav_local.h"
#include "nav_public.h"

#include <cstring>
#include <vector>
#include <unordered_set>
#include <algorithm>

extern "C" {
#include "../../qcommon/q_shared.h"
#include "../../qcommon/qcommon.h"
}

// from server.h, not included here: it has a parameter named `or`, a reserved word in C++.
extern "C" int BotImport_DebugPolygonCreate( int color, int numPoints, vec3_t *points );
extern "C" void BotImport_DebugPolygonDelete( int id );

// matches botai.h's LINECOLOR_* (not included, to avoid pulling in all of botlib.h for four integers).
#define NAVDBG_RED      1
#define NAVDBG_GREEN    2
#define NAVDBG_YELLOW   4

static void SwapYZ( const float *in, float *out ) {
	float y = in[1];
	out[0] = in[0];
	out[1] = in[2];
	out[2] = y;
}

// caps keep mesh + path slots well under sv_bot.c's shared 128-slot MAX_DEBUGPOLYS budget.
static const int NAVDBG_MAX_MESH_POLYS = 64;
static const int NAVDBG_PATH_SLOTS = 4;
static const int NAVDBG_MAX_SEGS_PER_SLOT = 12;

static int navDbgMeshIds[NAVDBG_MAX_MESH_POLYS];
static int navDbgMeshCount = 0;
static int navDbgPathIds[NAVDBG_PATH_SLOTS][NAVDBG_MAX_SEGS_PER_SLOT];
static int navDbgPathCount[NAVDBG_PATH_SLOTS];

static void ClearMeshPolys( void ) {
	for ( int i = 0; i < navDbgMeshCount; i++ ) {
		BotImport_DebugPolygonDelete( navDbgMeshIds[i] );
	}
	navDbgMeshCount = 0;
}

static void ClearPathSlot( int slot ) {
	for ( int i = 0; i < navDbgPathCount[slot]; i++ ) {
		BotImport_DebugPolygonDelete( navDbgPathIds[slot][i] );
	}
	navDbgPathCount[slot] = 0;
}

static void ClearAllPathSlots( void ) {
	for ( int s = 0; s < NAVDBG_PATH_SLOTS; s++ ) {
		ClearPathSlot( s );
	}
}

// same "thin quad" trick as g_utils.c's DebugLine - there's no line primitive, only filled polygons.
static void ShowDebugLineSeg( const float *start, const float *end, int color, int *idArray, int *idCount, int maxIds ) {
	if ( *idCount >= maxIds ) {
		return;
	}
	vec3_t points[4], dir, cross, up = { 0, 0, 1 };
	float dot;

	VectorCopy( start, points[0] );
	VectorCopy( start, points[1] );
	VectorCopy( end, points[2] );
	VectorCopy( end, points[3] );

	VectorSubtract( end, start, dir );
	VectorNormalize( dir );
	dot = DotProduct( dir, up );
	if ( dot > 0.99 || dot < -0.99 ) {
		VectorSet( cross, 1, 0, 0 );
	} else {
		CrossProduct( dir, up, cross );
	}
	VectorNormalize( cross );

	VectorMA( points[0], 2, cross, points[0] );
	VectorMA( points[1], -2, cross, points[1] );
	VectorMA( points[2], -2, cross, points[2] );
	VectorMA( points[3], 2, cross, points[3] );

	int id = BotImport_DebugPolygonCreate( color, 4, points );
	if ( id ) {
		idArray[( *idCount )++] = id;
	}
}

/*
====================
Nav_DebugShowNearby

Draws every navmesh poly within radius of origin (quake space), green if
reachable from origin's own poly (ordinary walkable adjacency + off-mesh
links), red if it's nearby but NOT reachable - i.e. the exact boundary of a
disconnected pocket, live, in the actual loaded mesh.
====================
*/
void Nav_DebugShowNearby( const float *origin, float radius ) {
	ClearMeshPolys();

	NavData_t *data = Nav_CurrentData();
	if ( !data ) {
		return;
	}

	vec3_t navOrigin;
	SwapYZ( origin, navOrigin );

	dtQueryFilter filter;
	const float ext[3] = { 96, 160, 96 };
	dtPolyRef seedRef;
	float seedNearest[3];
	if ( dtStatusFailed( data->query->findNearestPoly( navOrigin, ext, &filter, &seedRef, seedNearest ) ) || !seedRef ) {
		return;
	}

	// BFS the whole reachable set - a radius cutoff could wrongly call something reachable "too far" too soon.
	std::unordered_set<dtPolyRef> reachable;
	{
		std::vector<dtPolyRef> stack;
		reachable.insert( seedRef );
		stack.push_back( seedRef );
		while ( !stack.empty() ) {
			dtPolyRef ref = stack.back();
			stack.pop_back();
			const dtMeshTile *tile = 0;
			const dtPoly *poly = 0;
			if ( dtStatusFailed( data->mesh->getTileAndPolyByRef( ref, &tile, &poly ) ) ) {
				continue;
			}
			unsigned int linkIdx = poly->firstLink;
			while ( linkIdx != DT_NULL_LINK ) {
				const dtLink &link = tile->links[linkIdx];
				if ( reachable.find( link.ref ) == reachable.end() ) {
					reachable.insert( link.ref );
					stack.push_back( link.ref );
				}
				linkIdx = link.next;
			}
		}
	}

	// gather nearby polys, nearest first, capped at NAVDBG_MAX_MESH_POLYS.
	struct Candidate { dtPolyRef ref; float distSq; };
	std::vector<Candidate> candidates;
	const dtNavMesh *constMesh = data->mesh;
	for ( int i = 0; i < constMesh->getMaxTiles(); i++ ) {
		const dtMeshTile *tile = constMesh->getTile( i );
		if ( !tile || !tile->header ) {
			continue;
		}
		dtPolyRef base = constMesh->getPolyRefBase( tile );
		for ( int j = 0; j < tile->header->polyCount; j++ ) {
			const dtPoly *poly = &tile->polys[j];
			float cx = 0, cy = 0, cz = 0;
			for ( int k = 0; k < (int)poly->vertCount; k++ ) {
				const float *v = &tile->verts[poly->verts[k] * 3];
				cx += v[0]; cy += v[1]; cz += v[2];
			}
			cx /= poly->vertCount; cy /= poly->vertCount; cz /= poly->vertCount;
			float dx = cx - navOrigin[0], dy = cy - navOrigin[1], dz = cz - navOrigin[2];
			float distSq = dx * dx + dy * dy + dz * dz;
			if ( distSq <= radius * radius ) {
				Candidate c; c.ref = base | (dtPolyRef)j; c.distSq = distSq;
				candidates.push_back( c );
			}
		}
	}
	std::sort( candidates.begin(), candidates.end(), []( const Candidate &a, const Candidate &b ) { return a.distSq < b.distSq; } );

	for ( size_t i = 0; i < candidates.size() && navDbgMeshCount < NAVDBG_MAX_MESH_POLYS; i++ ) {
		const dtMeshTile *tile = 0;
		const dtPoly *poly = 0;
		if ( dtStatusFailed( data->mesh->getTileAndPolyByRef( candidates[i].ref, &tile, &poly ) ) ) {
			continue;
		}
		int color = ( reachable.find( candidates[i].ref ) != reachable.end() ) ? NAVDBG_GREEN : NAVDBG_RED;
		vec3_t quakePoints[6];
		int n = poly->vertCount > 6 ? 6 : (int)poly->vertCount;
		for ( int k = 0; k < n; k++ ) {
			const float *v = &tile->verts[poly->verts[k] * 3];
			vec3_t lifted = { v[0], v[1] + 1.0f, v[2] }; // 1 unit up so it doesn't z-fight the floor
			SwapYZ( lifted, quakePoints[k] );
		}
		int id = BotImport_DebugPolygonCreate( color, n, quakePoints );
		if ( id ) {
			navDbgMeshIds[navDbgMeshCount++] = id;
		}
	}
}

/*
==================
Nav_DebugShowPath

Draws the same straight path Nav_MoveToGoal would steer along, as connected
yellow line segments from start to goal (quake space). Empty/no-op if there's
no path - the absence of a line is itself the useful signal. slot selects one
of NAVDBG_PATH_SLOTS independent buffers (callers pass e.g. entityNum % slots)
so multiple AI can each keep their own path on screen at once.
==================
*/
void Nav_DebugShowPath( const float *start, const float *goal, int slot ) {
	if ( slot < 0 || slot >= NAVDBG_PATH_SLOTS ) {
		return;
	}
	ClearPathSlot( slot );

	NavData_t *data = Nav_CurrentData();
	if ( !data ) {
		return;
	}

	vec3_t navStart, navGoal;
	SwapYZ( start, navStart );
	SwapYZ( goal, navGoal );

	dtQueryFilter filter;
	const float ext[3] = { 96, 160, 96 };
	dtPolyRef startRef, endRef;
	float startNearest[3], endNearest[3];
	if ( dtStatusFailed( data->query->findNearestPoly( navStart, ext, &filter, &startRef, startNearest ) ) || !startRef ) {
		return;
	}
	if ( dtStatusFailed( data->query->findNearestPoly( navGoal, ext, &filter, &endRef, endNearest ) ) || !endRef ) {
		return;
	}

	dtPolyRef path[512];
	int pathCount = 0;
	if ( dtStatusFailed( data->query->findPath( startRef, endRef, startNearest, endNearest, &filter, path, &pathCount, 512 ) ) || pathCount == 0 ) {
		return;
	}
	if ( path[pathCount - 1] != endRef ) {
		return; // disconnected - draw nothing, same as Nav_MoveToGoal's own failure case
	}

	float straight[32 * 3];
	unsigned char straightFlags[32];
	dtPolyRef straightRefs[32];
	int straightCount = 0;
	if ( dtStatusFailed( data->query->findStraightPath( startNearest, endNearest, path, pathCount,
														 straight, straightFlags, straightRefs, &straightCount, 32 ) ) ) {
		return;
	}

	for ( int i = 1; i < straightCount; i++ ) {
		vec3_t qa, qb, lifted;
		lifted[0] = straight[( i - 1 ) * 3 + 0]; lifted[1] = straight[( i - 1 ) * 3 + 1] + 1.0f; lifted[2] = straight[( i - 1 ) * 3 + 2];
		SwapYZ( lifted, qa );
		lifted[0] = straight[i * 3 + 0]; lifted[1] = straight[i * 3 + 1] + 1.0f; lifted[2] = straight[i * 3 + 2];
		SwapYZ( lifted, qb );
		int color = ( straightFlags[i - 1] & DT_STRAIGHTPATH_OFFMESH_CONNECTION ) ? NAVDBG_RED : NAVDBG_YELLOW;
		ShowDebugLineSeg( qa, qb, color, navDbgPathIds[slot], &navDbgPathCount[slot], NAVDBG_MAX_SEGS_PER_SLOT );
	}
}

/*
==================
Nav_DebugClear
==================
*/
void Nav_DebugClear( void ) {
	ClearMeshPolys();
	ClearAllPathSlots();
}
