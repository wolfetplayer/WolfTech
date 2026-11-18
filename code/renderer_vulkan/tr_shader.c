// tr_shader.c -- trimmed shader-script parser, see tr_shader.h.

#include <string.h>

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

static void ParseStage( shaderStage_t *stage, char **text ) {
	char *token;
	char imageName[MAX_QPATH];
	qboolean hasBlend = qfalse;
	VkBlendFactor srcBlend = VK_BLEND_FACTOR_ONE;
	VkBlendFactor dstBlend = VK_BLEND_FACTOR_ZERO;
	int alphaTestFunc = 0;

	imageName[0] = '\0';

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
			// <frequency> <image1> <image2> ... -- just take the first frame.
			COM_ParseExt( text, qfalse );
			token = COM_ParseExt( text, qfalse );
			Q_strncpyz( imageName, token, sizeof( imageName ) );
			SkipRestOfLine( text );
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
		} else {
			// rgbGen, tcMod, depthFunc, etc. -- parsed-and-discarded, see tr_shader.h.
			SkipRestOfLine( text );
		}
	}

	stage->blendEnabled = hasBlend;
	stage->srcBlend = srcBlend;
	stage->dstBlend = dstBlend;
	stage->alphaTestFunc = alphaTestFunc;

	stage->image = imageName[0] ? R_FindImageFile( imageName, IMGFLAG_NONE ) : NULL;
	if ( !stage->image ) {
		stage->image = R_GetDefaultImage();
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

// qfalse if the shader had no usable stage (e.g. sky/portal-only).
static qboolean ParseShader( const char *name, char **text ) {
	char *token;
	shader_t tempShader;
	shaderStage_t throwaway;
	int rawStageCount = 0;

	memset( &tempShader, 0, sizeof( tempShader ) );
	Q_strncpyz( tempShader.name, name, sizeof( tempShader.name ) );

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
		} else if ( !Q_stricmp( token, "skyparms" ) ) {
			ParseSkyParms( &tempShader, text );
		} else {
			// sort, fogParms, deformVertexes, etc. -- parsed-and-discarded.
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
	if ( hShader < 0 || hShader >= numShaders ) {
		return &shaders[0];
	}
	return &shaders[hShader];
}

shader_t *R_GetDefaultShader( void ) {
	return &shaders[0];
}
