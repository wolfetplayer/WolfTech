// tr_bsp.h -- .bsp world data, loaded once per map and read by the (not yet built) world renderer.

#ifndef TR_BSP_H_
#define TR_BSP_H_

typedef struct {
	char shaderName[MAX_QPATH];
	int surfaceFlags;
	int contentFlags;
	qhandle_t shader; // resolved once at load time via RE_RegisterShaderInternal
} bspShader_t;

// MST_PATCH is tessellated by tr_patch.c at build time; FLARE/FOLIAGE are stored but not yet drawable.
typedef struct {
	int shaderNum; // index into bspWorld_t.shaders
	int surfaceType; // mapSurfaceType_t

	int firstVert, numVerts; // into bspWorld_t.drawVerts
	int firstIndex, numIndexes; // into bspWorld_t.drawIndexes; already-triangulated for PLANAR and TRIANGLE_SOUP

	int patchWidth, patchHeight; // MST_PATCH only

	int lightmapNum; // index into bspWorld_t.lightmapData, or -1 for vertex-lit/no lightmap
} bspSurface_t;

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

	int numClusters;
	int clusterBytes;
	byte *visData; // NULL if the map has no LUMP_VISIBILITY data

	int numLightmaps;
	byte *lightmapData; // RGB8, numLightmaps * LIGHTMAP_WIDTH * LIGHTMAP_HEIGHT * 3 bytes

	qboolean loaded;
} bspWorld_t;

extern bspWorld_t tr_world;

void RE_LoadWorldMap( const char *name );

#endif // TR_BSP_H_
