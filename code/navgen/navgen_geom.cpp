// navgen_geom.cpp -- brushes mirror Q3_BSPBrushToMapBrush (bspc/map_q3.c); patches mirror CM_GeneratePatchCollide's grid prep.

#include "navgen_geom.h"
#include "navgen_bspformat.h"
#include "navgen_winding.h"
#include "navgen_patch.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

static void AddVert( std::vector<float> &verts, std::vector<int> &tris, const vec_t *v ) {
	int index = (int)( verts.size() / 3 );
	verts.push_back( v[0] );
	verts.push_back( v[1] );
	verts.push_back( v[2] );
	tris.push_back( index );
}

static void AddTri( std::vector<float> &verts, std::vector<int> &tris, const vec_t *a, const vec_t *b, const vec_t *c ) {
	AddVert( verts, tris, a );
	AddVert( verts, tris, b );
	AddVert( verts, tris, c );
}

static int SolidContents( int contentFlags ) {
	return ( contentFlags & ( CONTENTS_SOLID | CONTENTS_PLAYERCLIP | CONTENTS_MONSTERCLIP ) ) != 0;
}

static int SkipFace( int surfaceFlags ) {
	return ( surfaceFlags & ( SURF_SKY | SURF_NODRAW ) ) != 0;
}

/*
===========
LoadBrushes
===========
*/
static void LoadBrushes( const byte *base, const dheader_t *header, std::vector<float> &verts, std::vector<int> &tris ) {
	const dmodel_t *model = (const dmodel_t *)( base + header->lumps[LUMP_MODELS].fileofs );
	const dbrush_t *brushes = (const dbrush_t *)( base + header->lumps[LUMP_BRUSHES].fileofs );
	const dbrushside_t *sides = (const dbrushside_t *)( base + header->lumps[LUMP_BRUSHSIDES].fileofs );
	const dplane_t *planes = (const dplane_t *)( base + header->lumps[LUMP_PLANES].fileofs );
	const dshader_t *shaders = (const dshader_t *)( base + header->lumps[LUMP_SHADERS].fileofs );

	for ( int i = model->firstBrush, m = 0; m < model->numBrushes; i++, m++ ) {
		const dbrush_t *brush = &brushes[i];

		if ( !SolidContents( shaders[brush->shaderNum].contentFlags ) ) {
			continue;
		}

		for ( int s = 0; s < brush->numSides; s++ ) {
			const dbrushside_t *side = &sides[brush->firstSide + s];
			const dplane_t *plane = &planes[side->planeNum];

			if ( SkipFace( shaders[side->shaderNum].surfaceFlags ) ) {
				continue;
			}

			winding_t *w = BaseWindingForPlane( (vec_t *)plane->normal, plane->dist );

			// planeNum^1 keeps the inward half-space (a stored plane's positive side faces out of the brush).
			for ( int s2 = 0; s2 < brush->numSides && w; s2++ ) {
				const dbrushside_t *chopSide = &sides[brush->firstSide + s2];

				if ( chopSide == side || chopSide->planeNum == ( side->planeNum ^ 1 ) ) {
					continue;
				}

				const dplane_t *chopPlane = &planes[chopSide->planeNum ^ 1];
				ChopWindingInPlace( &w, (vec_t *)chopPlane->normal, chopPlane->dist, NAVGEN_ON_EPSILON );
			}

			if ( w ) {
				RemoveColinearPoints( w );
				for ( int j = 2; j < w->numpoints; j++ ) {
					AddTri( verts, tris, w->p[0], w->p[j - 1], w->p[j] );
				}
				FreeWinding( w );
			}
		}
	}
}

/*
===========
LoadPatches
===========
*/
static void LoadPatches( const byte *base, const dheader_t *header, std::vector<float> &verts, std::vector<int> &tris ) {
	const dmodel_t *model = (const dmodel_t *)( base + header->lumps[LUMP_MODELS].fileofs );
	const dsurface_t *surfaces = (const dsurface_t *)( base + header->lumps[LUMP_SURFACES].fileofs );
	const drawVert_t *drawVerts = (const drawVert_t *)( base + header->lumps[LUMP_DRAWVERTS].fileofs );
	const dshader_t *shaders = (const dshader_t *)( base + header->lumps[LUMP_SHADERS].fileofs );

	for ( int i = model->firstSurface, n = 0; n < model->numSurfaces; i++, n++ ) {
		const dsurface_t *surf = &surfaces[i];

		if ( surf->surfaceType != MST_PATCH || surf->patchWidth <= 0 ) {
			continue;
		}
		if ( !SolidContents( shaders[surf->shaderNum].contentFlags ) ) {
			continue;
		}
		if ( SkipFace( shaders[surf->shaderNum].surfaceFlags ) ) {
			continue;
		}

		cGrid_t grid;
		grid.width = surf->patchWidth;
		grid.height = surf->patchHeight;
		grid.wrapWidth = qfalse;
		grid.wrapHeight = qfalse;

		const drawVert_t *curveVerts = &drawVerts[surf->firstVert];
		for ( int x = 0; x < grid.width; x++ ) {
			for ( int y = 0; y < grid.height; y++ ) {
				VectorCopy( curveVerts[y * grid.width + x].xyz, grid.points[x][y] );
			}
		}

		NavGen_SubdividePatch( &grid );

		for ( int x = 0; x < grid.width - 1; x++ ) {
			for ( int y = 0; y < grid.height - 1; y++ ) {
				AddTri( verts, tris, grid.points[x][y], grid.points[x + 1][y], grid.points[x + 1][y + 1] );
				AddTri( verts, tris, grid.points[x + 1][y + 1], grid.points[x][y + 1], grid.points[x][y] );
			}
		}
	}
}

/*
==================
NavGen_LoadGeometry
==================
*/
int NavGen_LoadGeometry( const char *mapPath, navGeom_t *outGeom ) {
	FILE *f = fopen( mapPath, "rb" );
	if ( !f ) {
		fprintf( stderr, "NavGen_LoadGeometry: could not open %s\n", mapPath );
		return qfalse;
	}

	fseek( f, 0, SEEK_END );
	long size = ftell( f );
	fseek( f, 0, SEEK_SET );

	byte *data = (byte *)malloc( size );
	if ( fread( data, 1, size, f ) != (size_t)size ) {
		fprintf( stderr, "NavGen_LoadGeometry: short read on %s\n", mapPath );
		fclose( f );
		free( data );
		return qfalse;
	}
	fclose( f );

	const dheader_t *header = (const dheader_t *)data;
	if ( header->ident != BSP_IDENT || header->version != BSP_VERSION ) {
		fprintf( stderr, "NavGen_LoadGeometry: %s is not a recognized .bsp (ident %x version %d)\n",
				 mapPath, header->ident, header->version );
		free( data );
		return qfalse;
	}

	std::vector<float> verts;
	std::vector<int> tris;

	LoadBrushes( data, header, verts, tris );
	LoadPatches( data, header, verts, tris );

	free( data );

	if ( tris.empty() ) {
		fprintf( stderr, "NavGen_LoadGeometry: no walkable geometry found in %s\n", mapPath );
		return qfalse;
	}

	outGeom->numVerts = (int)( verts.size() / 3 );
	outGeom->verts = (float *)malloc( verts.size() * sizeof( float ) );
	memcpy( outGeom->verts, verts.data(), verts.size() * sizeof( float ) );

	outGeom->numTris = (int)( tris.size() / 3 );
	outGeom->tris = (int *)malloc( tris.size() * sizeof( int ) );
	memcpy( outGeom->tris, tris.data(), tris.size() * sizeof( int ) );

	return qtrue;
}

/*
==================
NavGen_FreeGeometry
==================
*/
void NavGen_FreeGeometry( navGeom_t *geom ) {
	free( geom->verts );
	free( geom->tris );
	geom->verts = NULL;
	geom->tris = NULL;
	geom->numVerts = geom->numTris = 0;
}

/*
=============
NavGen_WriteObj
=============
*/
void NavGen_WriteObj( const navGeom_t *geom, const char *path ) {
	FILE *f = fopen( path, "w" );
	if ( !f ) {
		fprintf( stderr, "NavGen_WriteObj: could not open %s\n", path );
		return;
	}

	for ( int i = 0; i < geom->numVerts; i++ ) {
		const float *v = &geom->verts[i * 3];
		fprintf( f, "v %f %f %f\n", v[0], v[2], -v[1] ); // Quake Z-up -> OBJ Y-up
	}
	for ( int i = 0; i < geom->numTris; i++ ) {
		const int *t = &geom->tris[i * 3];
		fprintf( f, "f %d %d %d\n", t[0] + 1, t[1] + 1, t[2] + 1 );
	}

	fclose( f );
}
