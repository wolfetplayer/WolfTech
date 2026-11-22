// tr_shader.c -- trimmed shader-script parser, see tr_shader.h.

#include <string.h>
#include <stdlib.h>

#include "tr_local.h"
#include "tr_shader.h"

static shader_t shaders[MAX_SHADERS];
static int numShaders;

// Indexes every top-level "name { ... }" block's position; a shader is only parsed (and its image loaded) on first use.
#define MAX_SHADER_INDEX 32768
typedef struct {
	char name[MAX_QPATH];
	char *textAfterBrace;
} shaderIndexEntry_t;

static shaderIndexEntry_t shaderIndex[MAX_SHADER_INDEX];
static int numShaderIndex;

#define MAX_SHADER_FILES 512
static void *loadedShaderFiles[MAX_SHADER_FILES];
static int numLoadedShaderFiles;

static VkBlendFactor NameToBlendFactor( const char *name, qboolean isSrc ) {
	if ( !Q_stricmp( name, "GL_ONE" ) ) return VK_BLEND_FACTOR_ONE;
	if ( !Q_stricmp( name, "GL_ZERO" ) ) return VK_BLEND_FACTOR_ZERO;
	if ( !Q_stricmp( name, "GL_SRC_ALPHA" ) ) return VK_BLEND_FACTOR_SRC_ALPHA;
	if ( !Q_stricmp( name, "GL_ONE_MINUS_SRC_ALPHA" ) ) return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	if ( !Q_stricmp( name, "GL_SRC_COLOR" ) ) return VK_BLEND_FACTOR_SRC_COLOR;
	if ( !Q_stricmp( name, "GL_ONE_MINUS_SRC_COLOR" ) ) return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
	if ( !Q_stricmp( name, "GL_DST_COLOR" ) ) return VK_BLEND_FACTOR_DST_COLOR;
	if ( !Q_stricmp( name, "GL_ONE_MINUS_DST_COLOR" ) ) return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
	if ( !Q_stricmp( name, "GL_DST_ALPHA" ) ) return VK_BLEND_FACTOR_DST_ALPHA;
	if ( !Q_stricmp( name, "GL_ONE_MINUS_DST_ALPHA" ) ) return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
	if ( !Q_stricmp( name, "GL_SRC_ALPHA_SATURATE" ) ) return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
	return isSrc ? VK_BLEND_FACTOR_ONE : VK_BLEND_FACTOR_ZERO;
}

// Matches single_texture.frag's alpha_test_func encoding.
static int NameToAlphaTestFunc( const char *name ) {
	if ( !Q_stricmp( name, "GT0" ) ) return 1;
	if ( !Q_stricmp( name, "LT128" ) ) return 2;
	if ( !Q_stricmp( name, "GE128" ) ) return 3;
	return 0;
}

static genFunc_t NameToGenFunc( const char *name ) {
	if ( !Q_stricmp( name, "sin" ) ) return GF_SIN;
	if ( !Q_stricmp( name, "square" ) ) return GF_SQUARE;
	if ( !Q_stricmp( name, "triangle" ) ) return GF_TRIANGLE;
	if ( !Q_stricmp( name, "sawtooth" ) ) return GF_SAWTOOTH;
	if ( !Q_stricmp( name, "inversesawtooth" ) ) return GF_INVERSE_SAWTOOTH;
	if ( !Q_stricmp( name, "noise" ) ) return GF_NOISE;
	return GF_NONE;
}

// func base amplitude phase frequency
static void ParseWaveForm( char **text, waveForm_t *wave ) {
	wave->func = NameToGenFunc( COM_ParseExt( text, qfalse ) );
	wave->base = (float)atof( COM_ParseExt( text, qfalse ) );
	wave->amplitude = (float)atof( COM_ParseExt( text, qfalse ) );
	wave->phase = (float)atof( COM_ParseExt( text, qfalse ) );
	wave->frequency = (float)atof( COM_ParseExt( text, qfalse ) );
}

// tcMod turb|scale|scroll|stretch|transform|rotate|entityTranslate <...>; appends to stage->texMods.
static void ParseTexMod( char **text, shaderStage_t *stage ) {
	char *token;
	texModInfo_t *tmi;

	if ( stage->numTexMods >= TR_MAX_TEXMODS ) {
		ri.Printf( PRINT_WARNING, "WARNING: too many tcMod stages\n" );
		SkipRestOfLine( text );
		return;
	}
	tmi = &stage->texMods[stage->numTexMods];

	token = COM_ParseExt( text, qfalse );
	if ( !Q_stricmp( token, "turb" ) ) {
		tmi->wave.base = (float)atof( COM_ParseExt( text, qfalse ) );
		tmi->wave.amplitude = (float)atof( COM_ParseExt( text, qfalse ) );
		tmi->wave.phase = (float)atof( COM_ParseExt( text, qfalse ) );
		tmi->wave.frequency = (float)atof( COM_ParseExt( text, qfalse ) );
		tmi->type = TMOD_TURBULENT;
	} else if ( !Q_stricmp( token, "scale" ) ) {
		tmi->scale[0] = (float)atof( COM_ParseExt( text, qfalse ) );
		tmi->scale[1] = (float)atof( COM_ParseExt( text, qfalse ) );
		tmi->type = TMOD_SCALE;
	} else if ( !Q_stricmp( token, "scroll" ) ) {
		tmi->scroll[0] = (float)atof( COM_ParseExt( text, qfalse ) );
		tmi->scroll[1] = (float)atof( COM_ParseExt( text, qfalse ) );
		tmi->type = TMOD_SCROLL;
	} else if ( !Q_stricmp( token, "stretch" ) ) {
		tmi->wave.func = NameToGenFunc( COM_ParseExt( text, qfalse ) );
		tmi->wave.base = (float)atof( COM_ParseExt( text, qfalse ) );
		tmi->wave.amplitude = (float)atof( COM_ParseExt( text, qfalse ) );
		tmi->wave.phase = (float)atof( COM_ParseExt( text, qfalse ) );
		tmi->wave.frequency = (float)atof( COM_ParseExt( text, qfalse ) );
		tmi->type = TMOD_STRETCH;
	} else if ( !Q_stricmp( token, "transform" ) ) {
		tmi->matrix[0][0] = (float)atof( COM_ParseExt( text, qfalse ) );
		tmi->matrix[0][1] = (float)atof( COM_ParseExt( text, qfalse ) );
		tmi->matrix[1][0] = (float)atof( COM_ParseExt( text, qfalse ) );
		tmi->matrix[1][1] = (float)atof( COM_ParseExt( text, qfalse ) );
		tmi->translate[0] = (float)atof( COM_ParseExt( text, qfalse ) );
		tmi->translate[1] = (float)atof( COM_ParseExt( text, qfalse ) );
		tmi->type = TMOD_TRANSFORM;
	} else if ( !Q_stricmp( token, "rotate" ) ) {
		tmi->rotateSpeed = (float)atof( COM_ParseExt( text, qfalse ) );
		tmi->type = TMOD_ROTATE;
	} else if ( !Q_stricmp( token, "entitytranslate" ) ) {
		tmi->type = TMOD_ENTITY_TRANSLATE;
	} else {
		ri.Printf( PRINT_WARNING, "WARNING: unknown tcMod '%s'\n", token );
		SkipRestOfLine( text );
		return;
	}
	stage->numTexMods++;
	SkipRestOfLine( text );
}

static void ParseStage( shaderStage_t *stage, char **text ) {
	char *token;
	char imageName[MAX_QPATH];
	char animImageNames[MAX_IMAGE_ANIMATIONS][MAX_QPATH];
	int numAnimImageNames = 0;
	float animFrequency = 0.0f;
	qboolean hasBlend = qfalse;
	VkBlendFactor srcBlend = VK_BLEND_FACTOR_ONE;
	VkBlendFactor dstBlend = VK_BLEND_FACTOR_ZERO;
	int alphaTestFunc = 0;
	colorGen_t rgbGen = CGEN_IDENTITY_LIGHTING;
	qboolean rgbGenExplicit = qfalse;
	waveForm_t rgbWave;
	alphaGen_t alphaGen = AGEN_IDENTITY;
	waveForm_t alphaWave;
	byte constantColor[4];

	imageName[0] = '\0';
	memset( &rgbWave, 0, sizeof( rgbWave ) );
	memset( &alphaWave, 0, sizeof( alphaWave ) );
	memset( constantColor, 0, sizeof( constantColor ) );
	memset( stage, 0, sizeof( *stage ) ); // numTexMods must start at 0 -- ParseTexMod writes stage->texMods directly as it parses

	while ( 1 ) {
		token = COM_ParseExt( text, qtrue );
		if ( !token[0] ) {
			ri.Printf( PRINT_WARNING, "WARNING: no concluding '}' in shader stage\n" );
			break;
		}
		if ( token[0] == '}' ) {
			break;
		} else if ( !Q_stricmp( token, "map" ) || !Q_stricmp( token, "clampmap" ) ) {
			token = COM_ParseExt( text, qfalse );
			Q_strncpyz( imageName, token, sizeof( imageName ) );
			SkipRestOfLine( text );
		} else if ( !Q_stricmp( token, "animmap" ) ) {
			// animmap <frequency> <image1> <image2> ...
			token = COM_ParseExt( text, qfalse );
			animFrequency = (float)atof( token );
			numAnimImageNames = 0;
			while ( 1 ) {
				token = COM_ParseExt( text, qfalse );
				if ( !token[0] ) {
					break;
				}
				if ( numAnimImageNames < MAX_IMAGE_ANIMATIONS ) {
					Q_strncpyz( animImageNames[numAnimImageNames], token, sizeof( animImageNames[0] ) );
					numAnimImageNames++;
				}
			}
			if ( numAnimImageNames > 0 ) {
				Q_strncpyz( imageName, animImageNames[0], sizeof( imageName ) );
			}
		} else if ( !Q_stricmp( token, "blendfunc" ) ) {
			token = COM_ParseExt( text, qfalse );
			hasBlend = qtrue;
			if ( !Q_stricmp( token, "add" ) ) {
				srcBlend = VK_BLEND_FACTOR_ONE;
				dstBlend = VK_BLEND_FACTOR_ONE;
			} else if ( !Q_stricmp( token, "blend" ) ) {
				srcBlend = VK_BLEND_FACTOR_SRC_ALPHA;
				dstBlend = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			} else if ( !Q_stricmp( token, "filter" ) ) {
				srcBlend = VK_BLEND_FACTOR_DST_COLOR;
				dstBlend = VK_BLEND_FACTOR_ZERO;
			} else {
				srcBlend = NameToBlendFactor( token, qtrue );
				token = COM_ParseExt( text, qfalse );
				dstBlend = NameToBlendFactor( token, qfalse );
			}
			SkipRestOfLine( text );
		} else if ( !Q_stricmp( token, "alphaFunc" ) ) {
			token = COM_ParseExt( text, qfalse );
			alphaTestFunc = NameToAlphaTestFunc( token );
			SkipRestOfLine( text );
		} else if ( !Q_stricmp( token, "rgbGen" ) ) {
			token = COM_ParseExt( text, qfalse );
			rgbGenExplicit = qtrue;
			if ( !Q_stricmp( token, "wave" ) ) {
				ParseWaveForm( text, &rgbWave );
				rgbGen = CGEN_WAVEFORM;
			} else if ( !Q_stricmp( token, "const" ) ) {
				constantColor[0] = (byte)( 255 * atof( COM_ParseExt( text, qfalse ) ) );
				constantColor[1] = (byte)( 255 * atof( COM_ParseExt( text, qfalse ) ) );
				constantColor[2] = (byte)( 255 * atof( COM_ParseExt( text, qfalse ) ) );
				rgbGen = CGEN_CONST;
			} else if ( !Q_stricmp( token, "identity" ) ) {
				rgbGen = CGEN_IDENTITY;
			} else if ( !Q_stricmp( token, "identitylighting" ) ) {
				rgbGen = CGEN_IDENTITY_LIGHTING;
			} else if ( !Q_stricmp( token, "entity" ) ) {
				rgbGen = CGEN_ENTITY;
			} else if ( !Q_stricmp( token, "oneminusentity" ) ) {
				rgbGen = CGEN_ONE_MINUS_ENTITY;
			} else if ( !Q_stricmp( token, "vertex" ) ) {
				rgbGen = CGEN_VERTEX;
				if ( alphaGen == AGEN_IDENTITY ) {
					alphaGen = AGEN_VERTEX;
				}
			} else if ( !Q_stricmp( token, "exactvertex" ) ) {
				rgbGen = CGEN_EXACT_VERTEX;
			} else if ( !Q_stricmp( token, "lightingdiffuse" ) ) {
				rgbGen = CGEN_LIGHTING_DIFFUSE;
			} else if ( !Q_stricmp( token, "oneminusvertex" ) ) {
				rgbGen = CGEN_ONE_MINUS_VERTEX;
			} else {
				ri.Printf( PRINT_WARNING, "WARNING: unknown rgbGen '%s'\n", token );
			}
			SkipRestOfLine( text );
		} else if ( !Q_stricmp( token, "alphaGen" ) ) {
			token = COM_ParseExt( text, qfalse );
			if ( !Q_stricmp( token, "wave" ) ) {
				ParseWaveForm( text, &alphaWave );
				alphaGen = AGEN_WAVEFORM;
			} else if ( !Q_stricmp( token, "const" ) ) {
				constantColor[3] = (byte)( 255 * atof( COM_ParseExt( text, qfalse ) ) );
				alphaGen = AGEN_CONST;
			} else if ( !Q_stricmp( token, "identity" ) ) {
				alphaGen = AGEN_IDENTITY;
			} else if ( !Q_stricmp( token, "entity" ) ) {
				alphaGen = AGEN_ENTITY;
			} else if ( !Q_stricmp( token, "oneminusentity" ) ) {
				alphaGen = AGEN_ONE_MINUS_ENTITY;
			} else if ( !Q_stricmp( token, "vertex" ) ) {
				alphaGen = AGEN_VERTEX;
			} else if ( !Q_stricmp( token, "lightingspecular" ) ) {
				alphaGen = AGEN_LIGHTING_SPECULAR;
			} else if ( !Q_stricmp( token, "oneminusvertex" ) ) {
				alphaGen = AGEN_ONE_MINUS_VERTEX;
			} else if ( !Q_stricmp( token, "portal" ) ) {
				alphaGen = AGEN_PORTAL;
				COM_ParseExt( text, qfalse ); // range value -- not wired, see the shader-level "portal" keyword (tr_portal.c) for the actual trigger
			} else {
				ri.Printf( PRINT_WARNING, "WARNING: unknown alphaGen '%s'\n", token );
			}
			SkipRestOfLine( text );
		} else if ( !Q_stricmp( token, "tcMod" ) ) {
			ParseTexMod( text, stage );
		} else {
			// tcGen, depthFunc, detail, etc. -- parsed-and-discarded.
			SkipRestOfLine( text );
		}
	}

	stage->blendEnabled = hasBlend;
	stage->srcBlend = srcBlend;
	stage->dstBlend = dstBlend;
	stage->alphaTestFunc = alphaTestFunc;

	// If rgbGen wasn't given explicitly, default depends on blend (matches code/renderer's ParseStage).
	if ( !rgbGenExplicit ) {
		rgbGen = ( !hasBlend || srcBlend == VK_BLEND_FACTOR_ONE || srcBlend == VK_BLEND_FACTOR_SRC_ALPHA ) ?
			CGEN_IDENTITY_LIGHTING : CGEN_IDENTITY;
	}
	if ( alphaGen == AGEN_IDENTITY && ( rgbGen == CGEN_IDENTITY || rgbGen == CGEN_LIGHTING_DIFFUSE ) ) {
		alphaGen = AGEN_SKIP;
	}
	stage->rgbGen = rgbGen;
	stage->rgbWave = rgbWave;
	stage->alphaGen = alphaGen;
	stage->alphaWave = alphaWave;
	memcpy( stage->constantColor, constantColor, 4 );

	stage->image = imageName[0] ? R_FindImageFile( imageName, IMGFLAG_NONE ) : NULL;
	if ( !stage->image ) {
		stage->image = R_GetDefaultImage();
	}

	if ( numAnimImageNames > 0 ) {
		int i;
		stage->animImages[0] = stage->image; // already resolved above, and R_FindImageFile caches by name anyway
		for ( i = 1; i < numAnimImageNames; i++ ) {
			stage->animImages[i] = R_FindImageFile( animImageNames[i], IMGFLAG_NONE );
			if ( !stage->animImages[i] ) {
				stage->animImages[i] = R_GetDefaultImage();
			}
		}
		stage->numAnimImages = numAnimImageNames;
		stage->animFrequency = animFrequency;
	}
}

// skyParms <outerbox> <cloudheight> <innerbox> -- only outerbox is ported (innerbox/cloud layers aren't).
static void ParseSkyParms( shader_t *shader, char **text ) {
	static const char *suf[6] = { "rt", "lf", "bk", "ft", "up", "dn" };
	char pathBase[MAX_QPATH];
	char *token;
	int i;

	token = COM_ParseExt( text, qfalse ); // outerbox
	if ( token[0] && Q_stricmp( token, "-" ) ) {
		Q_strncpyz( pathBase, token, sizeof( pathBase ) );
		for ( i = 0; i < 6; i++ ) {
			char facePath[MAX_QPATH];
			Com_sprintf( facePath, sizeof( facePath ), "%s_%s", pathBase, suf[i] );
			shader->skyOuterBox[i] = R_FindImageFile( facePath, IMGFLAG_CLAMPTOEDGE );
			if ( !shader->skyOuterBox[i] ) {
				shader->skyOuterBox[i] = R_GetDefaultImage();
			}
		}
		shader->isSky = qtrue;
	}

	SkipRestOfLine( text ); // cloudheight, innerbox
}

// deformVertexes wave|normal|bulge|move|autosprite|autosprite2|projectionShadow|text[0-7]; appends to shader->deforms.
static void ParseDeform( shader_t *shader, char **text ) {
	char *token;
	deformStage_t *ds;

	if ( shader->numDeforms >= MAX_SHADER_DEFORMS ) {
		ri.Printf( PRINT_WARNING, "WARNING: MAX_SHADER_DEFORMS hit in shader '%s'\n", shader->name );
		SkipRestOfLine( text );
		return;
	}
	ds = &shader->deforms[shader->numDeforms];

	token = COM_ParseExt( text, qfalse );
	if ( !Q_stricmp( token, "projectionshadow" ) ) {
		ds->deformation = DEFORM_PROJECTION_SHADOW;
	} else if ( !Q_stricmp( token, "autosprite" ) ) {
		ds->deformation = DEFORM_AUTOSPRITE;
	} else if ( !Q_stricmp( token, "autosprite2" ) ) {
		ds->deformation = DEFORM_AUTOSPRITE2;
	} else if ( !Q_stricmpn( token, "text", 4 ) ) {
		ds->deformation = DEFORM_TEXT;
	} else if ( !Q_stricmp( token, "bulge" ) ) {
		ds->bulgeWidth = (float)atof( COM_ParseExt( text, qfalse ) );
		ds->bulgeHeight = (float)atof( COM_ParseExt( text, qfalse ) );
		ds->bulgeSpeed = (float)atof( COM_ParseExt( text, qfalse ) );
		ds->deformation = DEFORM_BULGE;
	} else if ( !Q_stricmp( token, "wave" ) ) {
		float div = (float)atof( COM_ParseExt( text, qfalse ) );
		ds->deformationSpread = ( div != 0.0f ) ? 1.0f / div : 100.0f;
		ParseWaveForm( text, &ds->deformationWave );
		ds->deformation = DEFORM_WAVE;
	} else if ( !Q_stricmp( token, "normal" ) ) {
		ds->deformationWave.amplitude = (float)atof( COM_ParseExt( text, qfalse ) );
		ds->deformationWave.frequency = (float)atof( COM_ParseExt( text, qfalse ) );
		ds->deformation = DEFORM_NORMALS;
	} else if ( !Q_stricmp( token, "move" ) ) {
		int i;
		for ( i = 0; i < 3; i++ ) {
			ds->moveVector[i] = (float)atof( COM_ParseExt( text, qfalse ) );
		}
		ParseWaveForm( text, &ds->deformationWave );
		ds->deformation = DEFORM_MOVE;
	} else {
		ri.Printf( PRINT_WARNING, "WARNING: unknown deformVertexes '%s' in shader '%s'\n", token, shader->name );
		SkipRestOfLine( text );
		return;
	}
	shader->numDeforms++;
	SkipRestOfLine( text );
}

// qfalse if the shader had no usable stage (e.g. sky/portal-only).
static qboolean ParseShader( const char *name, char **text ) {
	char *token;
	shader_t tempShader;
	shaderStage_t throwaway;
	int rawStageCount = 0;

	memset( &tempShader, 0, sizeof( tempShader ) );
	Q_strncpyz( tempShader.name, name, sizeof( tempShader.name ) );
	tempShader.explicitlyDefined = qtrue;

	while ( 1 ) {
		token = COM_ParseExt( text, qtrue );
		if ( !token[0] ) {
			ri.Printf( PRINT_WARNING, "WARNING: no concluding '}' in shader %s\n", name );
			return qfalse;
		}

		if ( token[0] == '}' ) {
			break;
		} else if ( token[0] == '{' ) {
			if ( rawStageCount < MAX_SHADER_STAGES ) {
				ParseStage( &tempShader.stages[rawStageCount], text );
			} else {
				ParseStage( &throwaway, text );
				ri.Printf( PRINT_WARNING, "WARNING: too many stages in shader %s (max is %i)\n", name, MAX_SHADER_STAGES );
			}
			rawStageCount++;
		} else if ( !Q_stricmp( token, "nomipmaps" ) ) {
			tempShader.noMipMaps = qtrue;
		} else if ( !Q_stricmp( token, "nopicmip" ) ) {
			tempShader.noPicMip = qtrue;
		} else if ( !Q_stricmp( token, "cull" ) ) {
			// Default (no keyword) stays CT_FRONT_SIDED.
			token = COM_ParseExt( text, qfalse );
			if ( !Q_stricmp( token, "none" ) || !Q_stricmp( token, "disable" ) || !Q_stricmp( token, "twosided" ) ) {
				tempShader.cullType = CT_TWO_SIDED;
			} else if ( !Q_stricmp( token, "back" ) || !Q_stricmp( token, "backside" ) || !Q_stricmp( token, "backsided" ) ) {
				tempShader.cullType = CT_BACK_SIDED;
			} else if ( token[0] ) {
				ri.Printf( PRINT_WARNING, "WARNING: invalid cull parm '%s' in shader '%s'\n", token, name );
			}
		} else if ( !Q_stricmp( token, "polygonOffset" ) ) {
			tempShader.polygonOffset = qtrue;
		} else if ( !Q_stricmp( token, "skyparms" ) ) {
			ParseSkyParms( &tempShader, text );
		} else if ( !Q_stricmp( token, "deformVertexes" ) ) {
			ParseDeform( &tempShader, text );
		} else if ( !Q_stricmp( token, "portal" ) ) {
			// Bare "portal" keyword -- shorthand for "sort portal", see tr_portal.c.
			tempShader.isPortal = qtrue;
			if ( tempShader.portalRange <= 0.0f ) {
				tempShader.portalRange = 256.0f;
			}
		} else if ( !Q_stricmp( token, "sort" ) ) {
			token = COM_ParseExt( text, qfalse );
			if ( !Q_stricmp( token, "portal" ) ) {
				tempShader.isPortal = qtrue;
				if ( tempShader.portalRange <= 0.0f ) {
					tempShader.portalRange = 256.0f;
				}
			}
			SkipRestOfLine( text );
		} else if ( !Q_stricmp( token, "fogParms" ) ) {
			int i;
			token = COM_ParseExt( text, qfalse ); // opening '('
			for ( i = 0; i < 3; i++ ) {
				token = COM_ParseExt( text, qfalse );
				tempShader.fogParms.color[i] = (float)atof( token );
			}
			token = COM_ParseExt( text, qfalse ); // closing ')'
			token = COM_ParseExt( text, qfalse );
			tempShader.fogParms.depthForOpaque = (float)atof( token );
			SkipRestOfLine( text ); // old gradient-direction parm, unused since RTCW
		} else {
			// other unhandled keywords -- parsed-and-discarded.
			SkipRestOfLine( text );
		}
	}

	if ( rawStageCount == 0 && !tempShader.isSky ) {
		return qfalse;
	}
	if ( rawStageCount == 0 ) {
		tempShader.stages[0].image = R_GetDefaultImage(); // sky-only shader -- stages[0] is unused but kept non-null
		tempShader.numStages = 1;
	} else {
		tempShader.numStages = ( rawStageCount < MAX_SHADER_STAGES ) ? rawStageCount : MAX_SHADER_STAGES;
	}

	if ( numShaders >= MAX_SHADERS ) {
		ri.Printf( PRINT_WARNING, "MAX_SHADERS hit, dropping shader '%s'\n", name );
		return qfalse;
	}

	tempShader.index = numShaders;
	shaders[numShaders++] = tempShader;
	return qtrue;
}

// Walks balanced braces to the matching '}', ignoring contents.
static void SkipBracedBlock( char **text ) {
	char *token;
	int depth = 1;

	while ( depth > 0 ) {
		token = COM_ParseExt( text, qtrue );
		if ( !token[0] ) {
			return; // EOF on a malformed file -- nothing more to skip anyway
		}
		if ( token[0] == '{' ) {
			depth++;
		} else if ( token[0] == '}' ) {
			depth--;
		}
	}
}

static void IndexShaderFile( char *text ) {
	char *p = text;
	char *token;
	char shaderName[MAX_QPATH];

	while ( 1 ) {
		token = COM_ParseExt( &p, qtrue );
		if ( !token[0] ) {
			break; // EOF
		}
		Q_strncpyz( shaderName, token, sizeof( shaderName ) );

		token = COM_ParseExt( &p, qtrue );
		if ( token[0] != '{' ) {
			ri.Printf( PRINT_WARNING, "WARNING: expecting '{', found '%s' for shader '%s'\n", token, shaderName );
			continue;
		}

		if ( numShaderIndex < MAX_SHADER_INDEX ) {
			Q_strncpyz( shaderIndex[numShaderIndex].name, shaderName, sizeof( shaderIndex[numShaderIndex].name ) );
			shaderIndex[numShaderIndex].textAfterBrace = p;
			numShaderIndex++;
		} else {
			ri.Printf( PRINT_WARNING, "MAX_SHADER_INDEX hit, '%s' won't be registerable\n", shaderName );
		}

		SkipBracedBlock( &p );
	}
}

static void ScanAndLoadShaderFiles( void ) {
	char **shaderFiles;
	int numShaderFiles;
	int i;

	shaderFiles = ri.FS_ListFiles( "scripts", ".shader", &numShaderFiles );
	if ( !shaderFiles ) {
		return;
	}

	for ( i = 0; i < numShaderFiles; i++ ) {
		char filename[MAX_QPATH];
		void *buffer;
		long len;

		Com_sprintf( filename, sizeof( filename ), "scripts/%s", shaderFiles[i] );
		len = ri.FS_ReadFile( filename, &buffer );
		if ( len <= 0 || !buffer ) {
			continue;
		}

		if ( numLoadedShaderFiles < MAX_SHADER_FILES ) {
			loadedShaderFiles[numLoadedShaderFiles++] = buffer;
		} else {
			ri.Printf( PRINT_WARNING, "MAX_SHADER_FILES hit, '%s' not indexed\n", filename );
			ri.FS_FreeFile( buffer );
			continue;
		}

		IndexShaderFile( (char *)buffer ); // freed later in R_ShutdownShaders
	}

	ri.FS_FreeFileList( shaderFiles );
}

void R_InitShaders( void ) {
	shader_t *def;

	memset( shaders, 0, sizeof( shaders ) );
	numShaders = 0;
	numShaderIndex = 0;
	numLoadedShaderFiles = 0;

	def = &shaders[numShaders++];
	Q_strncpyz( def->name, "*default", sizeof( def->name ) );
	def->index = 0;
	def->numStages = 1;
	def->stages[0].image = R_GetDefaultImage();
	// alpha-blended, not opaque -- stands in for the engine's "*white" too
	def->stages[0].blendEnabled = qtrue;
	def->stages[0].srcBlend = VK_BLEND_FACTOR_SRC_ALPHA;
	def->stages[0].dstBlend = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

	ScanAndLoadShaderFiles();

	ri.Printf( PRINT_ALL, "Indexed %d shaders from %d files\n", numShaderIndex, numLoadedShaderFiles );
}

void R_ShutdownShaders( void ) {
	int i;

	for ( i = 0; i < numLoadedShaderFiles; i++ ) {
		ri.FS_FreeFile( loadedShaderFiles[i] );
	}
	numLoadedShaderFiles = 0;
	numShaderIndex = 0;
	numShaders = 0;
}

static shader_t *R_FindShaderInList( const char *name ) {
	int i;
	for ( i = 0; i < numShaders; i++ ) {
		if ( !Q_stricmp( shaders[i].name, name ) ) {
			return &shaders[i];
		}
	}
	return NULL;
}

static shaderIndexEntry_t *R_FindShaderIndexEntry( const char *name ) {
	int i;
	for ( i = 0; i < numShaderIndex; i++ ) {
		if ( !Q_stricmp( shaderIndex[i].name, name ) ) {
			return &shaderIndex[i];
		}
	}
	return NULL;
}

static qhandle_t R_CreateSingleStageShader( const char *name, image_t *image );

qhandle_t RE_RegisterShaderInternal( const char *name, qboolean noMip ) {
	shader_t *shader;
	shaderIndexEntry_t *entry;
	image_t *image;
	int imgFlags;

	if ( !name || !name[0] ) {
		return 0;
	}

	if ( name[0] == '*' ) {
		return 0; // procedural shaders (*white, *black, ...) aren't file-backed
	}

	shader = R_FindShaderInList( name );
	if ( shader ) {
		return shader->index;
	}

	// first use of this name: parse it from the index now, lazily
	entry = R_FindShaderIndexEntry( name );
	if ( entry ) {
		char *textCursor = entry->textAfterBrace;
		if ( ParseShader( name, &textCursor ) ) {
			return shaders[numShaders - 1].index;
		}
	}

	// not a .shader script name -- try it directly as an image path
	imgFlags = noMip ? IMGFLAG_NOMIPMAP : IMGFLAG_NONE;
	image = R_FindImageFile( name, imgFlags );
	if ( !image ) {
		ri.Printf( PRINT_WARNING, "WARNING: couldn't find shader or image for '%s'\n", name );
		return 0;
	}

	return R_CreateSingleStageShader( name, image );
}

// Plain alpha-blended single-stage shader wrapping an image_t.
static qhandle_t R_CreateSingleStageShader( const char *name, image_t *image ) {
	shader_t *s;

	if ( numShaders >= MAX_SHADERS ) {
		return 0;
	}

	s = &shaders[numShaders];
	memset( s, 0, sizeof( *s ) );
	Q_strncpyz( s->name, name, sizeof( s->name ) );
	s->index = numShaders;
	s->numStages = 1;
	s->stages[0].image = image;
	s->stages[0].blendEnabled = qtrue;
	s->stages[0].srcBlend = VK_BLEND_FACTOR_SRC_ALPHA;
	s->stages[0].dstBlend = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	numShaders++;

	return s->index;
}

qhandle_t RE_RegisterShaderFromImage( const char *name, image_t *image ) {
	return R_CreateSingleStageShader( name, image );
}

shader_t *R_GetShaderByHandle( qhandle_t hShader ) {
	shader_t *shader;
	int guard;

	if ( hShader < 0 || hShader >= numShaders ) {
		return &shaders[0];
	}

	shader = &shaders[hShader];
	// Bounded, not recursive: a remap cycle (A->B->A) stops here instead of looping forever.
	for ( guard = 0; guard < 8 && shader->remappedShader; guard++ ) {
		shader = &shaders[shader->remappedShader];
	}
	return shader;
}

shader_t *R_GetDefaultShader( void ) {
	return &shaders[0];
}

void R_RemapShader( const char *oldShaderName, const char *newShaderName, const char *offsetTime ) {
	shader_t *oldShader = R_FindShaderInList( oldShaderName );
	qhandle_t newHandle;

	(void)offsetTime;

	if ( !oldShader ) {
		ri.Printf( PRINT_WARNING, "WARNING: R_RemapShader: shader '%s' not found\n", oldShaderName );
		return;
	}

	newHandle = RE_RegisterShaderInternal( newShaderName, qfalse );
	// Remapping a shader to itself clears any existing remap, instead of making remappedShader self-referential.
	oldShader->remappedShader = ( newHandle == oldShader->index ) ? 0 : newHandle;
}
