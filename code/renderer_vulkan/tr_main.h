// tr_main.h -- view/projection matrix setup shared by anything that draws into the 3D scene.

#ifndef TR_MAIN_H_
#define TR_MAIN_H_

void R_InitMain( void );

// Builds Vulkan-convention (Y-down, Z in [0,1]) column-major 4x4 matrices from a refdef_t.
void R_SetupViewMatrix( const refdef_t *fd, float viewMatrix[16] );
void R_SetupProjectionMatrix( const refdef_t *fd, float projMatrix[16] );

// out = a * b (column-major, i.e. out*v == a*(b*v)).
void R_MultMatrix( const float a[16], const float b[16], float out[16] );

typedef struct {
	vec3_t normal; // points into the visible volume
	float dist;
} cullPlane_t;

#define NUM_FRUSTUM_PLANES 4 // left, right, top, bottom; depth is handled by the depth buffer/zFar

// World-space side planes of the view frustum, for leaf/surface bounding-box culling.
void R_SetupFrustumPlanes( const refdef_t *fd, cullPlane_t frustum[NUM_FRUSTUM_PLANES] );

// qtrue if an axis-aligned box could be at least partly inside every frustum plane.
qboolean R_CullBoundsVsFrustum( const vec3_t mins, const vec3_t maxs, const cullPlane_t frustum[NUM_FRUSTUM_PLANES] );

#endif // TR_MAIN_H_
