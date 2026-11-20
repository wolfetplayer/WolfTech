// tr_scene.h -- per-frame scene state (entities, dynamic lights, polys), fed to the world+entity draw dispatch in RE_RenderScene.

#ifndef TR_SCENE_H_
#define TR_SCENE_H_

#define MAX_SCENE_POLYS 512
#define MAX_SCENE_POLY_VERTS 4096

typedef struct {
	vec3_t origin;
	vec3_t color; // 0..1 per channel
	float radius;
} dlight_t;

// A poly's verts are a slice of R_GetScenePolyVerts(), [firstVert, firstVert+numVerts).
typedef struct {
	qhandle_t shader;
	int firstVert;
	int numVerts;
} scenePoly_t;

void R_ClearScene( void );
void R_AddRefEntityToScene( const refEntity_t *ent );
void R_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b );
void RE_AddPolyToScene( qhandle_t hShader, int numVerts, const polyVert_t *verts );
void RE_AddPolysToScene( qhandle_t hShader, int numVerts, const polyVert_t *verts, int numPolys );

// Returns the current frame's entity array (contiguous) and its count via outCount.
const refEntity_t *R_GetSceneEntities( int *outCount );

// Returns the current frame's dynamic-light array (contiguous) and its count via outCount.
const dlight_t *R_GetSceneDlights( int *outCount );

// Returns the current frame's poly array (contiguous) and its count via outCount; verts come from R_GetScenePolyVerts.
const scenePoly_t *R_GetScenePolys( int *outCount );
const polyVert_t *R_GetScenePolyVerts( void );

// Windows off entities/dlights/polys added since the last call, for a frame with multiple RE_RenderScene calls (sky-portal, main, view-weapon). Call right after each one returns.
void R_AdvanceSceneWindow( void );

// Sky-portal (RDF_SKYBOXPORTAL/RDF_DRAWSKYBOX) tracking -- separate from tr_portal.h's mirror/portal surfaces; see tr_init.c's R_RenderViewInternal.
void R_ResetSkyboxPortal( void ); // call once per RE_LoadWorldMap -- otherwise sticky for the rest of the map
void R_NoteSkyboxPortalScene( int rdflags ); // call once per RE_RenderScene(fd), with fd->rdflags
qboolean R_SkyboxPortalActive( void ); // qtrue once a scene this map has set both RDF_SKYBOXPORTAL and RDF_DRAWSKYBOX

#endif // TR_SCENE_H_
