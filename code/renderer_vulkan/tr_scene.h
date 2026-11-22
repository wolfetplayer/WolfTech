// tr_scene.h -- per-frame scene state (entities, dynamic lights, polys), fed to the world+entity draw dispatch in RE_RenderScene.

#ifndef TR_SCENE_H_
#define TR_SCENE_H_

#define MAX_SCENE_POLYS 512
#define MAX_SCENE_POLY_VERTS 4096
#define MAX_SCENE_CORONAS 64

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

// RE_AddCoronaToScene's args, as given; see vk_flares.c for the persistent per-id state (fade, occlusion) built from these.
typedef struct {
	vec3_t origin;
	vec3_t color; // 0..1 per channel, already scaled by the caller
	float scale;
	int id;
	int flags; // bit 0: caller-side visibility gate (e.g. cg_ents.c's "empty corona" fade trick); other bits unused here
} sceneCorona_t;

void R_ClearScene( void );
void R_AddRefEntityToScene( const refEntity_t *ent );
void R_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b );
void RE_AddPolyToScene( qhandle_t hShader, int numVerts, const polyVert_t *verts );
void RE_AddPolysToScene( qhandle_t hShader, int numVerts, const polyVert_t *verts, int numPolys );
void R_AddCoronaToScene( const vec3_t org, float r, float g, float b, float scale, int id, int flags );

// Returns the current frame's entity array (contiguous) and its count via outCount.
const refEntity_t *R_GetSceneEntities( int *outCount );

// Returns the current frame's dynamic-light array (contiguous) and its count via outCount.
const dlight_t *R_GetSceneDlights( int *outCount );

// Returns the current frame's corona array (contiguous) and its count via outCount.
const sceneCorona_t *R_GetSceneCoronas( int *outCount );

// Returns the current frame's poly array (contiguous) and its count via outCount; verts come from R_GetScenePolyVerts.
const scenePoly_t *R_GetScenePolys( int *outCount );
const polyVert_t *R_GetScenePolyVerts( void );

// Windows off entities/dlights/polys/coronas added since the last call, for a frame with multiple RE_RenderScene calls (sky-portal, main, view-weapon). Call right after each one returns.
void R_AdvanceSceneWindow( void );

// Per-frame tracking of which of the sky-portal (RDF_SKYBOXPORTAL/RDF_DRAWSKYBOX) and real scenes rendered first this frame -- some cgame builds call them in either order; see tr_init.c's R_RenderViewInternal.
void R_BeginFrameSkyboxOrder( void ); // call once per real frame, before any RE_RenderScene call
qboolean R_PortalSceneRenderedThisFrame( void );
qboolean R_MainSceneRenderedThisFrame( void );
void R_NotePortalSceneRendered( void );
void R_NoteMainSceneRendered( void );

#endif // TR_SCENE_H_
