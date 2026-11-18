// tr_skin.c -- .skin file parsing, see tr_skin.h.

#include <string.h>
#include <stdlib.h>

#include "tr_local.h"
#include "tr_shader.h"
#include "tr_skin.h"

#define MAX_SKINS 1024
static skin_t skins[MAX_SKINS];
static int numSkins;

void R_InitSkins( void ) {
	memset( skins, 0, sizeof( skins ) );
	numSkins = 1; // handle 0 means "no skin"
}

skin_t *R_GetSkinByHandle( qhandle_t hSkin ) {
	if ( hSkin <= 0 || hSkin >= numSkins ) {
		return NULL;
	}
	return &skins[hSkin];
}

// Skin files use comma (not whitespace) to separate a line's two fields; quoted values are also comma-terminated.
static char *CommaParse( char **data_p ) {
	int c = 0, len;
	char *data;
	static char token[MAX_TOKEN_CHARS];

	data = *data_p;
	len = 0;
	token[0] = 0;

	if ( !data ) {
		*data_p = NULL;
		return token;
	}

	while ( 1 ) {
		while ( ( c = *data ) <= ' ' ) {
			if ( !c ) {
				break;
			}
			data++;
		}
		c = *data;

		if ( c == '/' && data[1] == '/' ) {
			data += 2;
			while ( *data && *data != '\n' ) {
				data++;
			}
		} else if ( c == '/' && data[1] == '*' ) {
			data += 2;
			while ( *data && ( *data != '*' || data[1] != '/' ) ) {
				data++;
			}
			if ( *data ) {
				data += 2;
			}
		} else {
			break;
		}
	}

	if ( c == 0 ) {
		*data_p = data;
		return "";
	}

	if ( c == '\"' ) {
		data++;
		while ( 1 ) {
			c = *data++;
			if ( c == '\"' || !c ) {
				token[len] = 0;
				*data_p = data;
				return token;
			}
			if ( len < MAX_TOKEN_CHARS - 1 ) {
				token[len++] = (char)c;
			}
		}
	}

	do {
		if ( len < MAX_TOKEN_CHARS - 1 ) {
			token[len++] = (char)c;
		}
		data++;
		c = *data;
	} while ( c > 32 && c != ',' );

	token[len] = 0;
	*data_p = data;
	return token;
}

qhandle_t RE_RegisterSkin( const char *name ) {
	qhandle_t hSkin;
	skin_t *skin;
	skinSurface_t parseSurfaces[MAX_SKIN_SURFACES];
	void *buffer;
	char *text_p, *token;
	char surfName[MAX_QPATH];
	size_t nameLen;

	if ( !name || !name[0] || strlen( name ) >= MAX_QPATH ) {
		return 0;
	}

	for ( hSkin = 1; hSkin < numSkins; hSkin++ ) {
		if ( !Q_stricmp( skins[hSkin].name, name ) ) {
			return skins[hSkin].numSurfaces ? hSkin : 0; // 0 surfaces means a prior failed parse -- fall back to default
		}
	}

	if ( numSkins >= MAX_SKINS ) {
		ri.Printf( PRINT_WARNING, "RE_RegisterSkin: MAX_SKINS hit, dropping '%s'\n", name );
		return 0;
	}

	hSkin = numSkins++;
	skin = &skins[hSkin];
	memset( skin, 0, sizeof( *skin ) );
	Q_strncpyz( skin->name, name, sizeof( skin->name ) );
	VectorSet( skin->scale, 1.0f, 1.0f, 1.0f );

	nameLen = strlen( name );
	if ( nameLen < 5 || Q_stricmp( name + nameLen - 5, ".skin" ) ) {
		// Not a .skin file: a bare shader/image path naming a single, unconditional override.
		skin->surfaces = ri.Hunk_Alloc( sizeof( skinSurface_t ), h_low );
		skin->surfaces[0].name[0] = '\0';
		skin->surfaces[0].shader = RE_RegisterShaderInternal( name, qfalse );
		skin->numSurfaces = 1;
		return hSkin;
	}

	if ( ri.FS_ReadFile( name, &buffer ) <= 0 || !buffer ) {
		return 0;
	}

	text_p = (char *)buffer;
	while ( text_p && *text_p ) {
		token = CommaParse( &text_p );
		if ( !token[0] ) {
			break;
		}
		Q_strncpyz( surfName, token, sizeof( surfName ) );
		Q_strlwr( surfName );

		if ( strstr( token, "tag_" ) ) {
			continue; // attachment bone reference, not a surface -- no mesh/tag support to wire this into yet
		}

		if ( strstr( token, "md3_" ) ) {
			if ( skin->numModels < MAX_PART_MODELS ) {
				Q_strncpyz( skin->models[skin->numModels].type, token, sizeof( skin->models[0].type ) );
				token = CommaParse( &text_p );
				Q_strncpyz( skin->models[skin->numModels].model, token, sizeof( skin->models[0].model ) );
				skin->numModels++;
			} else {
				ri.Printf( PRINT_WARNING, "RE_RegisterSkin: '%s' has more than %d part models, ignoring the rest\n", name, MAX_PART_MODELS );
			}
			continue;
		}

		if ( !Q_stricmp( token, "playerscale" ) ) {
			token = CommaParse( &text_p );
			skin->scale[0] = skin->scale[1] = skin->scale[2] = (float)atof( token );
			continue;
		}

		token = CommaParse( &text_p );
		if ( skin->numSurfaces < MAX_SKIN_SURFACES ) {
			Q_strncpyz( parseSurfaces[skin->numSurfaces].name, surfName, sizeof( parseSurfaces[0].name ) );
			parseSurfaces[skin->numSurfaces].shader = RE_RegisterShaderInternal( token, qfalse );
			skin->numSurfaces++;
		} else {
			ri.Printf( PRINT_WARNING, "RE_RegisterSkin: '%s' has more than %d surfaces, ignoring the rest\n", name, MAX_SKIN_SURFACES );
		}
	}

	ri.FS_FreeFile( buffer );

	if ( skin->numSurfaces == 0 ) {
		return 0; // caller falls back to the model's own default shaders
	}

	skin->surfaces = ri.Hunk_Alloc( skin->numSurfaces * sizeof( skinSurface_t ), h_low );
	memcpy( skin->surfaces, parseSurfaces, skin->numSurfaces * sizeof( skinSurface_t ) );

	return hSkin;
}

qboolean RE_GetSkinModel( qhandle_t skinid, const char *type, char *outName ) {
	skin_t *skin = R_GetSkinByHandle( skinid );
	int i;

	if ( !skin ) {
		return qfalse;
	}

	if ( !Q_stricmp( type, "playerscale" ) ) {
		Com_sprintf( outName, MAX_QPATH, "%.2f %.2f %.2f", skin->scale[0], skin->scale[1], skin->scale[2] );
		return qtrue;
	}

	for ( i = 0; i < skin->numModels; i++ ) {
		if ( !Q_stricmp( skin->models[i].type, type ) ) {
			Q_strncpyz( outName, skin->models[i].model, MAX_QPATH );
			return qtrue;
		}
	}
	return qfalse;
}

qhandle_t R_SkinShaderForSurface( const skin_t *skin, const char *surfaceName, qhandle_t fallbackShader ) {
	int i;

	if ( !skin || skin->numSurfaces == 0 ) {
		return fallbackShader;
	}

	// A skin registered from a bare shader path (not a .skin file) has one nameless entry that overrides every surface.
	if ( skin->numSurfaces == 1 && skin->surfaces[0].name[0] == '\0' ) {
		return skin->surfaces[0].shader;
	}

	for ( i = 0; i < skin->numSurfaces; i++ ) {
		if ( !Q_stricmp( skin->surfaces[i].name, surfaceName ) ) {
			return skin->surfaces[i].shader;
		}
	}
	return fallbackShader;
}
