/*
===========================================================================

Return to Castle Wolfenstein single player GPL Source Code
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company.

This file is part of the Return to Castle Wolfenstein single player GPL Source Code (RTCW SP Source Code).

RTCW SP Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

RTCW SP Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RTCW SP Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the RTCW SP Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the RTCW SP Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

// tr_bsp.c -- loads .bsp lumps into flat arrays; no tree-walk/PVS/tessellation yet, see tr_bsp.h.

#include <string.h>
#include <math.h>

#include "tr_local.h"
#include "tr_shader.h"
#include "tr_bsp.h"
#include "vk_world.h"

bspWorld_t tr_world;

static byte *fileBase;

static void R_LoadShaders( const lump_t *l ) {
	int i, count;
	dshader_t *in;
	bspShader_t *out;

	in = (void *)( fileBase + l->fileofs );
	count = l->filelen / sizeof( *in );
	out = ri.Hunk_Alloc( count * sizeof( *out ), h_low );

	tr_world.shaders = out;
	tr_world.numShaders = count;

	for ( i = 0; i < count; i++, in++, out++ ) {
		Q_strncpyz( out->shaderName, in->shader, sizeof( out->shaderName ) );
		out->surfaceFlags = LittleLong( in->surfaceFlags );
		out->contentFlags = LittleLong( in->contentFlags );
		out->shader = RE_RegisterShaderInternal( out->shaderName, qfalse );
	}
}

static void R_LoadPlanes( const lump_t *l ) {
	int i, j, count;
	dplane_t *in, *out;

	in = (void *)( fileBase + l->fileofs );
	count = l->filelen / sizeof( *in );
	out = ri.Hunk_Alloc( count * sizeof( *out ), h_low );
	memcpy( out, in, count * sizeof( *out ) );

	tr_world.planes = out;
	tr_world.numPlanes = count;

	for ( i = 0; i < count; i++ ) {
		for ( j = 0; j < 3; j++ ) {
			out[i].normal[j] = LittleFloat( out[i].normal[j] );
		}
		out[i].dist = LittleFloat( out[i].dist );
	}
}

static void R_LoadDrawVerts( const lump_t *l ) {
	int i, j, count;
	drawVert_t *in, *out;

	in = (void *)( fileBase + l->fileofs );
	count = l->filelen / sizeof( *in );
	out = ri.Hunk_Alloc( count * sizeof( *out ), h_low );
	memcpy( out, in, count * sizeof( *out ) );

	tr_world.drawVerts = out;
	tr_world.numDrawVerts = count;

	for ( i = 0; i < count; i++ ) {
		for ( j = 0; j < 3; j++ ) {
			out[i].xyz[j] = LittleFloat( out[i].xyz[j] );
			out[i].normal[j] = LittleFloat( out[i].normal[j] );
		}
		for ( j = 0; j < 2; j++ ) {
			out[i].st[j] = LittleFloat( out[i].st[j] );
			out[i].lightmap[j] = LittleFloat( out[i].lightmap[j] );
		}
	}
}

static void R_LoadDrawIndexes( const lump_t *l ) {
	int i, count;
	int *in, *out;

	in = (void *)( fileBase + l->fileofs );
	count = l->filelen / sizeof( *in );
	out = ri.Hunk_Alloc( count * sizeof( *out ), h_low );

	tr_world.drawIndexes = out;
	tr_world.numDrawIndexes = count;

	for ( i = 0; i < count; i++ ) {
		out[i] = LittleLong( in[i] );
	}
}

// References ranges in tr_world.drawVerts/drawIndexes; PLANAR/TRIANGLE_SOUP are pre-triangulated.
static void R_LoadSurfaces( const lump_t *l ) {
	int i, count;
	dsurface_t *in;
	bspSurface_t *out;

	in = (void *)( fileBase + l->fileofs );
	count = l->filelen / sizeof( *in );
	out = ri.Hunk_Alloc( count * sizeof( *out ), h_low );

	tr_world.surfaces = out;
	tr_world.numSurfaces = count;

	for ( i = 0; i < count; i++, in++, out++ ) {
		out->shaderNum = LittleLong( in->shaderNum );
		out->surfaceType = LittleLong( in->surfaceType );
		out->firstVert = LittleLong( in->firstVert );
		out->numVerts = LittleLong( in->numVerts );
		out->firstIndex = LittleLong( in->firstIndex );
		out->numIndexes = LittleLong( in->numIndexes );
		out->patchWidth = LittleLong( in->patchWidth );
		out->patchHeight = LittleLong( in->patchHeight );
		out->lightmapNum = LittleLong( in->lightmapNum );
	}
}

// Each lightmap is its own LIGHTMAP_WIDTH*LIGHTMAP_HEIGHT RGB8 image; surfaces index it directly, no atlasing.
static void R_LoadLightmaps( const lump_t *l ) {
	int lightmapSize = LIGHTMAP_WIDTH * LIGHTMAP_HEIGHT * 3;

	tr_world.numLightmaps = l->filelen / lightmapSize;
	tr_world.lightmapData = NULL;

	if ( tr_world.numLightmaps == 0 ) {
		return;
	}

	tr_world.lightmapData = ri.Hunk_Alloc( l->filelen, h_low );
	memcpy( tr_world.lightmapData, fileBase + l->fileofs, l->filelen );
}

static void R_LoadLeafSurfaces( const lump_t *l ) {
	int i, count;
	int *in, *out;

	in = (void *)( fileBase + l->fileofs );
	count = l->filelen / sizeof( *in );
	out = ri.Hunk_Alloc( count * sizeof( *out ), h_low );

	tr_world.leafSurfaces = out;
	tr_world.numLeafSurfaces = count;

	for ( i = 0; i < count; i++ ) {
		out[i] = LittleLong( in[i] );
	}
}

static void R_LoadNodes( const lump_t *l ) {
	int i, j, count;
	dnode_t *in, *out;

	in = (void *)( fileBase + l->fileofs );
	count = l->filelen / sizeof( *in );
	out = ri.Hunk_Alloc( count * sizeof( *out ), h_low );
	memcpy( out, in, count * sizeof( *out ) );

	tr_world.nodes = out;
	tr_world.numNodes = count;

	for ( i = 0; i < count; i++ ) {
		out[i].planeNum = LittleLong( out[i].planeNum );
		out[i].children[0] = LittleLong( out[i].children[0] );
		out[i].children[1] = LittleLong( out[i].children[1] );
		for ( j = 0; j < 3; j++ ) {
			out[i].mins[j] = LittleLong( out[i].mins[j] );
			out[i].maxs[j] = LittleLong( out[i].maxs[j] );
		}
	}
}

static void R_LoadLeafs( const lump_t *l ) {
	int i, j, count;
	dleaf_t *in, *out;

	in = (void *)( fileBase + l->fileofs );
	count = l->filelen / sizeof( *in );
	out = ri.Hunk_Alloc( count * sizeof( *out ), h_low );
	memcpy( out, in, count * sizeof( *out ) );

	tr_world.leafs = out;
	tr_world.numLeafs = count;

	for ( i = 0; i < count; i++ ) {
		out[i].cluster = LittleLong( out[i].cluster );
		out[i].area = LittleLong( out[i].area );
		out[i].firstLeafSurface = LittleLong( out[i].firstLeafSurface );
		out[i].numLeafSurfaces = LittleLong( out[i].numLeafSurfaces );
		out[i].firstLeafBrush = LittleLong( out[i].firstLeafBrush );
		out[i].numLeafBrushes = LittleLong( out[i].numLeafBrushes );
		for ( j = 0; j < 3; j++ ) {
			out[i].mins[j] = LittleLong( out[i].mins[j] );
			out[i].maxs[j] = LittleLong( out[i].maxs[j] );
		}
	}
}

static void R_LoadSubmodels( const lump_t *l ) {
	int i, j, count;
	dmodel_t *in, *out;

	in = (void *)( fileBase + l->fileofs );
	count = l->filelen / sizeof( *in );
	out = ri.Hunk_Alloc( count * sizeof( *out ), h_low );
	memcpy( out, in, count * sizeof( *out ) );

	tr_world.submodels = out;
	tr_world.numSubmodels = count;

	for ( i = 0; i < count; i++ ) {
		for ( j = 0; j < 3; j++ ) {
			out[i].mins[j] = LittleFloat( out[i].mins[j] );
			out[i].maxs[j] = LittleFloat( out[i].maxs[j] );
		}
		out[i].firstSurface = LittleLong( out[i].firstSurface );
		out[i].numSurfaces = LittleLong( out[i].numSurfaces );
		out[i].firstBrush = LittleLong( out[i].firstBrush );
		out[i].numBrushes = LittleLong( out[i].numBrushes );
	}
}

// Grid cell size the map was compiled with; RTCW lets worldspawn override this, but we don't parse entity keys yet.
#define LIGHTGRID_SIZE_X 64.0f
#define LIGHTGRID_SIZE_Y 64.0f
#define LIGHTGRID_SIZE_Z 128.0f

// Bounds come from the world model, same simplification the opengl1 renderer itself falls back to.
static void R_LoadLightGrid( const lump_t *l ) {
	vec3_t gridSize = { LIGHTGRID_SIZE_X, LIGHTGRID_SIZE_Y, LIGHTGRID_SIZE_Z };
	const float *mins, *maxs;
	vec3_t gridMaxs;
	int numGridPoints;
	int i;

	tr_world.lightGridData = NULL;

	if ( tr_world.numSubmodels == 0 ) {
		return;
	}

	mins = tr_world.submodels[0].mins;
	maxs = tr_world.submodels[0].maxs;

	for ( i = 0; i < 3; i++ ) {
		tr_world.lightGridInverseSize[i] = 1.0f / gridSize[i];
		tr_world.lightGridOrigin[i] = gridSize[i] * ceilf( mins[i] / gridSize[i] );
		gridMaxs[i] = gridSize[i] * floorf( maxs[i] / gridSize[i] );
		tr_world.lightGridBounds[i] = (int)( ( gridMaxs[i] - tr_world.lightGridOrigin[i] ) / gridSize[i] ) + 1;
	}

	numGridPoints = tr_world.lightGridBounds[0] * tr_world.lightGridBounds[1] * tr_world.lightGridBounds[2];

	if ( l->filelen != numGridPoints * 8 ) {
		return; // mismatched or absent (e.g. a -nolight compile) -- R_LightForPoint falls back gracefully
	}

	tr_world.lightGridData = ri.Hunk_Alloc( l->filelen, h_low );
	memcpy( tr_world.lightGridData, fileBase + l->fileofs, l->filelen );
}

// Lump layout: int numClusters, int clusterBytes, then numClusters*clusterBytes PVS bits.
static void R_LoadVisibility( const lump_t *l ) {
	const int *buf;

	tr_world.numClusters = 0;
	tr_world.clusterBytes = 0;
	tr_world.visData = NULL;

	if ( !l->filelen ) {
		return;
	}

	buf = (void *)( fileBase + l->fileofs );
	tr_world.numClusters = LittleLong( buf[0] );
	tr_world.clusterBytes = LittleLong( buf[1] );

	tr_world.visData = ri.Hunk_Alloc( l->filelen - 8, h_low );
	memcpy( tr_world.visData, fileBase + l->fileofs + 8, l->filelen - 8 );
}

void RE_LoadWorldMap( const char *name ) {
	dheader_t *header;
	void *buffer;
	int i;
	int numFaces = 0, numTriSurfs = 0, numPatches = 0, numFlares = 0, numFoliage = 0;

	if ( tr_world.loaded && !Q_stricmp( tr_world.name, name ) ) {
		return;
	}

	ri.FS_ReadFile( name, &buffer );
	if ( !buffer ) {
		ri.Error( ERR_DROP, "RE_LoadWorldMap: %s not found", name );
	}

	memset( &tr_world, 0, sizeof( tr_world ) );
	Q_strncpyz( tr_world.name, name, sizeof( tr_world.name ) );

	header = (dheader_t *)buffer;
	fileBase = (byte *)header;

	i = LittleLong( header->version );
	if ( i != BSP_VERSION ) {
		ri.Error( ERR_DROP, "RE_LoadWorldMap: %s has wrong version number (%i should be %i)", name, i, BSP_VERSION );
	}

	for ( i = 0; i < (int)( sizeof( *header ) / 4 ); i++ ) {
		( (int *)header )[i] = LittleLong( ( (int *)header )[i] );
	}

	R_LoadShaders( &header->lumps[LUMP_SHADERS] );
	ri.Printf( PRINT_ALL, "...bsp checkpoint: shaders (%d)\n", tr_world.numShaders );
	R_LoadPlanes( &header->lumps[LUMP_PLANES] );
	ri.Printf( PRINT_ALL, "...bsp checkpoint: planes (%d)\n", tr_world.numPlanes );
	R_LoadDrawVerts( &header->lumps[LUMP_DRAWVERTS] );
	ri.Printf( PRINT_ALL, "...bsp checkpoint: drawverts (%d)\n", tr_world.numDrawVerts );
	R_LoadDrawIndexes( &header->lumps[LUMP_DRAWINDEXES] );
	ri.Printf( PRINT_ALL, "...bsp checkpoint: drawindexes (%d)\n", tr_world.numDrawIndexes );
	R_LoadSurfaces( &header->lumps[LUMP_SURFACES] );
	ri.Printf( PRINT_ALL, "...bsp checkpoint: surfaces (%d)\n", tr_world.numSurfaces );
	R_LoadLightmaps( &header->lumps[LUMP_LIGHTMAPS] );
	ri.Printf( PRINT_ALL, "...bsp checkpoint: lightmaps (%d)\n", tr_world.numLightmaps );
	R_LoadLeafSurfaces( &header->lumps[LUMP_LEAFSURFACES] );
	ri.Printf( PRINT_ALL, "...bsp checkpoint: leafsurfaces (%d)\n", tr_world.numLeafSurfaces );
	R_LoadNodes( &header->lumps[LUMP_NODES] );
	ri.Printf( PRINT_ALL, "...bsp checkpoint: nodes (%d)\n", tr_world.numNodes );
	R_LoadLeafs( &header->lumps[LUMP_LEAFS] );
	ri.Printf( PRINT_ALL, "...bsp checkpoint: leafs (%d)\n", tr_world.numLeafs );
	R_LoadSubmodels( &header->lumps[LUMP_MODELS] );
	ri.Printf( PRINT_ALL, "...bsp checkpoint: submodels (%d)\n", tr_world.numSubmodels );
	R_LoadLightGrid( &header->lumps[LUMP_LIGHTGRID] );
	ri.Printf( PRINT_ALL, "...bsp checkpoint: light grid (%s)\n", tr_world.lightGridData ? "loaded" : "absent" );
	R_LoadVisibility( &header->lumps[LUMP_VISIBILITY] );
	ri.Printf( PRINT_ALL, "...bsp checkpoint: visibility (%d clusters)\n", tr_world.numClusters );

	ri.FS_FreeFile( buffer );

	tr_world.loaded = qtrue;

	ri.Printf( PRINT_ALL, "...bsp checkpoint: about to vk_world_build\n" );
	vk_world_build();
	ri.Printf( PRINT_ALL, "...bsp checkpoint: vk_world_build returned\n" );

	for ( i = 0; i < tr_world.numSurfaces; i++ ) {
		switch ( tr_world.surfaces[i].surfaceType ) {
		case MST_PLANAR: numFaces++; break;
		case MST_TRIANGLE_SOUP: numTriSurfs++; break;
		case MST_PATCH: numPatches++; break;
		case MST_FLARE: numFlares++; break;
		case MST_FOLIAGE: numFoliage++; break;
		default: break;
		}
	}

	ri.Printf( PRINT_ALL, "...loaded %s: %d shaders, %d verts, %d indexes, %d surfaces "
			   "(%d faces, %d trisurf, %d patch, %d flare, %d foliage), %d planes, %d nodes, %d leafs, %d clusters\n",
			   tr_world.name, tr_world.numShaders, tr_world.numDrawVerts, tr_world.numDrawIndexes, tr_world.numSurfaces,
			   numFaces, numTriSurfs, numPatches, numFlares, numFoliage,
			   tr_world.numPlanes, tr_world.numNodes, tr_world.numLeafs, tr_world.numClusters );
}
