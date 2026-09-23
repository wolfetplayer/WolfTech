// navgen_bake.cpp -- the two filters below are ported from Unvanquished's Recast integration (src/shared/navgen/nav.cpp).

#include "navgen_bake.h"
#include "navgen_types.h"
#include "navcache_format.h"

#include "Recast.h"
#include "RecastAlloc.h"
#include "DetourTileCacheBuilder.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <vector>

static const int NAVGEN_TILE_SIZE = 64; // cells

// no compression: an offline tool has no reason to pay that cost.
/*
============================
NavGenPassthroughCompressor
============================
*/
struct NavGenPassthroughCompressor : public dtTileCacheCompressor {
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

// box-shaped erosion instead of Recast's cylindrical rcErodeWalkableArea, since collision here is AABB.
/*
========================
rcErodeWalkableAreaByBox
========================
*/
static bool rcErodeWalkableAreaByBox( rcContext *ctx, int boxRadius, rcCompactHeightfield &chf ) {
	const int w = chf.width;
	const int h = chf.height;

	unsigned char *dist = (unsigned char *)rcAlloc( sizeof( unsigned char ) * chf.spanCount, RC_ALLOC_TEMP );
	if ( !dist ) {
		return false;
	}
	memset( dist, 0xff, sizeof( unsigned char ) * chf.spanCount );

	for ( int y = 0; y < h; ++y ) {
		for ( int x = 0; x < w; ++x ) {
			const rcCompactCell &c = chf.cells[x + y * w];
			for ( int i = (int)c.index, ni = (int)( c.index + c.count ); i < ni; ++i ) {
				if ( chf.areas[i] == RC_NULL_AREA ) {
					dist[i] = 0;
				} else {
					const rcCompactSpan &s = chf.spans[i];
					int nc = 0;
					for ( int dir = 0; dir < 4; ++dir ) {
						if ( rcGetCon( s, dir ) != RC_NOT_CONNECTED ) {
							const int nx = x + rcGetDirOffsetX( dir );
							const int ny = y + rcGetDirOffsetY( dir );
							const int nidx = (int)chf.cells[nx + ny * w].index + rcGetCon( s, dir );
							if ( chf.areas[nidx] != RC_NULL_AREA ) {
								nc++;
							}
						}
					}
					if ( nc != 4 ) {
						dist[i] = 0;
					}
				}
			}
		}
	}

	unsigned char nd;

	for ( int y = 0; y < h; ++y ) {
		for ( int x = 0; x < w; ++x ) {
			const rcCompactCell &c = chf.cells[x + y * w];
			for ( int i = (int)c.index, ni = (int)( c.index + c.count ); i < ni; ++i ) {
				const rcCompactSpan &s = chf.spans[i];

				if ( rcGetCon( s, 0 ) != RC_NOT_CONNECTED ) {
					const int ax = x + rcGetDirOffsetX( 0 );
					const int ay = y + rcGetDirOffsetY( 0 );
					const int ai = (int)chf.cells[ax + ay * w].index + rcGetCon( s, 0 );
					const rcCompactSpan &as = chf.spans[ai];
					nd = (unsigned char)rcMin( (int)dist[ai] + 2, 255 );
					if ( nd < dist[i] ) { dist[i] = nd; }

					if ( rcGetCon( as, 3 ) != RC_NOT_CONNECTED ) {
						const int aax = ax + rcGetDirOffsetX( 3 );
						const int aay = ay + rcGetDirOffsetY( 3 );
						const int aai = (int)chf.cells[aax + aay * w].index + rcGetCon( as, 3 );
						nd = (unsigned char)rcMin( (int)dist[aai] + 2, 255 );
						if ( nd < dist[i] ) { dist[i] = nd; }
					}
				}
				if ( rcGetCon( s, 3 ) != RC_NOT_CONNECTED ) {
					const int ax = x + rcGetDirOffsetX( 3 );
					const int ay = y + rcGetDirOffsetY( 3 );
					const int ai = (int)chf.cells[ax + ay * w].index + rcGetCon( s, 3 );
					const rcCompactSpan &as = chf.spans[ai];
					nd = (unsigned char)rcMin( (int)dist[ai] + 2, 255 );
					if ( nd < dist[i] ) { dist[i] = nd; }

					if ( rcGetCon( as, 2 ) != RC_NOT_CONNECTED ) {
						const int aax = ax + rcGetDirOffsetX( 2 );
						const int aay = ay + rcGetDirOffsetY( 2 );
						const int aai = (int)chf.cells[aax + aay * w].index + rcGetCon( as, 2 );
						nd = (unsigned char)rcMin( (int)dist[aai] + 2, 255 );
						if ( nd < dist[i] ) { dist[i] = nd; }
					}
				}
			}
		}
	}

	for ( int y = h - 1; y >= 0; --y ) {
		for ( int x = w - 1; x >= 0; --x ) {
			const rcCompactCell &c = chf.cells[x + y * w];
			for ( int i = (int)c.index, ni = (int)( c.index + c.count ); i < ni; ++i ) {
				const rcCompactSpan &s = chf.spans[i];

				if ( rcGetCon( s, 2 ) != RC_NOT_CONNECTED ) {
					const int ax = x + rcGetDirOffsetX( 2 );
					const int ay = y + rcGetDirOffsetY( 2 );
					const int ai = (int)chf.cells[ax + ay * w].index + rcGetCon( s, 2 );
					const rcCompactSpan &as = chf.spans[ai];
					nd = (unsigned char)rcMin( (int)dist[ai] + 2, 255 );
					if ( nd < dist[i] ) { dist[i] = nd; }

					if ( rcGetCon( as, 1 ) != RC_NOT_CONNECTED ) {
						const int aax = ax + rcGetDirOffsetX( 1 );
						const int aay = ay + rcGetDirOffsetY( 1 );
						const int aai = (int)chf.cells[aax + aay * w].index + rcGetCon( as, 1 );
						nd = (unsigned char)rcMin( (int)dist[aai] + 2, 255 );
						if ( nd < dist[i] ) { dist[i] = nd; }
					}
				}
				if ( rcGetCon( s, 1 ) != RC_NOT_CONNECTED ) {
					const int ax = x + rcGetDirOffsetX( 1 );
					const int ay = y + rcGetDirOffsetY( 1 );
					const int ai = (int)chf.cells[ax + ay * w].index + rcGetCon( s, 1 );
					const rcCompactSpan &as = chf.spans[ai];
					nd = (unsigned char)rcMin( (int)dist[ai] + 2, 255 );
					if ( nd < dist[i] ) { dist[i] = nd; }

					if ( rcGetCon( as, 0 ) != RC_NOT_CONNECTED ) {
						const int aax = ax + rcGetDirOffsetX( 0 );
						const int aay = ay + rcGetDirOffsetY( 0 );
						const int aai = (int)chf.cells[aax + aay * w].index + rcGetCon( as, 0 );
						nd = (unsigned char)rcMin( (int)dist[aai] + 2, 255 );
						if ( nd < dist[i] ) { dist[i] = nd; }
					}
				}
			}
		}
	}

	const unsigned char thr = (unsigned char)( boxRadius * 2 );
	for ( int i = 0; i < chf.spanCount; ++i ) {
		if ( dist[i] < thr ) {
			chf.areas[i] = RC_NULL_AREA;
		}
	}

	rcFree( dist );
	return true;
}

// bridges small gaps a box collider can straddle but Recast would otherwise treat as void.
/*
==========
rcFilterGaps
==========
*/
static void rcFilterGaps( rcContext *ctx, int walkableRadius, int walkableClimb, int walkableHeight, rcHeightfield &solid ) {
	const int h = solid.height;
	const int w = solid.width;
	const int MAX_HEIGHT = 0xffff;
	std::vector<int> spanData;
	std::vector<int> data;

	for ( int y = 0; y < h; ++y ) {
		for ( int x = 0; x < w; ++x ) {
			for ( rcSpan *s = solid.spans[x + y * w]; s; s = s->next ) {
				const int sbot = s->smax;

				if ( s->area == RC_NULL_AREA ) {
					continue;
				}

				for ( int dir = 0; dir < 4; dir++ ) {
					const int dirx = rcGetDirOffsetX( dir );
					const int diry = rcGetDirOffsetY( dir );
					int dx = x;
					int dy = y;
					bool freeSpace = false;
					bool stop = false;

					for ( int i = 1; i < walkableRadius * 2; i++ ) {
						dx = dx + dirx;
						dy = dy + diry;
						if ( dx < 0 || dy < 0 || dx >= w || dy >= h ) {
							freeSpace = false;
							stop = false;
							break;
						}

						freeSpace = false;

						for ( rcSpan *ns = solid.spans[dx + dy * w]; ns; ns = ns->next ) {
							int nsbot = ns->smax;
							int nstop = ( ns->next ) ? ( ns->next->smin ) : MAX_HEIGHT;

							if ( abs( sbot - nsbot ) <= walkableClimb && ns->area != RC_NULL_AREA ) {
								stop = true;
								if ( i > 1 ) {
									freeSpace = true;
								}
								break;
							}

							if ( nsbot < sbot && nstop >= sbot + walkableHeight ) {
								freeSpace = true;
								data.push_back( dx );
								data.push_back( dy );
								data.push_back( sbot );
								break;
							}
						}

						if ( stop || !freeSpace ) {
							break;
						}
					}
					if ( freeSpace && stop ) {
						for ( size_t i = 0; i < data.size(); i++ ) {
							spanData.push_back( data[i] );
						}
					}
					data.clear();
				}
			}
		}
	}

	for ( size_t i = 0; i < spanData.size(); i += 3 ) {
		rcAddSpan( ctx, solid, spanData[i], spanData[i + 1], spanData[i + 2] - 1, spanData[i + 2], RC_WALKABLE_AREA, walkableClimb );
	}
}

/*
========
BakeTile
========
*/
static bool BakeTile( rcContext &ctx, const rcConfig &cfg, const navGeom_t *geom, int tx, int ty,
					   std::vector<unsigned char *> &tileBlobs, std::vector<int> &tileBlobSizes ) {
	rcHeightfield *solid = rcAllocHeightfield();
	if ( !rcCreateHeightfield( &ctx, *solid, cfg.width, cfg.height, cfg.bmin, cfg.bmax, cfg.cs, cfg.ch ) ) {
		rcFreeHeightField( solid );
		return false;
	}

	std::vector<unsigned char> triAreas( geom->numTris, 0 );
	rcMarkWalkableTriangles( &ctx, cfg.walkableSlopeAngle, geom->verts, geom->numVerts,
							 geom->tris, geom->numTris, triAreas.data() );
	rcRasterizeTriangles( &ctx, geom->verts, geom->numVerts, geom->tris, triAreas.data(), geom->numTris, *solid );

	rcFilterLowHangingWalkableObstacles( &ctx, cfg.walkableClimb, *solid );
	rcFilterWalkableLowHeightSpans( &ctx, cfg.walkableHeight, *solid );
	rcFilterGaps( &ctx, cfg.walkableRadius, cfg.walkableClimb, cfg.walkableHeight, *solid );

	rcCompactHeightfield *chf = rcAllocCompactHeightfield();
	if ( !rcBuildCompactHeightfield( &ctx, cfg.walkableHeight, cfg.walkableClimb, *solid, *chf ) ) {
		rcFreeHeightField( solid );
		rcFreeCompactHeightfield( chf );
		return false;
	}
	rcFreeHeightField( solid );

	if ( !rcErodeWalkableAreaByBox( &ctx, cfg.walkableRadius, *chf ) ) {
		rcFreeCompactHeightfield( chf );
		return false;
	}

	rcHeightfieldLayerSet *lset = rcAllocHeightfieldLayerSet();
	if ( !rcBuildHeightfieldLayers( &ctx, *chf, cfg.borderSize, cfg.walkableHeight, *lset ) ) {
		rcFreeCompactHeightfield( chf );
		rcFreeHeightfieldLayerSet( lset );
		return false;
	}
	rcFreeCompactHeightfield( chf );

	NavGenPassthroughCompressor comp;

	for ( int i = 0; i < lset->nlayers; i++ ) {
		const rcHeightfieldLayer *layer = &lset->layers[i];

		dtTileCacheLayerHeader header;
		header.magic = DT_TILECACHE_MAGIC;
		header.version = DT_TILECACHE_VERSION;
		header.tx = tx;
		header.ty = ty;
		header.tlayer = i;
		header.bmin[0] = layer->bmin[0]; header.bmin[1] = layer->bmin[1]; header.bmin[2] = layer->bmin[2];
		header.bmax[0] = layer->bmax[0]; header.bmax[1] = layer->bmax[1]; header.bmax[2] = layer->bmax[2];
		header.width = (unsigned char)layer->width;
		header.height = (unsigned char)layer->height;
		header.minx = (unsigned char)layer->minx;
		header.maxx = (unsigned char)layer->maxx;
		header.miny = (unsigned char)layer->miny;
		header.maxy = (unsigned char)layer->maxy;
		header.hmin = (unsigned short)layer->hmin;
		header.hmax = (unsigned short)layer->hmax;

		unsigned char *data = NULL;
		int dataSize = 0;
		dtStatus status = dtBuildTileCacheLayer( &comp, &header, layer->heights, layer->areas, layer->cons, &data, &dataSize );
		if ( dtStatusFailed( status ) ) {
			continue;
		}

		tileBlobs.push_back( data );
		tileBlobSizes.push_back( dataSize );
	}

	rcFreeHeightfieldLayerSet( lset );
	return true;
}

/*
===============
NavGen_BakeClass
===============
*/
int NavGen_BakeClass( const navGeom_t *geom, const navGenClass_t *cls, const char *mapName, const char *outDir ) {
	float bmin[3], bmax[3];
	rcCalcBounds( geom->verts, geom->numVerts, bmin, bmax );

	const float cellSize = cls->radius / 3.0f;
	const float cellHeight = 2.0f;

	rcConfig cfg;
	memset( &cfg, 0, sizeof( cfg ) );
	cfg.cs = cellSize;
	cfg.ch = cellHeight;
	cfg.walkableSlopeAngle = 45.0f;
	cfg.walkableHeight = (int)ceil( cls->height / cellHeight );
	cfg.walkableClimb = (int)floor( cls->climb / cellHeight );
	cfg.walkableRadius = (int)ceil( cls->radius / cellSize );
	cfg.maxEdgeLen = 0;
	cfg.maxSimplificationError = 1.3f;
	cfg.minRegionArea = 8 * 8;
	cfg.mergeRegionArea = 20 * 20;
	cfg.maxVertsPerPoly = 6;
	cfg.tileSize = NAVGEN_TILE_SIZE;
	cfg.borderSize = cfg.walkableRadius * 2;
	cfg.width = cfg.tileSize + cfg.borderSize * 2;
	cfg.height = cfg.tileSize + cfg.borderSize * 2;
	cfg.detailSampleDist = cfg.cs * 6.0f;
	cfg.detailSampleMaxError = cfg.ch;
	memcpy( cfg.bmin, bmin, sizeof( bmin ) );
	memcpy( cfg.bmax, bmax, sizeof( bmax ) );

	int gw, gh;
	rcCalcGridSize( bmin, bmax, cellSize, &gw, &gh );
	const int tw = ( gw + cfg.tileSize - 1 ) / cfg.tileSize;
	const int th = ( gh + cfg.tileSize - 1 ) / cfg.tileSize;

	printf( "  [%s] radius %.0f height %.0f: cellSize %.2f, grid %dx%d cells, %dx%d tiles\n",
			cls->name, cls->radius, cls->height, cellSize, gw, gh, tw, th );

	rcContext ctx( false );
	const float tcs = cfg.tileSize * cfg.cs;

	std::vector<NavCacheTileEntry> entries;
	std::vector<unsigned char *> allBlobs;
	int tilesWithLayers = 0;

	for ( int ty = 0; ty < th; ty++ ) {
		for ( int tx = 0; tx < tw; tx++ ) {
			rcConfig tileCfg = cfg;
			tileCfg.bmin[0] = cfg.bmin[0] + tx * tcs;
			tileCfg.bmin[1] = cfg.bmin[1];
			tileCfg.bmin[2] = cfg.bmin[2] + ty * tcs;
			tileCfg.bmax[0] = cfg.bmin[0] + ( tx + 1 ) * tcs;
			tileCfg.bmax[1] = cfg.bmax[1];
			tileCfg.bmax[2] = cfg.bmin[2] + ( ty + 1 ) * tcs;

			tileCfg.bmin[0] -= tileCfg.borderSize * tileCfg.cs;
			tileCfg.bmin[2] -= tileCfg.borderSize * tileCfg.cs;
			tileCfg.bmax[0] += tileCfg.borderSize * tileCfg.cs;
			tileCfg.bmax[2] += tileCfg.borderSize * tileCfg.cs;

			std::vector<unsigned char *> tileBlobs;
			std::vector<int> tileBlobSizes;
			if ( !BakeTile( ctx, tileCfg, geom, tx, ty, tileBlobs, tileBlobSizes ) ) {
				fprintf( stderr, "  tile %d,%d failed to bake\n", tx, ty );
				continue;
			}

			if ( !tileBlobs.empty() ) {
				tilesWithLayers++;
			}
			for ( size_t i = 0; i < tileBlobs.size(); i++ ) {
				NavCacheTileEntry entry;
				entry.tx = tx;
				entry.ty = ty;
				entry.tlayer = (int)i;
				entry.dataSize = tileBlobSizes[i];
				entries.push_back( entry );
				allBlobs.push_back( tileBlobs[i] );
			}
		}
	}

	printf( "  [%s] %d/%d grid tiles have walkable floor (rest are void, walls, or too-steep terrain)\n",
			cls->name, tilesWithLayers, tw * th );

	char path[1024];
	snprintf( path, sizeof( path ), "%s/%s_%s.navcache", outDir, mapName, cls->name );

	FILE *f = fopen( path, "wb" );
	if ( !f ) {
		fprintf( stderr, "NavGen_BakeClass: could not open %s for writing\n", path );
		return qfalse;
	}

	NavCacheHeader header;
	header.magic = NAVCACHE_MAGIC;
	header.version = NAVCACHE_VERSION;
	header.cellSize = cellSize;
	header.cellHeight = cellHeight;
	header.orig[0] = bmin[0]; header.orig[1] = bmin[1]; header.orig[2] = bmin[2];
	header.tileSize = cfg.tileSize;
	header.tw = tw;
	header.th = th;
	header.numTiles = (int)entries.size();
	header.walkableHeight = cls->height;
	header.walkableRadius = cls->radius;
	header.walkableClimb = cls->climb;
	header.maxSimplificationError = cfg.maxSimplificationError;
	fwrite( &header, sizeof( header ), 1, f );

	for ( size_t i = 0; i < entries.size(); i++ ) {
		fwrite( &entries[i], sizeof( NavCacheTileEntry ), 1, f );
		fwrite( allBlobs[i], 1, entries[i].dataSize, f );
		dtFree( allBlobs[i] );
	}

	fclose( f );

	printf( "  wrote %s (%d tile layers)\n", path, (int)entries.size() );
	return qtrue;
}
