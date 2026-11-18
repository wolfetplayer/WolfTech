// tr_patch.h -- fixed-level bezier tessellation for MST_PATCH surfaces (curved brushes: arches, pipes, stairs).

#ifndef TR_PATCH_H_
#define TR_PATCH_H_

void R_InitPatches( void );

// 0 if patchWidth/patchHeight aren't a valid odd-dimensioned control grid.
int R_PatchTessVertCount( int patchWidth, int patchHeight );
int R_PatchTessIndexCount( int patchWidth, int patchHeight );

// outVerts/outIndexes must be sized via the two counts above; outIndexes are local (0-based within outVerts).
void R_TessellatePatch( const drawVert_t *controlVerts, int patchWidth, int patchHeight,
						 drawVert_t *outVerts, int *outIndexes );

#endif // TR_PATCH_H_
