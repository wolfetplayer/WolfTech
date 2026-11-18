// tr_world.h -- marks which world surfaces are visible this frame (BSP point-in-leaf + PVS + frustum).

#ifndef TR_WORLD_H_
#define TR_WORLD_H_

void R_MarkVisibleSurfaces( const refdef_t *fd );
qboolean R_SurfaceIsVisible( int surfaceIndex );

#endif // TR_WORLD_H_
