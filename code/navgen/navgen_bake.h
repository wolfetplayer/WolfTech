// navgen_bake.h -- Recast/DetourTileCache bake pipeline for one agent class.

#ifndef __NAVGEN_BAKE_H
#define __NAVGEN_BAKE_H

#include <vector>

#include "navgen_geom.h"
#include "navgen_classes.h"

// bakes geom for cls into <outDir>/<mapName>_<cls->name>.navcache; qfalse on failure.
int NavGen_BakeClass( const navGeom_t *geom, const navGenClass_t *cls, const char *mapName, const char *outDir );

// same bake, serialized into memory instead of written to an OS path (e.g. for the engine's FS_WriteFile); qfalse on failure.
int NavGen_BakeClassToBuffer( const navGeom_t *geom, const navGenClass_t *cls, std::vector<unsigned char> &outBytes );

#endif // __NAVGEN_BAKE_H
