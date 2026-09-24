// nav_load.cpp -- loads navgen's .navcache files into queryable dtNavMesh/dtTileCache instances.

#include "nav_local.h"
#include "nav_public.h"
#include "../../navgen/navcache_format.h"

#include <cstdio>
#include <cstring>

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

No off-mesh connections yet (ladders land in a later phase); every walkable
poly just gets a nonzero flag so Detour's default query filter accepts it.
===============
*/
struct NavMeshProcess : public dtTileCacheMeshProcess {
	virtual void process( dtNavMeshCreateParams *params, unsigned char *polyAreas, unsigned short *polyFlags ) {
		for ( int i = 0; i < params->polyCount; i++ ) {
			if ( polyAreas[i] != DT_TILECACHE_NULL_AREA ) {
				polyFlags[i] = 1;
			}
		}
	}
};

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
	tcparams.maxObstacles = 128; // unused until Phase 4

	data->alloc = new dtTileCacheAlloc();
	data->compressor = new NavLoadPassthroughCompressor();
	data->meshProcess = new NavMeshProcess();

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
		Com_Printf( "Nav_LoadClass: %s addTile %d/%d ok, buildNavMeshTilesAt %d/%d ok, %d polys built, %d links (%d external)\n",
					cls->name, addOk, addOk + addFail, buildOk, buildOk + buildFail, totalPolys, totalLinks, externalLinks );
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
