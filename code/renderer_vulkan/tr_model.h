// tr_model.h -- model registry: loads .mds (self-contained skeletal mesh), .mdx (skeleton) and .mdm (skeletal mesh + tags); other formats get a placeholder handle.

#ifndef TR_MODEL_H_
#define TR_MODEL_H_

typedef enum {
	MOD_BAD,
	MOD_MDS, // self-contained skeletal mesh + tags (legacy RTCW SP format; BJ's own player model is this)
	MOD_MDM, // skeletal mesh + tags; bone data comes from a separate MOD_MDX model (refEntity_t.frameModel)
	MOD_MDX, // skeletal data only, no mesh
	MOD_OTHER // registered, but not a format this renderer loads yet (MD3/MDC/...) -- no tag data
} modelType_t;

typedef struct {
	char name[MAX_QPATH];
	modelType_t type;
	mdsHeader_t *mds;
	mdmHeader_t *mdm;
	mdxHeader_t *mdx;
} model_t;

void R_InitModels( void );

qhandle_t RE_RegisterModel( const char *name );
model_t *R_GetModelByHandle( qhandle_t hModel );

#endif // TR_MODEL_H_
