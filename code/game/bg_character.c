/*
===========================================================================

Wolf-Tech Source Code
Character definition ( .char ) file parsing, ported from ET: Legacy's
bg_character.c/bg_public.h with the trap_PC_* tokenizer calls replaced by
COM_Parse, since the game (qagame) module in this codebase has no PC_*
trap - only cgame and ui do. This matches the same file-reading + COM_Parse
convention already used by BG_AnimParseAnimConfig in bg_animation.c.

===========================================================================
*/

// bg_character.c -- character (.char) file parsing, shared by game/cgame/ui

#include "../qcommon/q_shared.h"
#include "bg_public.h"

// not declared by any header shared across game/cgame/ui, so declared directly here (see bg_animgroup.c too)
extern int  trap_FS_FOpenFile( const char *qpath, fileHandle_t *f, fsMode_t mode );
extern void trap_FS_Read( void *buffer, int len, fileHandle_t f );
extern void trap_FS_FCloseFile( fileHandle_t f );

#define MAX_CHARACTER_FILE_SIZE 8192

/*
=================
BG_CharParseError
=================
*/
static void QDECL BG_CharParseError( const char *filename, const char *msg, ... ) {
	va_list argptr;
	char text[1024];

	va_start( argptr, msg );
	Q_vsnprintf( text, sizeof( text ), msg, argptr );
	va_end( argptr );

	Com_Error( ERR_DROP, "%s: (%s, line %i)", text, filename, COM_GetCurrentParseLine() + 1 );
}

/*
=============
BG_ParseCharacterFile

Parses a characterDef { ... } block - see e.g.
characters/temperate/allied/soldier.char - into characterDef. Returns
qfalse (without erroring) if the file doesn't exist, so callers can fall
back to the legacy wolfanim.cfg-based model path; any other parse failure
is a hard error, matching G_ParseAnimationFiles' existing convention for
malformed content in a file that IS present.
=============
*/
qboolean BG_ParseCharacterFile( const char *filename, bg_characterDef_t *characterDef ) {
	char text[MAX_CHARACTER_FILE_SIZE];
	char *text_p, *token;
	fileHandle_t f;
	int len;

	len = trap_FS_FOpenFile( filename, &f, FS_READ );
	if ( len <= 0 ) {
		return qfalse;
	}
	if ( len >= sizeof( text ) - 1 ) {
		trap_FS_FCloseFile( f );
		BG_CharParseError( filename, "file too long" );
		return qfalse;
	}
	trap_FS_Read( text, len, f );
	text[len] = 0;
	trap_FS_FCloseFile( f );

	Com_Memset( characterDef, 0, sizeof( *characterDef ) );

	text_p = text;
	COM_BeginParseSession( "BG_ParseCharacterFile" );

	token = COM_Parse( &text_p );
	if ( Q_stricmp( token, "characterDef" ) ) {
		BG_CharParseError( filename, "expected 'characterDef'" );
		return qfalse;
	}

	token = COM_Parse( &text_p );
	if ( Q_stricmp( token, "{" ) ) {
		BG_CharParseError( filename, "expected '{'" );
		return qfalse;
	}

	while ( 1 ) {
		token = COM_Parse( &text_p );
		if ( !token[0] ) {
			break;
		}

		if ( token[0] == '}' ) {
			break;
		}

		if ( !Q_stricmp( token, "mesh" ) ) {
			token = COM_Parse( &text_p );
			Q_strncpyz( characterDef->mesh, token, sizeof( characterDef->mesh ) );
		} else if ( !Q_stricmp( token, "animationgroup" ) ) {
			token = COM_Parse( &text_p );
			Q_strncpyz( characterDef->animationGroup, token, sizeof( characterDef->animationGroup ) );
		} else if ( !Q_stricmp( token, "animationscript" ) ) {
			token = COM_Parse( &text_p );
			Q_strncpyz( characterDef->animationScript, token, sizeof( characterDef->animationScript ) );
		} else if ( !Q_stricmp( token, "skin" ) ) {
			token = COM_Parse( &text_p );
			Q_strncpyz( characterDef->skin, token, sizeof( characterDef->skin ) );
		} else if ( !Q_stricmp( token, "undressedcorpsemodel" ) ) {
			token = COM_Parse( &text_p );
			Q_strncpyz( characterDef->undressedCorpseModel, token, sizeof( characterDef->undressedCorpseModel ) );
		} else if ( !Q_stricmp( token, "undressedcorpseskin" ) ) {
			token = COM_Parse( &text_p );
			Q_strncpyz( characterDef->undressedCorpseSkin, token, sizeof( characterDef->undressedCorpseSkin ) );
		} else if ( !Q_stricmp( token, "hudhead" ) ) {
			token = COM_Parse( &text_p );
			Q_strncpyz( characterDef->hudhead, token, sizeof( characterDef->hudhead ) );
		} else if ( !Q_stricmp( token, "hudheadskin" ) ) {
			token = COM_Parse( &text_p );
			Q_strncpyz( characterDef->hudheadskin, token, sizeof( characterDef->hudheadskin ) );
		} else if ( !Q_stricmp( token, "hudheadanims" ) ) {
			token = COM_Parse( &text_p );
			Q_strncpyz( characterDef->hudheadanims, token, sizeof( characterDef->hudheadanims ) );
		} else {
			BG_CharParseError( filename, "unknown token '%s'", token );
			return qfalse;
		}
	}

	if ( !characterDef->mesh[0] ) {
		BG_CharParseError( filename, "characterDef has no 'mesh'" );
		return qfalse;
	}

	return qtrue;
}
