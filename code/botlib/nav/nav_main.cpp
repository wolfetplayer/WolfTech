// nav_main.cpp -- nav module lifecycle and Phase 0 link/round-trip smoke test.

#include "nav_local.h"
#include "nav_public.h"

#include <cstdio>

void Nav_Init( void ) {
	printf( "Nav_Init: navigation module initialized\n" );
}

void Nav_Shutdown( void ) {
	printf( "Nav_Shutdown: navigation module shut down\n" );
}

int Nav_Test( void ) {
	dtNavMesh *mesh = dtAllocNavMesh();

	if ( !mesh ) {
		printf( "Nav_Test: dtAllocNavMesh failed\n" );
		return 0;
	}

	printf( "Nav_Test: dtAllocNavMesh/dtFreeNavMesh round trip OK\n" );
	dtFreeNavMesh( mesh );

	return 1;
}
