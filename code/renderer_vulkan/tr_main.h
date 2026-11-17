// tr_main.h -- view/projection matrix setup shared by anything that draws into the 3D scene.

#ifndef TR_MAIN_H_
#define TR_MAIN_H_

void R_InitMain( void );

// Builds Vulkan-convention (Y-down, Z in [0,1]) column-major 4x4 matrices from a refdef_t.
void R_SetupViewMatrix( const refdef_t *fd, float viewMatrix[16] );
void R_SetupProjectionMatrix( const refdef_t *fd, float projMatrix[16] );

// out = a * b (column-major, i.e. out*v == a*(b*v)).
void R_MultMatrix( const float a[16], const float b[16], float out[16] );

#endif // TR_MAIN_H_
