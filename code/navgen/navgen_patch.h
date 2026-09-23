// navgen_patch.h -- bezier grid subdivision ported from code/qcommon/cm_patch.c.

#ifndef __NAVGEN_PATCH_H
#define __NAVGEN_PATCH_H

#include "navgen_types.h"

#define MAX_GRID_SIZE   129

typedef struct {
	int width;
	int height;
	qboolean wrapWidth;
	qboolean wrapHeight;
	vec3_t points[MAX_GRID_SIZE][MAX_GRID_SIZE]; // [width][height]
} cGrid_t;

// same two-axis subdivide pass as CM_GeneratePatchCollide; leaves grid->points as a dense vertex grid on the patch surface.
void NavGen_SubdividePatch( cGrid_t *grid );

#endif // __NAVGEN_PATCH_H
