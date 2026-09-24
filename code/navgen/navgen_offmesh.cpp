// navgen_offmesh.cpp -- proposes bridging connections for gaps Recast can't connect as ordinary floor, bounded by this game's jump physics (be_aas_reach.c) and validated against real geometry.

#include "navgen_offmesh.h"
#include "navgen_types.h"

#include "Recast.h"
#include "RecastAlloc.h"
#include "DetourNavMesh.h"
#include "DetourNavMeshBuilder.h"
#include "DetourCommon.h"
#include "DetourTileCache.h"
#include "DetourTileCacheBuilder.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <unordered_map>
#include <algorithm>

// sv_gravity=800, sv_jumpvel=270 (be_aas_move.c): 0.5*g*(v/g)^2.
static const float NAVGEN_MAX_JUMP_UP = 46.0f;
// MAX_JUMPFALLHEIGHT (be_aas_reach.c) - how far a jump may fall and still be controllable.
static const float NAVGEN_MAX_JUMP_DOWN = 450.0f;
// well under AAS_MaxJumpDistance(270)~=447: a generous cap with no ballistic check bridged straight across open water.
static const float NAVGEN_MAX_JUMP_HORIZ = 96.0f;
// AAS_AreaJumpReachable's own "too close to bother" cutoff (be_aas_reach.c: bestdist > 4).
static const float NAVGEN_MIN_JUMP_DIST = 4.0f;
// lift above each endpoint for the obstruction test, clearing the floor/ramp surface itself.
static const float NAVGEN_TRACE_LIFT = 14.0f;

struct NavGenOffMeshCompressor : public dtTileCacheCompressor {
	virtual int maxCompressedSize( const int bufferSize ) { return bufferSize; }
	virtual dtStatus compress( const unsigned char *buffer, const int bufferSize,
							   unsigned char *compressed, const int maxCompressedSize, int *compressedSize ) {
		if ( bufferSize > maxCompressedSize ) { return DT_FAILURE; }
		memcpy( compressed, buffer, bufferSize );
		*compressedSize = bufferSize;
		return DT_SUCCESS;
	}
	virtual dtStatus decompress( const unsigned char *compressed, const int compressedSize,
								 unsigned char *buffer, const int maxBufferSize, int *bufferSize ) {
		if ( compressedSize > maxBufferSize ) { return DT_FAILURE; }
		memcpy( buffer, compressed, compressedSize );
		*bufferSize = compressedSize;
		return DT_SUCCESS;
	}
};

struct NavGenOffMeshMeshProcess : public dtTileCacheMeshProcess {
	virtual void process( dtNavMeshCreateParams *params, unsigned char *polyAreas, unsigned short *polyFlags ) {
		for ( int i = 0; i < params->polyCount; i++ ) {
			if ( polyAreas[i] != DT_TILECACHE_NULL_AREA ) {
				polyFlags[i] = 1;
			}
		}
	}
};

struct NavGenUnionFind {
	std::vector<int> parent, rank_;
	void init( int n ) { parent.resize( n ); rank_.assign( n, 0 ); for ( int i = 0; i < n; i++ ) { parent[i] = i; } }
	int find( int x ) { while ( parent[x] != x ) { parent[x] = parent[parent[x]]; x = parent[x]; } return x; }
	void unite( int a, int b ) {
		a = find( a ); b = find( b );
		if ( a == b ) { return; }
		if ( rank_[a] < rank_[b] ) { std::swap( a, b ); }
		parent[b] = a;
		if ( rank_[a] == rank_[b] ) { rank_[a]++; }
	}
};

struct NavGenBorderEdge {
	float va[3], vb[3]; // full segment, navmesh (Y-up) space
	int comp;
};

// closest points between segment (p1,q1) and (p2,q2), written to c1/c2 (Ericson's ClosestPtSegmentSegment).
static void ClosestPtSegmentSegment( const float *p1, const float *q1, const float *p2, const float *q2,
									  float *c1, float *c2 ) {
	float d1[3], d2[3], r[3];
	VectorSubtract( q1, p1, d1 );
	VectorSubtract( q2, p2, d2 );
	VectorSubtract( p1, p2, r );
	float a = DotProduct( d1, d1 );
	float e = DotProduct( d2, d2 );
	float f = DotProduct( d2, r );
	float s, t;
	const float EPS = 1e-8f;

	if ( a <= EPS && e <= EPS ) {
		s = t = 0.0f;
	} else if ( a <= EPS ) {
		s = 0.0f;
		t = rcClamp( f / e, 0.0f, 1.0f );
	} else {
		float c = DotProduct( d1, r );
		if ( e <= EPS ) {
			t = 0.0f;
			s = rcClamp( -c / a, 0.0f, 1.0f );
		} else {
			float b = DotProduct( d1, d2 );
			float denom = a * e - b * b;
			s = ( denom != 0.0f ) ? rcClamp( ( b * f - c * e ) / denom, 0.0f, 1.0f ) : 0.0f;
			t = ( b * s + f ) / e;
			if ( t < 0.0f ) {
				t = 0.0f;
				s = rcClamp( -c / a, 0.0f, 1.0f );
			} else if ( t > 1.0f ) {
				t = 1.0f;
				s = rcClamp( ( b - c ) / a, 0.0f, 1.0f );
			}
		}
	}
	VectorMA( p1, s, d1, c1 );
	VectorMA( p2, t, d2, c2 );
}

// standard Moller-Trumbore, bounded to the segment (t clamped off 0/1 so a touch at a lifted endpoint doesn't count).
static bool SegmentHitsTriangle( const float *a, const float *b, const float *v0, const float *v1, const float *v2 ) {
	float dir[3], e1[3], e2[3], pvec[3], tvec[3], qvec[3];
	VectorSubtract( b, a, dir );
	VectorSubtract( v1, v0, e1 );
	VectorSubtract( v2, v0, e2 );
	CrossProduct( dir, e2, pvec );
	float det = DotProduct( e1, pvec );
	if ( det > -1e-7f && det < 1e-7f ) {
		return false; // parallel to the triangle's plane
	}
	float invDet = 1.0f / det;
	VectorSubtract( a, v0, tvec );
	float u = DotProduct( tvec, pvec ) * invDet;
	if ( u < 0.0f || u > 1.0f ) {
		return false;
	}
	CrossProduct( tvec, e1, qvec );
	float v = DotProduct( dir, qvec ) * invDet;
	if ( v < 0.0f || u + v > 1.0f ) {
		return false;
	}
	float t = DotProduct( e2, qvec ) * invDet;
	return t > 0.01f && t < 0.99f;
}

// true if segment a-b (navmesh space, not lifted) is free of solid geometry; brute-force with a per-triangle AABB reject.
static bool SegmentClearRaw( const float *a, const float *b, const navGeom_t *geom ) {
	float segLo[3] = { rcMin( a[0], b[0] ), rcMin( a[1], b[1] ), rcMin( a[2], b[2] ) };
	float segHi[3] = { rcMax( a[0], b[0] ), rcMax( a[1], b[1] ), rcMax( a[2], b[2] ) };

	for ( int t = 0; t < geom->numTris; t++ ) {
		const float *v0 = &geom->verts[geom->tris[t * 3 + 0] * 3];
		const float *v1 = &geom->verts[geom->tris[t * 3 + 1] * 3];
		const float *v2 = &geom->verts[geom->tris[t * 3 + 2] * 3];

		float triLo[3], triHi[3];
		for ( int k = 0; k < 3; k++ ) {
			triLo[k] = rcMin( v0[k], rcMin( v1[k], v2[k] ) );
			triHi[k] = rcMax( v0[k], rcMax( v1[k], v2[k] ) );
			if ( triHi[k] < segLo[k] || triLo[k] > segHi[k] ) {
				goto nextTri; // no AABB overlap on this axis - can't intersect
			}
		}
		if ( SegmentHitsTriangle( a, b, v0, v1, v2 ) ) {
			return false;
		}
nextTri:;
	}
	return true;
}

// true if a character could get from a to b: up-and-over instead of one diagonal, which would clip the ledge's own wall.
static bool JumpPathClear( const float *aIn, const float *bIn, const navGeom_t *geom ) {
	const float *hi = ( aIn[1] >= bIn[1] ) ? aIn : bIn;
	const float *lo = ( aIn[1] >= bIn[1] ) ? bIn : aIn;

	vec3_t hiLift, loLift, crossAtLo;
	VectorCopy( hi, hiLift ); hiLift[1] += NAVGEN_TRACE_LIFT;
	VectorCopy( lo, loLift ); loLift[1] += NAVGEN_TRACE_LIFT;
	crossAtLo[0] = lo[0]; crossAtLo[1] = hiLift[1]; crossAtLo[2] = lo[2];

	// up (sanity - should trivially pass unless something overhangs the takeoff point)
	if ( !SegmentClearRaw( hi, hiLift, geom ) ) { return false; }
	// across, at the higher point's own height - clears whatever's below it
	if ( !SegmentClearRaw( hiLift, crossAtLo, geom ) ) { return false; }
	// down to the landing point
	if ( !SegmentClearRaw( crossAtLo, loLift, geom ) ) { return false; }
	if ( !SegmentClearRaw( loLift, lo, geom ) ) { return false; }
	return true;
}

/*
====================
NavGen_FindOffMeshConns
====================
*/
void NavGen_FindOffMeshConns( const NavCacheHeader &header, const std::vector<NavGenTileBlob> &tiles,
							   const navGeom_t *geom, std::vector<NavCacheOffMeshConn> &outConns ) {
	if ( tiles.empty() ) {
		return;
	}

	dtTileCacheParams tcparams;
	memset( &tcparams, 0, sizeof( tcparams ) );
	tcparams.orig[0] = header.orig[0]; tcparams.orig[1] = header.orig[1]; tcparams.orig[2] = header.orig[2];
	tcparams.cs = header.cellSize;
	tcparams.ch = header.cellHeight;
	tcparams.width = header.tileSize;
	tcparams.height = header.tileSize;
	tcparams.walkableHeight = header.walkableHeight;
	tcparams.walkableRadius = header.walkableRadius;
	tcparams.walkableClimb = header.walkableClimb;
	tcparams.maxSimplificationError = header.maxSimplificationError;
	tcparams.maxTiles = (int)tiles.size();
	tcparams.maxObstacles = 1;

	NavGenOffMeshCompressor comp;
	NavGenOffMeshMeshProcess meshProcess;

	dtTileCache *cache = new dtTileCache();
	dtTileCacheAlloc alloc;
	if ( dtStatusFailed( cache->init( &tcparams, &alloc, &comp, &meshProcess ) ) ) {
		delete cache;
		return;
	}

	dtNavMeshParams meshParams;
	memset( &meshParams, 0, sizeof( meshParams ) );
	meshParams.orig[0] = header.orig[0]; meshParams.orig[1] = header.orig[1]; meshParams.orig[2] = header.orig[2];
	meshParams.tileWidth = header.tileSize * header.cellSize;
	meshParams.tileHeight = header.tileSize * header.cellSize;
	int tileBits = rcMin( (int)dtIlog2( dtNextPow2( (unsigned int)tcparams.maxTiles ) ), 14 );
	int polyBits = 22 - tileBits;
	meshParams.maxTiles = 1 << tileBits;
	meshParams.maxPolys = 1 << polyBits;

	dtNavMesh *mesh = dtAllocNavMesh();
	if ( dtStatusFailed( mesh->init( &meshParams ) ) ) {
		dtFreeNavMesh( mesh );
		delete cache;
		return;
	}

	int lastTx = -1, lastTy = -1;
	bool haveLast = false;
	for ( size_t i = 0; i < tiles.size(); i++ ) {
		const NavGenTileBlob &entry = tiles[i];

		unsigned char *tileData = (unsigned char *)dtAlloc( entry.dataSize, DT_ALLOC_PERM );
		memcpy( tileData, entry.data, entry.dataSize );

		dtCompressedTileRef ref;
		if ( dtStatusFailed( cache->addTile( tileData, entry.dataSize, DT_COMPRESSEDTILE_FREE_DATA, &ref ) ) ) {
			dtFree( tileData );
			continue;
		}

		if ( haveLast && ( entry.tx != lastTx || entry.ty != lastTy ) ) {
			cache->buildNavMeshTilesAt( lastTx, lastTy, mesh );
		}
		lastTx = entry.tx;
		lastTy = entry.ty;
		haveLast = true;
	}
	if ( haveLast ) {
		cache->buildNavMeshTilesAt( lastTx, lastTy, mesh );
	}

	// assign a dense index to every poly so union-find can use a flat array.
	std::unordered_map<dtPolyRef, int> refToIndex;
	std::vector<dtPolyRef> indexToRef;
	const dtNavMesh *constMesh = mesh;
	for ( int i = 0; i < constMesh->getMaxTiles(); i++ ) {
		const dtMeshTile *tile = constMesh->getTile( i );
		if ( !tile || !tile->header ) { continue; }
		dtPolyRef base = constMesh->getPolyRefBase( tile );
		for ( int j = 0; j < tile->header->polyCount; j++ ) {
			dtPolyRef ref = base | (dtPolyRef)j;
			refToIndex[ref] = (int)indexToRef.size();
			indexToRef.push_back( ref );
		}
	}

	NavGenUnionFind uf;
	uf.init( (int)indexToRef.size() );
	for ( int i = 0; i < constMesh->getMaxTiles(); i++ ) {
		const dtMeshTile *tile = constMesh->getTile( i );
		if ( !tile || !tile->header ) { continue; }
		dtPolyRef base = constMesh->getPolyRefBase( tile );
		for ( int j = 0; j < tile->header->polyCount; j++ ) {
			int myIdx = refToIndex[base | (dtPolyRef)j];
			const dtPoly *poly = &tile->polys[j];
			unsigned int linkIdx = poly->firstLink;
			while ( linkIdx != DT_NULL_LINK ) {
				const dtLink &link = tile->links[linkIdx];
				std::unordered_map<dtPolyRef, int>::iterator it = refToIndex.find( link.ref );
				if ( it != refToIndex.end() ) {
					uf.unite( myIdx, it->second );
				}
				linkIdx = link.next;
			}
		}
	}

	// border edges: edges with no neighbor at all (not even an unresolved external one).
	std::vector<NavGenBorderEdge> edges;
	for ( int i = 0; i < constMesh->getMaxTiles(); i++ ) {
		const dtMeshTile *tile = constMesh->getTile( i );
		if ( !tile || !tile->header ) { continue; }
		dtPolyRef base = constMesh->getPolyRefBase( tile );
		for ( int j = 0; j < tile->header->polyCount; j++ ) {
			const dtPoly *poly = &tile->polys[j];
			if ( poly->getType() != DT_POLYTYPE_GROUND ) { continue; }
			int comp = uf.find( refToIndex[base | (dtPolyRef)j] );
			for ( int k = 0; k < (int)poly->vertCount; k++ ) {
				if ( poly->neis[k] != 0 ) { continue; }
				const float *va = &tile->verts[poly->verts[k] * 3];
				const float *vb = &tile->verts[poly->verts[( k + 1 ) % poly->vertCount] * 3];
				NavGenBorderEdge edge;
				VectorCopy( va, edge.va );
				VectorCopy( vb, edge.vb );
				edge.comp = comp;
				edges.push_back( edge );
			}
		}
	}

	// NAVGEN_DEBUG_POINT=x,y,z (quake space, e.g. from in-game /where): prints nearby candidates and why each was accepted/rejected.
	const char *debugPointStr = getenv( "NAVGEN_DEBUG_POINT" );
	if ( debugPointStr && debugPointStr[0] ) {
		float qx, qy, qz;
		if ( sscanf( debugPointStr, "%f,%f,%f", &qx, &qy, &qz ) == 3 ) {
			vec3_t queryNav = { qx, qz, qy }; // quake (x,y,z) -> navmesh (x,z,y)
			printf( "\n[NAVGEN_DEBUG_POINT] quake (%.1f %.1f %.1f) -> navmesh (%.1f %.1f %.1f)\n",
					qx, qy, qz, queryNav[0], queryNav[1], queryNav[2] );

			std::vector<std::pair<float, size_t> > byDist;
			for ( size_t i = 0; i < edges.size(); i++ ) {
				vec3_t mid = { ( edges[i].va[0] + edges[i].vb[0] ) * 0.5f,
								( edges[i].va[1] + edges[i].vb[1] ) * 0.5f,
								( edges[i].va[2] + edges[i].vb[2] ) * 0.5f };
				vec3_t d; VectorSubtract( mid, queryNav, d );
				byDist.push_back( std::make_pair( DotProduct( d, d ), i ) );
			}
			std::sort( byDist.begin(), byDist.end() );

			for ( int n = 0; n < 3 && n < (int)byDist.size(); n++ ) {
				size_t i = byDist[n].second;
				printf( "\n-- nearest edge #%d: idx=%zu comp=%d dist-to-query=%.1f\n",
						n, i, edges[i].comp, sqrtf( byDist[n].first ) );
				printf( "   va=(%.1f %.1f %.1f) vb=(%.1f %.1f %.1f)\n",
						edges[i].va[0], edges[i].va[1], edges[i].va[2],
						edges[i].vb[0], edges[i].vb[1], edges[i].vb[2] );

				std::vector<std::pair<float, size_t> > others;
				for ( size_t j = 0; j < edges.size(); j++ ) {
					if ( j == i || edges[j].comp == edges[i].comp ) { continue; }
					float pi[3], pj[3];
					ClosestPtSegmentSegment( edges[i].va, edges[i].vb, edges[j].va, edges[j].vb, pi, pj );
					vec3_t d; VectorSubtract( pi, pj, d );
					others.push_back( std::make_pair( DotProduct( d, d ), j ) );
				}
				std::sort( others.begin(), others.end() );

				int shown = 0;
				for ( size_t oi = 0; oi < others.size() && shown < 6; oi++ ) {
					size_t j = others[oi].second;
					float pi[3], pj[3];
					ClosestPtSegmentSegment( edges[i].va, edges[i].vb, edges[j].va, edges[j].vb, pi, pj );
					float dx = pi[0] - pj[0], dz = pi[2] - pj[2], dy = pi[1] - pj[1];
					float horiz = sqrtf( dx * dx + dz * dz );
					bool horizOk = horiz >= NAVGEN_MIN_JUMP_DIST && horiz <= NAVGEN_MAX_JUMP_HORIZ;
					bool heightOk = dy <= NAVGEN_MAX_JUMP_UP && dy >= -NAVGEN_MAX_JUMP_DOWN;
					bool clear = ( horizOk && heightOk ) ? JumpPathClear( pi, pj, geom ) : false;
					printf( "   candidate comp=%d horiz=%.1f (%s) dy=%.1f (%s) %s\n",
							edges[j].comp, horiz, horizOk ? "ok" : "OUT OF RANGE",
							dy, heightOk ? "ok" : "OUT OF RANGE",
							!( horizOk && heightOk ) ? "-- skipped" : ( clear ? "-- CLEAR (usable)" : "-- BLOCKED by geometry" ) );
					shown++;
				}
				if ( others.empty() ) {
					printf( "   (no other-component edges exist anywhere in this class's mesh)\n" );
				}
			}
			printf( "[NAVGEN_DEBUG_POINT] done\n\n" );
		}
	}

	// for each edge, take the nearest in-range candidate that passes JumpPathClear, not just the nearest overall.
	struct NavGenCandidate {
		float dist;
		int other;
		float ptSelf[3], ptOther[3];
		bool operator<( const NavGenCandidate &o ) const { return dist < o.dist; }
	};
	std::vector<std::pair<size_t, size_t> > matches; // deduped (i<j) pairs
	std::vector<float> matchPtA, matchPtB; // precise connection points, parallel to matches
	{
		std::vector<std::vector<NavGenCandidate> > candidates( edges.size() );
		for ( size_t i = 0; i < edges.size(); i++ ) {
			for ( size_t j = i + 1; j < edges.size(); j++ ) {
				if ( edges[i].comp == edges[j].comp ) { continue; }
				float pi[3], pj[3];
				ClosestPtSegmentSegment( edges[i].va, edges[i].vb, edges[j].va, edges[j].vb, pi, pj );
				float dx = pi[0] - pj[0];
				float dz = pi[2] - pj[2];
				float dy = pi[1] - pj[1]; // navmesh Y = up
				float horiz = sqrtf( dx * dx + dz * dz );
				if ( horiz < NAVGEN_MIN_JUMP_DIST || horiz > NAVGEN_MAX_JUMP_HORIZ ) { continue; }
				if ( dy > NAVGEN_MAX_JUMP_UP || dy < -NAVGEN_MAX_JUMP_DOWN ) { continue; }
				float dist3 = sqrtf( horiz * horiz + dy * dy );

				NavGenCandidate ci; ci.dist = dist3; ci.other = (int)j;
				VectorCopy( pi, ci.ptSelf ); VectorCopy( pj, ci.ptOther );
				candidates[i].push_back( ci );

				NavGenCandidate cj; cj.dist = dist3; cj.other = (int)i;
				VectorCopy( pj, cj.ptSelf ); VectorCopy( pi, cj.ptOther );
				candidates[j].push_back( cj );
			}
		}

		std::vector<int> bestOther( edges.size(), -1 );
		std::vector<std::pair<vec3_t, vec3_t> > bestPts( edges.size() );
		for ( size_t i = 0; i < edges.size(); i++ ) {
			std::sort( candidates[i].begin(), candidates[i].end() );
			for ( size_t c = 0; c < candidates[i].size(); c++ ) {
				const NavGenCandidate &cand = candidates[i][c];
				if ( JumpPathClear( cand.ptSelf, cand.ptOther, geom ) ) {
					bestOther[i] = cand.other;
					VectorCopy( cand.ptSelf, bestPts[i].first );
					VectorCopy( cand.ptOther, bestPts[i].second );
					break;
				}
			}
		}

		std::unordered_map<long long, bool> seen;
		for ( size_t i = 0; i < edges.size(); i++ ) {
			if ( bestOther[i] < 0 ) { continue; }
			size_t j = (size_t)bestOther[i];
			size_t lo = i < j ? i : j, hi = i < j ? j : i;
			long long key = ( (long long)lo << 32 ) | (long long)hi;
			if ( seen.find( key ) != seen.end() ) { continue; }
			seen[key] = true;
			matches.push_back( std::make_pair( lo, hi ) );
			// bestPts[i] is (point on i, point on j) regardless of i<j ordering.
			matchPtA.push_back( bestPts[i].first[0] ); matchPtA.push_back( bestPts[i].first[1] ); matchPtA.push_back( bestPts[i].first[2] );
			matchPtB.push_back( bestPts[i].second[0] ); matchPtB.push_back( bestPts[i].second[1] ); matchPtB.push_back( bestPts[i].second[2] );
		}
	}

	for ( size_t m = 0; m < matches.size(); m++ ) {
		NavCacheOffMeshConn conn;
		VectorCopy( &matchPtA[m * 3], conn.startPos );
		VectorCopy( &matchPtB[m * 3], conn.endPos );
		conn.radius = header.walkableRadius;
		conn.bidir = 1;
		outConns.push_back( conn );
	}

	dtFreeNavMesh( mesh );
	delete cache;
}
