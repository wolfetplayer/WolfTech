// navgen_bspformat.h -- .bsp lump layout, copied by value from qfiles.h (see code/bspc's own q3_dbrush_t for the same pattern).

#ifndef __NAVGEN_BSPFORMAT_H
#define __NAVGEN_BSPFORMAT_H

#include "navgen_types.h"

#define BSP_IDENT   ( ( 'P' << 24 ) + ( 'S' << 16 ) + ( 'B' << 8 ) + 'I' )
#define BSP_VERSION 47

#define MAX_QPATH 64

typedef struct {
	int fileofs, filelen;
} lump_t;

#define LUMP_ENTITIES       0
#define LUMP_SHADERS        1
#define LUMP_PLANES         2
#define LUMP_NODES          3
#define LUMP_LEAFS          4
#define LUMP_LEAFSURFACES   5
#define LUMP_LEAFBRUSHES    6
#define LUMP_MODELS         7
#define LUMP_BRUSHES        8
#define LUMP_BRUSHSIDES     9
#define LUMP_DRAWVERTS      10
#define LUMP_DRAWINDEXES    11
#define LUMP_FOGS           12
#define LUMP_SURFACES       13
#define LUMP_LIGHTMAPS      14
#define LUMP_LIGHTGRID      15
#define LUMP_VISIBILITY     16
#define HEADER_LUMPS        17

typedef struct {
	int ident;
	int version;
	lump_t lumps[HEADER_LUMPS];
} dheader_t;

typedef struct {
	float mins[3], maxs[3];
	int firstSurface, numSurfaces;
	int firstBrush, numBrushes;
} dmodel_t;

typedef struct {
	char shader[MAX_QPATH];
	int surfaceFlags;
	int contentFlags;
} dshader_t;

typedef struct {
	float normal[3];
	float dist;
} dplane_t;

typedef struct {
	int planeNum;
	int shaderNum;
} dbrushside_t;

typedef struct {
	int firstSide;
	int numSides;
	int shaderNum;
} dbrush_t;

typedef enum {
	MST_BAD,
	MST_PLANAR,
	MST_PATCH,
	MST_TRIANGLE_SOUP,
	MST_FLARE,
	MST_FOLIAGE
} mapSurfaceType_t;

typedef struct {
	vec3_t xyz;
	float st[2];
	float lightmap[2];
	vec3_t normal;
	byte color[4];
} drawVert_t;

typedef struct {
	int shaderNum;
	int fogNum;
	int surfaceType;

	int firstVert;
	int numVerts;

	int firstIndex;
	int numIndexes;

	int lightmapNum;
	int lightmapX, lightmapY;
	int lightmapWidth, lightmapHeight;

	vec3_t lightmapOrigin;
	vec3_t lightmapVecs[3];

	int patchWidth;
	int patchHeight;
} dsurface_t;

// code/qcommon/surfaceflags.h values
#define CONTENTS_SOLID          1
#define CONTENTS_PLAYERCLIP     0x10000
#define CONTENTS_MONSTERCLIP    0x20000
#define CONTENTS_PLAYERCLIP2    0x800000 // true playerclip - a second, engine-specific playerclip flag (see textures/common/clip_player)
#define SURF_SKY                0x4
#define SURF_LADDER             0x8
#define SURF_NODRAW             0x80

#endif // __NAVGEN_BSPFORMAT_H
