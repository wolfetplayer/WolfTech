// tr_shader.h -- trimmed shader-script parser; see tr_shade_calc.h for what's actually evaluated.

#ifndef TR_SHADER_H_
#define TR_SHADER_H_

#include "tr_image.h"

#define MAX_SHADERS     8192
#define MAX_SHADER_STAGES 8
#define TR_MAX_TEXMODS 4
#define MAX_SHADER_DEFORMS 3
#define MAX_IMAGE_ANIMATIONS 16

// Matches code/renderer/tr_local.h's cullType_t.
typedef enum {
	CT_FRONT_SIDED,
	CT_BACK_SIDED,
	CT_TWO_SIDED
} cullType_t;

// Matches vkQuake3's genFunc_t. GF_NOISE has no real Perlin noise backing it here -- see tr_shade_calc.c.
typedef enum {
	GF_NONE,
	GF_SIN,
	GF_SQUARE,
	GF_TRIANGLE,
	GF_SAWTOOTH,
	GF_INVERSE_SAWTOOTH,
	GF_NOISE
} genFunc_t;

typedef struct {
	genFunc_t func;
	float base, amplitude, phase, frequency;
} waveForm_t;

// Matches vkQuake3's colorGen_t/alphaGen_t; some values are fallbacks, see tr_shade_calc.c.
typedef enum {
	CGEN_IDENTITY_LIGHTING,
	CGEN_IDENTITY,
	CGEN_ENTITY,
	CGEN_ONE_MINUS_ENTITY,
	CGEN_EXACT_VERTEX,
	CGEN_VERTEX,
	CGEN_ONE_MINUS_VERTEX,
	CGEN_WAVEFORM,
	CGEN_LIGHTING_DIFFUSE,
	CGEN_FOG,
	CGEN_CONST
} colorGen_t;

typedef enum {
	AGEN_IDENTITY,
	AGEN_SKIP,
	AGEN_ENTITY,
	AGEN_ONE_MINUS_ENTITY,
	AGEN_VERTEX,
	AGEN_ONE_MINUS_VERTEX,
	AGEN_LIGHTING_SPECULAR,
	AGEN_WAVEFORM,
	AGEN_PORTAL,
	AGEN_CONST
} alphaGen_t;

typedef enum {
	TMOD_NONE,
	TMOD_TRANSFORM,
	TMOD_TURBULENT,
	TMOD_SCROLL,
	TMOD_SCALE,
	TMOD_STRETCH,
	TMOD_ROTATE,
	TMOD_ENTITY_TRANSLATE // scrolls by the entity's refEntity_t.shaderTexCoord; see tr_shade_calc.c
} texMod_t;

typedef enum {
	TCGEN_BASE,
	TCGEN_ENVIRONMENT
} texCoordGen_t;

typedef struct {
	texMod_t type;
	waveForm_t wave;	// TMOD_TURBULENT, TMOD_STRETCH
	float matrix[2][2];	// TMOD_TRANSFORM: s' = s*m[0][0] + t*m[1][0] + translate[0]
	float translate[2];
	float scale[2];		// TMOD_SCALE
	float scroll[2];	// TMOD_SCROLL
	float rotateSpeed;	// TMOD_ROTATE
} texModInfo_t;

// Matches vkQuake3's deform_t; MOVE/WAVE/BULGE are evaluated (WAVE/BULGE only where a per-vertex normal is available -- see R_DeformPosition), the rest pass through. TEXT0-7 collapse to one value.
typedef enum {
	DEFORM_NONE,
	DEFORM_WAVE,
	DEFORM_NORMALS,
	DEFORM_BULGE,
	DEFORM_MOVE,
	DEFORM_PROJECTION_SHADOW,
	DEFORM_AUTOSPRITE,
	DEFORM_AUTOSPRITE2,
	DEFORM_TEXT
} deform_t;

typedef struct {
	deform_t deformation;
	vec3_t moveVector;		// DEFORM_MOVE
	waveForm_t deformationWave;	// DEFORM_WAVE, DEFORM_MOVE, DEFORM_NORMALS
	float deformationSpread;	// DEFORM_WAVE
	float bulgeWidth, bulgeHeight, bulgeSpeed; // DEFORM_BULGE
} deformStage_t;

typedef struct shaderStage_s {
	image_t *image; // frame 0; the only frame when numAnimImages <= 1

	// animmap: cycles through animImages at animFrequency Hz. See R_GetStageImage.
	int numAnimImages;
	image_t *animImages[MAX_IMAGE_ANIMATIONS];
	float animFrequency;

	VkBlendFactor srcBlend;
	VkBlendFactor dstBlend;
	qboolean blendEnabled;
	int alphaTestFunc; // 0=none, 1=GT0, 2=LT128, 3=GE128
	qboolean depthWrite;

	colorGen_t rgbGen;
	waveForm_t rgbWave;
	alphaGen_t alphaGen;
	waveForm_t alphaWave;
	byte constantColor[4]; // CGEN_CONST (rgb) / AGEN_CONST (alpha)

	int numTexMods;
	texModInfo_t texMods[TR_MAX_TEXMODS];

	texCoordGen_t tcGen; // TCGEN_BASE (0) by default -- see ParseStage's memset

	qboolean isLightmap;
} shaderStage_t;


typedef struct {
	vec3_t color;
	float depthForOpaque;
} fogParms_t;

typedef struct shader_s {
	char name[MAX_QPATH];
	int index; // handle returned to the game/cgame/ui VMs

	qboolean noMipMaps;
	qboolean noPicMip;
	qboolean explicitlyDefined;
	cullType_t cullType;
	qboolean polygonOffset; // decal shaders set this to avoid z-fighting with coplanar world geometry; see vk_getPipeline3D

	qboolean isSky;
	image_t *skyOuterBox[6]; // rt, lf, bk, ft, up, dn -- see ParseSkyParms
	float skyCloudHeight; // skyparms' cloudheight param ("-" leaves this 0); gates cloud-layer stage rendering, see vk_sky.c

	qboolean isPortal;   // "portal" or "sort portal" keyword -- triggers mirror/portal-surface recursion, see tr_portal.c
	float portalRange;   // distance (world units) the portal entity may sit from this surface's plane; see tr_portal.c

	fogParms_t fogParms;

	vec4_t distanceCull;

	int numStages;
	shaderStage_t stages[MAX_SHADER_STAGES];

	int numDeforms;
	deformStage_t deforms[MAX_SHADER_DEFORMS];

	qhandle_t remappedShader; // set by R_RemapShader; 0 means "no remap" (ambiguous with a remap to *default itself)
} shader_t;

void R_InitShaders( void );
void R_ShutdownShaders( void );

qhandle_t RE_RegisterShaderInternal( const char *name, qboolean noMip );
shader_t *R_GetShaderByHandle( qhandle_t hShader );
shader_t *R_GetDefaultShader( void );

// Wraps an already-built image_t in a plain alpha-blended single-stage shader.
qhandle_t RE_RegisterShaderFromImage( const char *name, image_t *image );

// R_GetShaderByHandle shows newShaderName's appearance for oldShaderName until remapped again; offsetTime is unused.
void R_RemapShader( const char *oldShaderName, const char *newShaderName, const char *offsetTime );

#endif // TR_SHADER_H_
