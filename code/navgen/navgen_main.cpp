// navgen_main.cpp -- CLI entry point. Usage: navgen <path/to/map.bsp> [outDir]

#include "navgen_types.h"
#include "navgen_geom.h"
#include "navgen_classes.h"
#include "navgen_bake.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define NAVGEN_MKDIR( path ) _mkdir( path )
#else
#include <sys/stat.h>
#define NAVGEN_MKDIR( path ) mkdir( path, 0755 )
#endif

static void ExtractMapName( const char *bspPath, char *out, size_t outSize ) {
	const char *slash = strrchr( bspPath, '/' );
	const char *backslash = strrchr( bspPath, '\\' );
	const char *base = bspPath;
	if ( slash && slash > base ) { base = slash + 1; }
	if ( backslash && backslash > base ) { base = backslash + 1; }

	snprintf( out, outSize, "%s", base );
	char *dot = strrchr( out, '.' );
	if ( dot ) { *dot = '\0'; }
}

/*
====
main
====
*/
int main( int argc, char **argv ) {
	if ( argc < 2 ) {
		fprintf( stderr, "usage: %s <path/to/map.bsp> [outDir]\n", argv[0] );
		return 1;
	}

	const char *bspPath = argv[1];
	const char *outDir = ( argc >= 3 ) ? argv[2] : "main/maps/nav";

	char mapName[256];
	ExtractMapName( bspPath, mapName, sizeof( mapName ) );

	printf( "navgen: loading %s\n", bspPath );

	navGeom_t geom;
	if ( !NavGen_LoadGeometry( bspPath, &geom ) ) {
		return 1;
	}
	printf( "navgen: %d verts, %d tris of walkable geometry\n", geom.numVerts, geom.numTris );

	NAVGEN_MKDIR( outDir );

	char objPath[1024];
	snprintf( objPath, sizeof( objPath ), "%s/%s_geom.obj", outDir, mapName );
	NavGen_WriteObj( &geom, objPath );
	printf( "navgen: wrote %s for visual sanity-checking\n", objPath );

	int ok = qtrue;
	for ( int i = 0; i < NAVGEN_NUM_CLASSES; i++ ) {
		if ( !NavGen_BakeClass( &geom, &navGenClasses[i], mapName, outDir ) ) {
			ok = qfalse;
		}
	}

	NavGen_FreeGeometry( &geom );

	return ok ? 0 : 1;
}
