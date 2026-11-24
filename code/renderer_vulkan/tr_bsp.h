// tr_bsp.h -- .bsp world data, loaded once per map and read by the (not yet built) world renderer.

#ifndef TR_BSP_H_
#define TR_BSP_H_

typedef struct {
	char shaderName[MAX_QPATH];
	int surfaceFlags;
	int contentFlags;
	qhandle_t shader; // resolved once at load time via RE_RegisterShaderInternal
} bspShader_t;

typedef struct {
	int shaderNum; // index into bspWorld_t.shaders
	int surfaceType; // mapSurfaceType_t

	int firstVert, numVerts; // into bspWorld_t.drawVerts
	int firstIndex, numIndexes; // into bspWorld_t.drawIndexes; already-triangulated for PLANAR and TRIANGLE_SOUP

	int patchWidth, patchHeight; // MST_PATCH only

	int lightmapNum; // index into bspWorld_t.lightmapData, or -1 for vertex-lit/no lightmap

	int fogIndex; // into bspWorld_t.fogs; 0 means "not in any fog volume"
} bspSurface_t;


typedef struct {
	qboolean active;
	vec3_t mins, maxs; // world-space AABB, from the referenced brush's 6 axial sides
	vec3_t color;
	float depthForOpaque;
} bspFog_t;

typedef struct {
	char name[MAX_QPATH];

	int numShaders;
	bspShader_t *shaders;

	int numDrawVerts;
	drawVert_t *drawVerts;

	int numDrawIndexes;
	int *drawIndexes;

	int numSurfaces;
	bspSurface_t *surfaces;

	int numPlanes;
	dplane_t *planes;

	int numNodes;
	dnode_t *nodes;

	int numLeafs;
	dleaf_t *leafs;

	int numLeafSurfaces;
	int *leafSurfaces;

	int numSubmodels;
	dmodel_t *submodels;

	int numFogs; // always >= 1; index 0 is the reserved inactive "no fog" entry
	bspFog_t *fogs;

	char *entityString; // raw LUMP_ENTITIES text, null-terminated; read token-by-token by R_GetEntityToken
	char *entityParsePoint;

	int numClusters;
	int clusterBytes;
	byte *visData; // NULL if the map has no LUMP_VISIBILITY data

	int numLightmaps;
	byte *lightmapData; // RGB8, numLightmaps * LIGHTMAP_WIDTH * LIGHTMAP_HEIGHT * 3 bytes

	// Ambient/directed light samples on a regular 3D grid, used to shade entities (which have no lightmap of their own).
	vec3_t lightGridSize;
	vec3_t lightGridOrigin;
	vec3_t lightGridInverseSize;
	int lightGridBounds[3];
	byte *lightGridData; // 8 bytes/point: ambient RGB, directed RGB, direction as lat/long; NULL if absent (e.g. -nolight)

	// First sky-shaded surface's outer box faces, found while vk_world_build sorts drawable surfaces.
	qboolean hasSky;
	struct image_s *skyOuterBox[6]; // rt, lf, bk, ft, up, dn
	struct shader_s *skyShader; // same surface's shader -- its non-skyparms stages are cloud layers, see vk_sky.c

	qboolean loaded;
} bspWorld_t;

extern bspWorld_t tr_world;

void RE_LoadWorldMap( const char *name );

// Shares the client's already-loaded vis data (RE_SetWorldVisData) instead of allocating a second copy.
void R_SetExternalVisData( const byte *vis );

#endif // TR_BSP_H_
