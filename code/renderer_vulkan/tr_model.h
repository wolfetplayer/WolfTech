// tr_model.h -- model registry: loads .md3/.mdc (frame-morph meshes), .mds (self-contained skeletal mesh), .mdx (skeleton) and .mdm (skeletal mesh + tags); other formats get a placeholder handle.

#ifndef TR_MODEL_H_
#define TR_MODEL_H_

typedef enum {
	MOD_BAD,
	MOD_MD3, // simple per-vertex frame-morph mesh, no bones (weapons/props; some characters)
	MOD_MDC, // MD3 with most frames delta-compressed against periodic base frames; same draw path as MD3
	MOD_MDS, // self-contained skeletal mesh + tags (legacy RTCW SP format; BJ's own player model is this)
	MOD_MDM, // skeletal mesh + tags; bone data comes from a separate MOD_MDX model (refEntity_t.frameModel)
	MOD_MDX, // skeletal data only, no mesh
	MOD_BRUSH, // inline BSP submodel ("*N"); geometry lives in tr_world, see vk_world_draw_brushmodel
	MOD_OTHER // registered, but not a format this renderer loads yet -- no tag/mesh data
} modelType_t;

typedef struct {
	char name[MAX_QPATH];
	modelType_t type;
	md3Header_t *md3;
	mdcHeader_t *mdc;
	mdsHeader_t *mds;
	mdmHeader_t *mdm;
	mdxHeader_t *mdx;
	int bmodelIndex; // MOD_BRUSH only: index into tr_world.submodels
} model_t;

void R_InitModels( void );

qhandle_t RE_RegisterModel( const char *name );
model_t *R_GetModelByHandle( qhandle_t hModel );

#endif // TR_MODEL_H_
