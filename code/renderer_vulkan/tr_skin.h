// tr_skin.h -- .skin file registry: per-surface shader overrides, plus named part-model references (md3_back, etc.).

#ifndef TR_SKIN_H_
#define TR_SKIN_H_

#define MAX_SKIN_SURFACES 256
#define MAX_PART_MODELS 5

typedef struct {
	char name[MAX_QPATH]; // lowercased model surface name this entry overrides
	qhandle_t shader;
} skinSurface_t;

typedef struct {
	char type[MAX_QPATH]; // e.g. "md3_back", "md3_hat"
	char model[MAX_QPATH];
} skinModel_t;

typedef struct {
	char name[MAX_QPATH];
	int numSurfaces;
	skinSurface_t *surfaces;
	int numModels;
	skinModel_t models[MAX_PART_MODELS];
	vec3_t scale;
} skin_t;

void R_InitSkins( void );

qhandle_t RE_RegisterSkin( const char *name );
skin_t *R_GetSkinByHandle( qhandle_t hSkin );

// Looks up type (e.g. "md3_back") among a skin's part-model references; qfalse if not present.
qboolean RE_GetSkinModel( qhandle_t skinid, const char *type, char *outName );

// Shader override for a named model surface, or fallbackShader if the skin has no entry for it.
qhandle_t R_SkinShaderForSurface( const skin_t *skin, const char *surfaceName, qhandle_t fallbackShader );

#endif // TR_SKIN_H_
