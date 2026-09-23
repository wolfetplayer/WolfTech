// navcache_format.h -- on-disk format shared by navgen_bake.cpp (writer) and code/botlib/nav/nav_load.cpp (reader).

#ifndef __NAVCACHE_FORMAT_H
#define __NAVCACHE_FORMAT_H

static const int NAVCACHE_MAGIC = ( 'N' << 24 ) | ( 'G' << 16 ) | ( 'C' << 8 ) | '1';
static const int NAVCACHE_VERSION = 2;

struct NavCacheHeader {
	int magic;
	int version;
	float cellSize;
	float cellHeight;
	float orig[3];
	int tileSize;
	int tw, th;
	int numTiles;
	// added in version 2: needed to rebuild dtTileCacheParams at load time.
	float walkableHeight;
	float walkableRadius;
	float walkableClimb;
	float maxSimplificationError;
};

struct NavCacheTileEntry {
	int tx, ty, tlayer;
	int dataSize;
};

#endif // __NAVCACHE_FORMAT_H
