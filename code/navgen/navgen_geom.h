// navgen_geom.h -- extracts walkable-surface triangle soup from a .bsp.

#ifndef __NAVGEN_GEOM_H
#define __NAVGEN_GEOM_H

typedef struct {
	float *verts;   // numVerts * 3 floats
	int numVerts;
	int *tris;      // numTris * 3 vertex indices
	int numTris;
} navGeom_t;

// loads solid/playerclip/monsterclip brush + patch geometry only (the only two sources with real collision); qfalse on failure.
int NavGen_LoadGeometry( const char *mapPath, navGeom_t *outGeom );
// same, from an already-loaded .bsp buffer (e.g. the engine's FS_ReadFile) instead of an OS path.
int NavGen_LoadGeometryFromMemory( const unsigned char *data, int size, navGeom_t *outGeom );
void NavGen_FreeGeometry( navGeom_t *geom );

// dumps the extracted triangle soup as a .obj for visual sanity-checking.
void NavGen_WriteObj( const navGeom_t *geom, const char *path );

#endif // __NAVGEN_GEOM_H
