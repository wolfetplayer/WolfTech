// nav_load.cpp -- loads navgen's .navcache files into queryable dtNavMesh/dtTileCache instances.

#include "nav_local.h"
#include "nav_public.h"
#include "../../navgen/navcache_format.h"
#include "../../navgen/navgen_geom.h"
#include "../../navgen/navgen_bake.h"

#include <cstdio>
#include <cstring>
#include <vector>

extern "C" {
#include "../../qcommon/q_shared.h"
#include "../../qcommon/qcommon.h"
}

/*
=======================
NavLoadPassthroughCompressor

Same rationale as navgen's: an already-baked tile has nothing left to compress.
=======================
*/
struct NavLoadPassthroughCompressor : public dtTileCacheCompressor {
	virtual int maxCompressedSize( const int bufferSize ) { return bufferSize; }

	virtual dtStatus compress( const unsigned char *buffer, const int bufferSize,
							   unsigned char *compressed, const int maxCompressedSize, int *compressedSize ) {
		if ( bufferSize > maxCompressedSize ) {
			return DT_FAILURE;
		}
		memcpy( compressed, buffer, bufferSize );
		*compressedSize = bufferSize;
		return DT_SUCCESS;
	}

	virtual dtStatus decompress( const unsigned char *compressed, const int compressedSize,
								 unsigned char *buffer, const int maxBufferSize, int *bufferSize ) {
		if ( compressedSize > maxBufferSize ) {
			return DT_FAILURE;
		}
		memcpy( buffer, compressed, compressedSize );
		*bufferSize = compressedSize;
		return DT_SUCCESS;
	}
};

/*
===============
NavMeshProcess

Every walkable poly gets a nonzero flag so Detour's default query filter
accepts it. Off-mesh links (navgen_offmesh.cpp's bridged jump/step-across
connections) are baked in here: each tile build gets just the connections
whose midpoint falls inside that tile's bounds - registering a connection in
one tile is enough, since dtNavMesh::addTile resolves the far endpoint against
that tile and its direct neighbors (see connectExtOffMeshLinks), and navgen
caps connection length well inside one tile's footprint.
===============
*/
struct NavMeshProcess : public dtTileCacheMeshProcess {
	std::vector<NavCacheOffMeshConn> conns;

	// scratch for the tile currently building; process() points offMeshCon* here, outlives the call.
	std::vector<float> scratchVerts;
	std::vector<float> scratchRad;
	std::vector<unsigned short> scratchFlags;
	std::vector<unsigned char> scratchAreas;
	std::vector<unsigned char> scratchDir;
	std::vector<unsigned int> scratchUserID;

	virtual void process( dtNavMeshCreateParams *params, unsigned char *polyAreas, unsigned short *polyFlags ) {
		for ( int i = 0; i < params->polyCount; i++ ) {
			if ( polyAreas[i] != DT_TILECACHE_NULL_AREA ) {
				polyFlags[i] = 1;
			}
		}

		scratchVerts.clear(); scratchRad.clear(); scratchFlags.clear();
		scratchAreas.clear(); scratchDir.clear(); scratchUserID.clear();

		for ( size_t i = 0; i < conns.size(); i++ ) {
			const NavCacheOffMeshConn &c = conns[i];
			float midX = ( c.startPos[0] + c.endPos[0] ) * 0.5f;
			float midY = ( c.startPos[1] + c.endPos[1] ) * 0.5f;
			float midZ = ( c.startPos[2] + c.endPos[2] ) * 0.5f;
			if ( midX < params->bmin[0] || midX > params->bmax[0] ||
				 midY < params->bmin[1] || midY > params->bmax[1] ||
				 midZ < params->bmin[2] || midZ > params->bmax[2] ) {
				continue;
			}
			scratchVerts.push_back( c.startPos[0] ); scratchVerts.push_back( c.startPos[1] ); scratchVerts.push_back( c.startPos[2] );
			scratchVerts.push_back( c.endPos[0] ); scratchVerts.push_back( c.endPos[1] ); scratchVerts.push_back( c.endPos[2] );
			scratchRad.push_back( c.radius );
			scratchFlags.push_back( 1 );
			scratchAreas.push_back( 0 );
			scratchDir.push_back( c.bidir ? DT_OFFMESH_CON_BIDIR : 0 );
			scratchUserID.push_back( (unsigned int)i );
		}

		if ( !scratchRad.empty() ) {
			params->offMeshConVerts = scratchVerts.data();
			params->offMeshConRad = scratchRad.data();
			params->offMeshConFlags = scratchFlags.data();
			params->offMeshConAreas = scratchAreas.data();
			params->offMeshConDir = scratchDir.data();
			params->offMeshConUserID = scratchUserID.data();
			params->offMeshConCount = (int)scratchRad.size();
		} else {
			params->offMeshConCount = 0;
		}
	}
};

/*
=================
Nav_AutoBakeClass

Bakes classIndex's navcache from the map's .bsp and writes it to qpath, in-process, on the calling
(main) thread. Only called when nav_autobake is set and the cached file is missing or stale - this
runs the full Recast pipeline synchronously, so it can stall a cold map load for a while on a map
that hasn't been baked yet.
=================
*/
static bool Nav_AutoBakeClass( const char *mapname, const navGenClass_t *cls, const char *qpath ) {
	char bspPath[MAX_QPATH];
	snprintf( bspPath, sizeof( bspPath ), "maps/%s.bsp", mapname );

	void *bspBuf = NULL;
	long bspLen = FS_ReadFile( bspPath, &bspBuf );
	if ( bspLen <= 0 || !bspBuf ) {
		Com_Printf( "Nav_AutoBakeClass: could not read %s\n", bspPath );
		return false;
	}

	navGeom_t geom;
	memset( &geom, 0, sizeof( geom ) );
	bool geomOk = NavGen_LoadGeometryFromMemory( (const unsigned char *)bspBuf, (int)bspLen, &geom ) != 0;
	FS_FreeFile( bspBuf );
	if ( !geomOk ) {
		Com_Printf( "Nav_AutoBakeClass: no walkable geometry extracted from %s\n", bspPath );
		return false;
	}

	Com_Printf( "Nav_AutoBakeClass: no cached navmesh for class %s, baking %s now (this can take a while)...\n", cls->name, bspPath );

	std::vector<unsigned char> bytes;
	bool bakeOk = NavGen_BakeClassToBuffer( &geom, cls, bytes ) != 0;
	NavGen_FreeGeometry( &geom );
	if ( !bakeOk || bytes.empty() ) {
		Com_Printf( "Nav_AutoBakeClass: bake failed for class %s\n", cls->name );
		return false;
	}

	FS_WriteFile( qpath, bytes.data(), (int)bytes.size() );
	Com_Printf( "Nav_AutoBakeClass: generated %s (%d bytes)\n", qpath, (int)bytes.size() );
	return true;
}

/*
============
Nav_LoadClass
============
*/
static bool Nav_LoadClass( const char *mapname, int classIndex ) {
	const navGenClass_t *cls = &navGenClasses[classIndex];
	NavData_t *data = &navData[classIndex];

	char qpath[MAX_QPATH];
	snprintf( qpath, sizeof( qpath ), "maps/nav/%s_%s.navcache", mapname, cls->name );

	void *buf = NULL;
	long len = FS_ReadFile( qpath, &buf );

	bool needsBake = len <= 0 || !buf;
	if ( !needsBake ) {
		NavCacheHeader peek;
		memcpy( &peek, buf, sizeof( peek ) );
		needsBake = ( peek.magic != NAVCACHE_MAGIC || peek.version != NAVCACHE_VERSION );
	}

	if ( needsBake && Cvar_VariableIntegerValue( "nav_autobake" ) ) {
		if ( buf ) {
			FS_FreeFile( buf );
			buf = NULL;
		}
		if ( Nav_AutoBakeClass( mapname, cls, qpath ) ) {
			len = FS_ReadFile( qpath, &buf );
		}
	}

	if ( len <= 0 || !buf ) {
		Com_Printf( "Nav_LoadClass: no navcache for %s (%s)\n", cls->name, qpath );
		return false;
	}

	const byte *cursor = (const byte *)buf;
	NavCacheHeader header;
	memcpy( &header, cursor, sizeof( header ) );
	cursor += sizeof( header );

	if ( header.magic != NAVCACHE_MAGIC || header.version != NAVCACHE_VERSION ) {
		Com_Printf( "Nav_LoadClass: %s has an unrecognized/stale navcache format, re-run navgen\n", qpath );
		FS_FreeFile( buf );
		return false;
	}

	// off-mesh links sit right after the header (see navgen_bake.cpp); NavMeshProcess needs them before any tile builds.
	std::vector<NavCacheOffMeshConn> offMeshConns( header.numOffMeshConns );
	if ( header.numOffMeshConns > 0 ) {
		memcpy( offMeshConns.data(), cursor, sizeof( NavCacheOffMeshConn ) * header.numOffMeshConns );
		cursor += sizeof( NavCacheOffMeshConn ) * header.numOffMeshConns;
	}

	dtTileCacheParams tcparams;
	memset( &tcparams, 0, sizeof( tcparams ) );
	VectorCopy( header.orig, tcparams.orig );
	tcparams.cs = header.cellSize;
	tcparams.ch = header.cellHeight;
	tcparams.width = header.tileSize;
	tcparams.height = header.tileSize;
	tcparams.walkableHeight = header.walkableHeight;
	tcparams.walkableRadius = header.walkableRadius;
	tcparams.walkableClimb = header.walkableClimb;
	tcparams.maxSimplificationError = header.maxSimplificationError;
	tcparams.maxTiles = header.numTiles > 0 ? header.numTiles : 1;
	tcparams.maxObstacles = 128; // doors/movers/constructibles, see nav_tilecache.cpp

	data->alloc = new dtTileCacheAlloc();
	data->compressor = new NavLoadPassthroughCompressor();
	NavMeshProcess *meshProcess = new NavMeshProcess();
	meshProcess->conns = offMeshConns;
	data->meshProcess = meshProcess;

	data->cache = new dtTileCache();
	if ( dtStatusFailed( data->cache->init( &tcparams, data->alloc, data->compressor, data->meshProcess ) ) ) {
		Com_Printf( "Nav_LoadClass: dtTileCache::init failed for %s\n", cls->name );
		FS_FreeFile( buf );
		return false;
	}

	dtNavMeshParams meshParams;
	memset( &meshParams, 0, sizeof( meshParams ) );
	VectorCopy( header.orig, meshParams.orig );
	meshParams.tileWidth = header.tileSize * header.cellSize;
	meshParams.tileHeight = header.tileSize * header.cellSize;
	int tileBits = rcMin( (int)dtIlog2( dtNextPow2( (unsigned int)tcparams.maxTiles ) ), 14 );
	int polyBits = 22 - tileBits;
	meshParams.maxTiles = 1 << tileBits;
	meshParams.maxPolys = 1 << polyBits;

	data->mesh = dtAllocNavMesh();
	if ( dtStatusFailed( data->mesh->init( &meshParams ) ) ) {
		Com_Printf( "Nav_LoadClass: dtNavMesh::init failed for %s\n", cls->name );
		FS_FreeFile( buf );
		return false;
	}

	int lastTx = -1, lastTy = -1;
	bool haveLast = false;
	int addOk = 0, addFail = 0, buildOk = 0, buildFail = 0;

	for ( int i = 0; i < header.numTiles; i++ ) {
		NavCacheTileEntry entry;
		memcpy( &entry, cursor, sizeof( entry ) );
		cursor += sizeof( entry );

		unsigned char *tileData = (unsigned char *)dtAlloc( entry.dataSize, DT_ALLOC_PERM );
		memcpy( tileData, cursor, entry.dataSize );
		cursor += entry.dataSize;

		dtCompressedTileRef ref;
		if ( dtStatusFailed( data->cache->addTile( tileData, entry.dataSize, DT_COMPRESSEDTILE_FREE_DATA, &ref ) ) ) {
			dtFree( tileData );
			addFail++;
			continue;
		}
		addOk++;

		if ( haveLast && ( entry.tx != lastTx || entry.ty != lastTy ) ) {
			if ( dtStatusFailed( data->cache->buildNavMeshTilesAt( lastTx, lastTy, data->mesh ) ) ) { buildFail++; } else { buildOk++; }
		}
		lastTx = entry.tx;
		lastTy = entry.ty;
		haveLast = true;
	}
	if ( haveLast ) {
		if ( dtStatusFailed( data->cache->buildNavMeshTilesAt( lastTx, lastTy, data->mesh ) ) ) { buildFail++; } else { buildOk++; }
	}

	FS_FreeFile( buf );

	{
		int totalPolys = 0;
		int totalLinks = 0, externalLinks = 0;
		const dtNavMesh *constMesh = data->mesh;
		for ( int i = 0; i < constMesh->getMaxTiles(); i++ ) {
			const dtMeshTile *tile = constMesh->getTile( i );
			if ( tile && tile->header ) {
				totalPolys += tile->header->polyCount;
				for ( int j = 0; j < tile->header->polyCount; j++ ) {
					const dtPoly *poly = &tile->polys[j];
					unsigned int linkIdx = poly->firstLink;
					while ( linkIdx != DT_NULL_LINK ) {
						const dtLink &link = tile->links[linkIdx];
						totalLinks++;
						if ( link.side != 0xff ) {
							externalLinks++;
						}
						linkIdx = link.next;
					}
				}
			}
		}
		Com_Printf( "Nav_LoadClass: %s addTile %d/%d ok, buildNavMeshTilesAt %d/%d ok, %d polys built, %d links (%d external), %d off-mesh links loaded\n",
					cls->name, addOk, addOk + addFail, buildOk, buildOk + buildFail, totalPolys, totalLinks, externalLinks, header.numOffMeshConns );
	}

	data->query = new dtNavMeshQuery();
	if ( dtStatusFailed( data->query->init( data->mesh, 2048 ) ) ) {
		Com_Printf( "Nav_LoadClass: dtNavMeshQuery::init failed for %s\n", cls->name );
		return false;
	}

	data->loaded = true;
	Com_Printf( "Nav_LoadClass: loaded %s (%d tile layers)\n", qpath, header.numTiles );
	return true;
}

/*
===========
Nav_LoadMap
===========
*/
void Nav_LoadMap( const char *mapname ) {
	// every tracked handle points at a cache we're about to delete below.
	Nav_ClearObstacles();

	for ( int i = 0; i < NAV_MAX_CLASSES; i++ ) {
		NavData_t *data = &navData[i];
		delete data->query;
		delete data->mesh;
		delete data->cache;
		delete data->alloc;
		delete data->compressor;
		delete data->meshProcess;
		memset( data, 0, sizeof( *data ) );

		Nav_LoadClass( mapname, i );
	}
}
