// vk_draw3d.h -- depth-tested 3D geometry recording, for now just a sanity test cube.

#ifndef VK_DRAW3D_H_
#define VK_DRAW3D_H_

void vk_draw3d_init( void );
void vk_draw3d_shutdown( void );

// r_vk_test3d-gated: draws a fixed-size RGB-faced cube centered on origin, transformed by mvp.
void vk_draw3d_test_cube( const vec3_t origin, float halfSize, const float mvp[16] );

#endif // VK_DRAW3D_H_
