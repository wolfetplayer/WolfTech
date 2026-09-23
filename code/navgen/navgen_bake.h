// navgen_bake.h -- Recast/DetourTileCache bake pipeline for one agent class.

#ifndef __NAVGEN_BAKE_H
#define __NAVGEN_BAKE_H

#include "navgen_geom.h"
#include "navgen_classes.h"

// bakes geom for cls into <outDir>/<mapName>_<cls->name>.navcache; qfalse on failure.
int NavGen_BakeClass( const navGeom_t *geom, const navGenClass_t *cls, const char *mapName, const char *outDir );

#endif // __NAVGEN_BAKE_H
