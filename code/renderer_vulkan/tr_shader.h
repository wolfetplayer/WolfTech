// tr_shader.h -- trimmed shader-script parser; tokenizes the full grammar but only keeps the first stage's image and blendfunc.

#ifndef TR_SHADER_H_
#define TR_SHADER_H_

#include "tr_image.h"

#define MAX_SHADERS     8192

typedef struct shaderStage_s {
	image_t *image;
	VkBlendFactor srcBlend;
	VkBlendFactor dstBlend;
	qboolean blendEnabled;
} shaderStage_t;

typedef struct shader_s {
	char name[MAX_QPATH];
	int index; // handle returned to the game/cgame/ui VMs

	qboolean noMipMaps;
	qboolean noPicMip;
	qboolean cullNone;

	qboolean isSky;
	image_t *skyOuterBox[6]; // rt, lf, bk, ft, up, dn -- see ParseSkyParms

	int numStages;
	shaderStage_t stages[1]; // only stages[0] is ever drawn; extras just parse
} shader_t;

void R_InitShaders( void );
void R_ShutdownShaders( void );

qhandle_t RE_RegisterShaderInternal( const char *name, qboolean noMip );
shader_t *R_GetShaderByHandle( qhandle_t hShader );
shader_t *R_GetDefaultShader( void );

// Wraps an already-built image_t in a plain alpha-blended single-stage shader.
qhandle_t RE_RegisterShaderFromImage( const char *name, image_t *image );

#endif // TR_SHADER_H_
