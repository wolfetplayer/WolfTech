// vk_world.h -- uploads tr_world (see tr_bsp.h) into GPU buffers and draws it, culled per surface by tr_world.c.

#ifndef VK_WORLD_H_
#define VK_WORLD_H_

// Called once after RE_LoadWorldMap finishes parsing a map; safe to call again for a new map.
void vk_world_build( void );

// Frees the current world's GPU buffers; called by vk_world_build and at renderer shutdown.
void vk_world_clear( void );
void vk_world_shutdown( void );

// Draws every visible PLANAR/TRIANGLE_SOUP/PATCH surface, batched by shader+lightmap (fogPush is tr_fog.h's push-constant block). No-op if no world is built.
void vk_world_draw( const float mvp[16], const float fogPush[12] );

// Draws one brush-model (func_door/etc.) entity, transformed by its current origin/axis. Call after vk_world_draw.
void vk_world_draw_brushmodel( const refEntity_t *ent, int bmodelIndex, const float mvp[16], const float fogPush[12] );

// First currently-visible isPortal surface (tr_shader.h), if any: its world-space plane plus a representative point (for tr_portal.c's RT_PORTALSURFACE search).
qboolean vk_world_findVisiblePortalSurface( vec3_t outNormal, float *outDist, vec3_t outPoint );

#endif // VK_WORLD_H_
