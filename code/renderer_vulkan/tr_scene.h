// tr_scene.h -- per-frame scene state (entities, dynamic lights), fed to the world+entity draw dispatch in RE_RenderScene.

#ifndef TR_SCENE_H_
#define TR_SCENE_H_

typedef struct {
	vec3_t origin;
	vec3_t color; // 0..1 per channel
	float radius;
} dlight_t;

void R_ClearScene( void );
void R_AddRefEntityToScene( const refEntity_t *ent );
void R_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b );

// Returns the current frame's entity array (contiguous) and its count via outCount.
const refEntity_t *R_GetSceneEntities( int *outCount );

// Returns the current frame's dynamic-light array (contiguous) and its count via outCount.
const dlight_t *R_GetSceneDlights( int *outCount );

#endif // TR_SCENE_H_
