// tr_portal.h -- mirror/portal-surface recursion (RTCW mirrors, periscopes); world surfaces only, see tr_portal.c.

#ifndef TR_PORTAL_H_
#define TR_PORTAL_H_

void R_InitPortals( void );

// Call once per view, right after R_MarkVisibleSurfaces(fd) and before that view's own draws; re-run R_MarkVisibleSurfaces(fd) afterward if this returns qtrue.
qboolean R_TryRenderPortalView( const refdef_t *fd );

// qtrue only while drawing inside a recursive portal/mirror view (see vk_world.c/vk_pipelines.c).
qboolean R_InPortalView( void );
qboolean R_InMirrorView( void );

// World-space clip plane for the current recursive view; surfaces are kept where dot(pos, outNormal) - outDist >= 0.
void R_GetActivePortalPlane( vec3_t outNormal, float *outDist );

#endif // TR_PORTAL_H_
