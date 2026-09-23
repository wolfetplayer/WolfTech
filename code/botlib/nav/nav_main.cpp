// nav_main.cpp -- nav module lifecycle: shared state, init/shutdown, class selection.

#include "nav_local.h"
#include "nav_public.h"

#include <cstdio>
#include <cstring>

NavData_t navData[NAV_MAX_CLASSES];
int navCurrentClass = 0;

/*
==============
Nav_CurrentData
==============
*/
NavData_t *Nav_CurrentData( void ) {
	NavData_t *data = &navData[navCurrentClass];
	return data->loaded ? data : NULL;
}

/*
========
Nav_Init
========
*/
void Nav_Init( void ) {
	memset( navData, 0, sizeof( navData ) );
	navCurrentClass = 0;
	printf( "Nav_Init: navigation module initialized\n" );
}

/*
============
Nav_Shutdown
============
*/
void Nav_Shutdown( void ) {
	for ( int i = 0; i < NAV_MAX_CLASSES; i++ ) {
		NavData_t *data = &navData[i];
		delete data->query;
		delete data->mesh;
		delete data->cache;
		delete data->alloc;
		delete data->compressor;
		delete data->meshProcess;
	}
	memset( navData, 0, sizeof( navData ) );
	printf( "Nav_Shutdown: navigation module shut down\n" );
}

/*
===============
Nav_SelectClass
===============
*/
void Nav_SelectClass( int classIndex ) {
	if ( classIndex < 0 || classIndex >= NAV_MAX_CLASSES ) {
		return;
	}
	navCurrentClass = classIndex;
}
