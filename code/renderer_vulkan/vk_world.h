// vk_world.h -- uploads tr_world (see tr_bsp.h) into GPU buffers and draws it, culled per surface by tr_world.c.

#ifndef VK_WORLD_H_
#define VK_WORLD_H_

// Called once after RE_LoadWorldMap finishes parsing a map. Safe to call again for a new map.
void vk_world_build( void );

// Frees the current world's GPU buffers; called by vk_world_build and at renderer shutdown.
void vk_world_clear( void );
void vk_world_shutdown( void );

// Draws every visible PLANAR/TRIANGLE_SOUP/PATCH surface, batched by shader+lightmap (fogPush is tr_fog.h's push-constant block). No-op if no world is built.
void vk_world_draw( const float mvp[16], const float fogPush[12] );

#endif // VK_WORLD_H_
