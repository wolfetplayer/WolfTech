// vk_draw3d.h -- depth-tested 3D geometry recording: a sanity test cube, and per-entity position markers until real model rendering lands.

#ifndef VK_DRAW3D_H_
#define VK_DRAW3D_H_

void vk_draw3d_init( void );
void vk_draw3d_shutdown( void );

// r_vk_test3d-gated: draws a fixed-size RGB-faced cube centered on origin, transformed by mvp (fogPush is tr_fog.h's push-constant block).
void vk_draw3d_test_cube( const vec3_t origin, float halfSize, const float mvp[16], const float fogPush[12] );

// r_vk_showEntityMarkers-gated: one small cube per scene entity, standing in for real model rendering.
void vk_draw3d_entity_markers( const refEntity_t *entities, int numEntities, float halfSize, const float mvp[16], const float fogPush[12] );

#endif // VK_DRAW3D_H_
