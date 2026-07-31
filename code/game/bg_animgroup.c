/*
===========================================================================

Wolf-Tech Source Code
Animation group ( .anim ) file parsing, ported from ET: Legacy's
bg_animgroup.c with the trap_PC_* tokenizer replaced by COM_Parse (the
game/qagame module has no PC_* trap - only cgame and ui do), and with
animModelInfo_t->animations[] populated by value instead of through a
shared pointer pool, to match this codebase's existing
animation_t animations[MAX_MODEL_ANIMATIONS] array-of-structs layout
(see BG_AnimParseAnimConfig in bg_animation.c).

Unlike trap_PC_*, COM_Parse has no built-in #include support, but real
.anim content (e.g. animations/human_base.anim) relies on it to pull in
the bulk per-animation frame-range table from a separate .aninc file:

    animfile "animations/human/base/body.mdx"
    {
        #include "animations/human/base/body.aninc"
    }

BG_RAG_GetToken below implements a small include stack so that pattern
works without needing the full PC_* trap.

===========================================================================
*/

// bg_animgroup.c -- animation group (.anim) file parsing, shared by game/cgame/ui

#include "../qcommon/q_shared.h"
#include "bg_public.h"

// not declared by any header shared across game/cgame/ui - see bg_character.c
extern int  trap_FS_FOpenFile( const char *qpath, fileHandle_t *f, fsMode_t mode );
extern void trap_FS_Read( void *buffer, int len, fileHandle_t f );
extern void trap_FS_FCloseFile( fileHandle_t f );

#ifdef CGAMEDLL
qhandle_t trap_R_RegisterModel( const char *name );
#endif

#define MAX_ANIMGROUP_INCLUDE_DEPTH 4
#define MAX_ANIMGROUP_FILE_SIZE     ( 64 * 1024 )

typedef struct {
	char name[MAX_QPATH];
	char *text_p;
	char buffer[MAX_ANIMGROUP_FILE_SIZE];
} animGroupSource_t;

static animGroupSource_t animGroupSources[MAX_ANIMGROUP_INCLUDE_DEPTH];
static int animGroupSourceDepth;

/*
=================
BG_RAG_ParseError
=================
*/
static void QDECL BG_RAG_ParseError( const char *msg, ... ) {
	va_list argptr;
	char text[1024];
	const char *filename;

	va_start( argptr, msg );
	Q_vsnprintf( text, sizeof( text ), msg, argptr );
	va_end( argptr );

	filename = animGroupSourceDepth > 0 ? animGroupSources[animGroupSourceDepth - 1].name : "<unknown>";
	Com_Error( ERR_DROP, "%s: (%s, line %i)", text, filename, COM_GetCurrentParseLine() + 1 );
}

/*
=================
BG_RAG_PushSource

Loads filename onto the include stack as the new current source. Returns
qfalse (no error thrown) if the file can't be opened, so the caller can
report a proper "expected mdx filename"-style error with context.
=================
*/
static qboolean BG_RAG_PushSource( const char *filename ) {
	fileHandle_t f;
	int len;
	animGroupSource_t *src;

	if ( animGroupSourceDepth >= MAX_ANIMGROUP_INCLUDE_DEPTH ) {
		BG_RAG_ParseError( "#include nested too deep (max %i)", MAX_ANIMGROUP_INCLUDE_DEPTH );
		return qfalse;
	}

	len = trap_FS_FOpenFile( filename, &f, FS_READ );
	if ( len <= 0 ) {
		return qfalse;
	}

	src = &animGroupSources[animGroupSourceDepth];
	if ( len >= sizeof( src->buffer ) - 1 ) {
		trap_FS_FCloseFile( f );
		BG_RAG_ParseError( "%s is too long", filename );
		return qfalse;
	}

	trap_FS_Read( src->buffer, len, f );
	trap_FS_FCloseFile( f );
	src->buffer[len] = 0;
	src->text_p = src->buffer;
	Q_strncpyz( src->name, filename, sizeof( src->name ) );

	animGroupSourceDepth++;

	return qtrue;
}

/*
=================
BG_RAG_GetToken

Like COM_Parse, but transparently follows "#include \"file\"" directives
into their own source and pops back to the including file at EOF.
=================
*/
static char *BG_RAG_GetToken( void ) {
	char *token;

	if ( animGroupSourceDepth <= 0 ) {
		return "";
	}

	token = COM_Parse( &animGroupSources[animGroupSourceDepth - 1].text_p );

	if ( !token[0] ) {
		// EOF of this source - pop back to the one that included it, if any
		if ( animGroupSourceDepth > 1 ) {
			animGroupSourceDepth--;
			return BG_RAG_GetToken();
		}
		return token;
	}

	if ( !strcmp( token, "#include" ) ) {
		token = COM_Parse( &animGroupSources[animGroupSourceDepth - 1].text_p );
		if ( !token[0] ) {
			BG_RAG_ParseError( "expected filename after #include" );
			return "";
		}
		if ( !BG_RAG_PushSource( token ) ) {
			BG_RAG_ParseError( "failed to open included file '%s'", token );
			return "";
		}
		return BG_RAG_GetToken();
	}

	return token;
}

/*
=================
BG_RAG_UngetToken

Only valid immediately after a BG_RAG_GetToken() call that didn't cross
an include boundary (i.e. the token came from the currently active
source) - true for every call site below, since the optional trailing
fields they peek at are always on the same line/file as what preceded them.
=================
*/
static void BG_RAG_UngetToken( char *token ) {
	if ( animGroupSourceDepth > 0 && token[0] ) {
		animGroupSources[animGroupSourceDepth - 1].text_p -= strlen( token );
	}
}

/*
=================
BG_RAG_ParseAnimation

format: <name> <firstFrame> <numFrames> <loopFrames> <fps> <moveSpeed> <animBlend> <reversed> [<priority>]

The trailing priority field is optional (some .aninc content predates it) -
if the next token isn't numeric, it's ungotten and priority defaults to 0.
=================
*/
static qboolean BG_RAG_ParseAnimation( animation_t *animation ) {
	char *token;
	float fps;

	animation->flags = 0;
	animation->priority = 0;

	token = BG_RAG_GetToken();
	if ( !token[0] ) {
		BG_RAG_ParseError( "expected first frame integer" );
		return qfalse;
	}
	animation->firstFrame = atoi( token );

	token = BG_RAG_GetToken();
	if ( !token[0] ) {
		BG_RAG_ParseError( "expected length integer" );
		return qfalse;
	}
	animation->numFrames = atoi( token );

	token = BG_RAG_GetToken();
	if ( !token[0] ) {
		BG_RAG_ParseError( "expected looping integer" );
		return qfalse;
	}
	animation->loopFrames = atoi( token );

	token = BG_RAG_GetToken();
	if ( !token[0] ) {
		BG_RAG_ParseError( "expected fps" );
		return qfalse;
	}
	fps = (float)atof( token );
	if ( fps == 0 ) {
		fps = 100;
	}
	animation->frameLerp   = (int)( 1000.0f / fps );
	animation->initialLerp = (int)( 1000.0f / fps );

	token = BG_RAG_GetToken();
	if ( !token[0] ) {
		BG_RAG_ParseError( "expected move speed integer" );
		return qfalse;
	}
	animation->moveSpeed = atoi( token );

	token = BG_RAG_GetToken();
	if ( !token[0] ) {
		BG_RAG_ParseError( "expected transition integer" );
		return qfalse;
	}
	animation->animBlend = atoi( token );

	token = BG_RAG_GetToken();
	if ( !token[0] ) {
		BG_RAG_ParseError( "expected reversed integer" );
		return qfalse;
	}
	if ( atoi( token ) == 1 ) {
		animation->flags |= ANIMFL_REVERSED;
	}

	// optional trailing priority
	token = BG_RAG_GetToken();
	if ( token[0] && ( ( token[0] >= '0' && token[0] <= '9' ) || token[0] == '-' ) ) {
		animation->priority = atoi( token );
	} else {
		BG_RAG_UngetToken( token );
	}

	// calculate the duration
	animation->duration = animation->initialLerp
	                       + animation->frameLerp * animation->numFrames
	                       + animation->animBlend;

	animation->nameHash = BG_StringHashValue( animation->name );

	// hacky-ish stuff, matches ET's BG_RAG_ParseAnimation
	if ( !Q_strncmp( animation->name, "climb", 5 ) ) {
		animation->flags |= ANIMFL_LADDERANIM;
	}
	if ( strstr( animation->name, "firing" ) ) {
		animation->flags      |= ANIMFL_FIRINGANIM;
		animation->initialLerp = 40;
	}

	return qtrue;
}

/*
=================
BG_RAG_ParseAnimFile

Parses one "animfile <mdxpath> { <name> <fields...> ... }" block, appending
each named animation directly into animModelInfo->animations[] by value.
The .mdx is only registered for a real handle in cgame/ui builds - the
game (server) module only needs the frame-timing data for its animation
state machine, never renders anything.
=================
*/
static qboolean BG_RAG_ParseAnimFile( animModelInfo_t *animModelInfo ) {
	char *token;
	qhandle_t mdxFile = 0;
	animation_t *animation;

	token = BG_RAG_GetToken();
	if ( !token[0] ) {
		BG_RAG_ParseError( "expected mdx filename" );
		return qfalse;
	}

#ifdef CGAMEDLL
	mdxFile = trap_R_RegisterModel( token );
	if ( !mdxFile ) {
		BG_RAG_ParseError( "failed to load %s", token );
		return qfalse;
	}
#endif

	token = BG_RAG_GetToken();
	if ( Q_stricmp( token, "{" ) ) {
		BG_RAG_ParseError( "expected '{'" );
		return qfalse;
	}

	while ( 1 ) {
		token = BG_RAG_GetToken();
		if ( !token[0] ) {
			BG_RAG_ParseError( "unexpected EOF" );
			return qfalse;
		}

		if ( token[0] == '}' ) {
			break;
		}

		if ( animModelInfo->numAnimations >= MAX_MODEL_ANIMATIONS ) {
			BG_RAG_ParseError( "out of animation storage space" );
			return qfalse;
		}

		animation = &animModelInfo->animations[animModelInfo->numAnimations];
		Com_Memset( animation, 0, sizeof( *animation ) );
		Q_strncpyz( animation->name, token, sizeof( animation->name ) );
		Q_strlwr( animation->name );
		animation->mdxFile = mdxFile;

		if ( !BG_RAG_ParseAnimation( animation ) ) {
			return qfalse;
		}

		animModelInfo->numAnimations++;
	}

	return qtrue;
}

/*
=================
BG_RegisterAnimationGroup

Parses an "animgroup { animfile ... animfile ... }" file - see e.g.
animations/human_base.anim - populating animModelInfo->animations[].
Returns qfalse (without erroring) if the file doesn't exist, so callers
can fall back to the legacy per-model wolfanim.cfg/.script path.
=================
*/
qboolean BG_RegisterAnimationGroup( const char *filename, animModelInfo_t *animModelInfo ) {
	char *token;

	animModelInfo->numAnimations = 0;
	animModelInfo->footsteps     = FOOTSTEP_NORMAL;
	animModelInfo->gender        = GENDER_MALE;
	animModelInfo->isSkeletal    = qtrue;
	animModelInfo->version       = 3;

	animGroupSourceDepth = 0;
	if ( !BG_RAG_PushSource( filename ) ) {
		return qfalse;
	}

	COM_BeginParseSession( "BG_RegisterAnimationGroup" );

	token = BG_RAG_GetToken();
	if ( Q_stricmp( token, "animgroup" ) ) {
		BG_RAG_ParseError( "expected 'animgroup'" );
		return qfalse;
	}

	token = BG_RAG_GetToken();
	if ( Q_stricmp( token, "{" ) ) {
		BG_RAG_ParseError( "expected '{'" );
		return qfalse;
	}

	while ( 1 ) {
		token = BG_RAG_GetToken();
		if ( !token[0] ) {
			break;
		}

		if ( token[0] == '}' ) {
			break;
		}

		if ( !Q_stricmp( token, "animfile" ) ) {
			if ( !BG_RAG_ParseAnimFile( animModelInfo ) ) {
				return qfalse;
			}
		} else {
			BG_RAG_ParseError( "unknown token '%s'", token );
			return qfalse;
		}
	}

	return qtrue;
}
