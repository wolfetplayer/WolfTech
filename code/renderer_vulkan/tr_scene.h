// tr_scene.h -- per-frame scene entity list (RE_ClearScene/RE_AddRefEntityToScene), fed to the world+entity draw dispatch in RE_RenderScene.

#ifndef TR_SCENE_H_
#define TR_SCENE_H_

void R_ClearScene( void );
void R_AddRefEntityToScene( const refEntity_t *ent );

// Returns the current frame's entity array (contiguous) and its count via outCount.
const refEntity_t *R_GetSceneEntities( int *outCount );

#endif // TR_SCENE_H_
