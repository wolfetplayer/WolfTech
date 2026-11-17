// tr_main.c -- view/projection matrix setup shared by anything that draws into the 3D scene.

#include <math.h>
#include <string.h>

#include "tr_local.h"
#include "tr_main.h"
#include "tr_bsp.h"

static cvar_t *r_znear;

// Fallback zFar for scenes with no world (menus, player previews) -- same value id uses.
#define TR_MAIN_ZFAR_NOWORLD 2048.0f

// Farthest corner of the loaded world's bounds from the camera, so distant geometry isn't clipped.
static float R_ComputeZFar( const refdef_t *fd ) {
	const float *mins, *maxs;
	float farthestSq = 0.0f;
	int i;

	if ( !tr_world.loaded || tr_world.numSubmodels == 0 || ( fd->rdflags & RDF_NOWORLDMODEL ) ) {
		return TR_MAIN_ZFAR_NOWORLD;
	}

	mins = tr_world.submodels[0].mins;
	maxs = tr_world.submodels[0].maxs;

	for ( i = 0; i < 8; i++ ) {
		vec3_t corner, delta;
		corner[0] = ( i & 1 ) ? mins[0] : maxs[0];
		corner[1] = ( i & 2 ) ? mins[1] : maxs[1];
		corner[2] = ( i & 4 ) ? mins[2] : maxs[2];
		VectorSubtract( corner, fd->vieworg, delta );
		float distSq = DotProduct( delta, delta );
		if ( distSq > farthestSq ) {
			farthestSq = distSq;
		}
	}

	return MAX( sqrtf( farthestSq ), 256.0f );
}

void R_InitMain( void ) {
	r_znear = ri.Cvar_Get( "r_znear", "2", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_znear, 0.001f, 200, qfalse );
}

void R_MultMatrix( const float a[16], const float b[16], float out[16] ) {
	int c, r, k;
	for ( c = 0; c < 4; c++ ) {
		for ( r = 0; r < 4; r++ ) {
			float sum = 0.0f;
			for ( k = 0; k < 4; k++ ) {
				sum += a[k * 4 + r] * b[c * 4 + k];
			}
			out[c * 4 + r] = sum;
		}
	}
}

// World -> Vulkan eye space (X=right, Y=up, Z=into the screen), built directly from the camera's basis.
void R_SetupViewMatrix( const refdef_t *fd, float viewMatrix[16] ) {
	const float *forward = fd->viewaxis[0];
	const float *left = fd->viewaxis[1];
	const float *up = fd->viewaxis[2];
	const float *eye = fd->vieworg;

	viewMatrix[0] = -left[0];
	viewMatrix[1] = up[0];
	viewMatrix[2] = -forward[0];
	viewMatrix[3] = 0.0f;

	viewMatrix[4] = -left[1];
	viewMatrix[5] = up[1];
	viewMatrix[6] = -forward[1];
	viewMatrix[7] = 0.0f;

	viewMatrix[8] = -left[2];
	viewMatrix[9] = up[2];
	viewMatrix[10] = -forward[2];
	viewMatrix[11] = 0.0f;

	viewMatrix[12] = DotProduct( left, eye );
	viewMatrix[13] = -DotProduct( up, eye );
	viewMatrix[14] = DotProduct( forward, eye );
	viewMatrix[15] = 1.0f;
}

// Vulkan clip space: Y-down, Z in [0,1] (vs GL's Y-up, Z in [-1,1]).
void R_SetupProjectionMatrix( const refdef_t *fd, float projMatrix[16] ) {
	float zNear = r_znear->value;
	float zFar = R_ComputeZFar( fd );
	float px = tanf( fd->fov_x * ( M_PI / 360.0f ) );
	float py = tanf( fd->fov_y * ( M_PI / 360.0f ) );
	float p10 = -zFar / ( zFar - zNear );

	memset( projMatrix, 0, 16 * sizeof( float ) );
	projMatrix[0] = 1.0f / px;
	projMatrix[5] = -1.0f / py;
	projMatrix[10] = p10;
	projMatrix[11] = -1.0f;
	projMatrix[14] = zNear * p10;
}
