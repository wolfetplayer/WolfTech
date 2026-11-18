// tr_model.c -- model registry, see tr_model.h.

#include <string.h>

#include "tr_local.h"
#include "tr_model.h"

#define MAX_MOD_KNOWN 1024
static model_t models[MAX_MOD_KNOWN];
static int numModels;

void R_InitModels( void ) {
	memset( models, 0, sizeof( models ) );
	numModels = 1; // handle 0 means "no model"
}

model_t *R_GetModelByHandle( qhandle_t hModel ) {
	if ( hModel <= 0 || hModel >= numModels ) {
		return NULL;
	}
	return &models[hModel];
}

// Tag data only -- surfaces/mesh are left unswapped in the hunk copy until real mesh rendering lands.
static qboolean R_LoadMDS( model_t *mod, void *buffer, const char *name ) {
	mdsHeader_t *mds;
	mdsFrame_t *frame;
	mdsBoneInfo_t *bi;
	mdsTag_t *tag;
	int version, size, frameSize;
	int i, j;

	version = LittleLong( ( (mdsHeader_t *)buffer )->version );
	if ( version != MDS_VERSION ) {
		ri.Printf( PRINT_WARNING, "R_LoadMDS: %s has wrong version (%i should be %i)\n", name, version, MDS_VERSION );
		return qfalse;
	}

	size = LittleLong( ( (mdsHeader_t *)buffer )->ofsEnd );
	mds = mod->mds = ri.Hunk_Alloc( size, h_low );
	memcpy( mds, buffer, size );

	mds->ident = LittleLong( mds->ident );
	mds->version = LittleLong( mds->version );
	mds->numFrames = LittleLong( mds->numFrames );
	mds->numBones = LittleLong( mds->numBones );
	mds->numTags = LittleLong( mds->numTags );
	mds->numSurfaces = LittleLong( mds->numSurfaces );
	mds->ofsFrames = LittleLong( mds->ofsFrames );
	mds->ofsBones = LittleLong( mds->ofsBones );
	mds->ofsTags = LittleLong( mds->ofsTags );
	mds->ofsSurfaces = LittleLong( mds->ofsSurfaces );
	mds->ofsEnd = LittleLong( mds->ofsEnd );
	mds->lodBias = LittleFloat( mds->lodBias );
	mds->lodScale = LittleFloat( mds->lodScale );
	mds->torsoParent = LittleLong( mds->torsoParent );

	if ( mds->numFrames < 1 ) {
		ri.Printf( PRINT_WARNING, "R_LoadMDS: %s has no frames\n", name );
		return qfalse;
	}

	// frames are mdsFrame_t (its trailing bones[1] already covers one slot) followed by (numBones-1) more.
	frameSize = (int)( sizeof( mdsFrame_t ) - sizeof( mdsBoneFrameCompressed_t ) + mds->numBones * sizeof( mdsBoneFrameCompressed_t ) );

	for ( i = 0; i < mds->numFrames; i++ ) {
		frame = (mdsFrame_t *)( (byte *)mds + mds->ofsFrames + i * frameSize );
		frame->radius = LittleFloat( frame->radius );
		for ( j = 0; j < 3; j++ ) {
			frame->bounds[0][j] = LittleFloat( frame->bounds[0][j] );
			frame->bounds[1][j] = LittleFloat( frame->bounds[1][j] );
			frame->localOrigin[j] = LittleFloat( frame->localOrigin[j] );
			frame->parentOffset[j] = LittleFloat( frame->parentOffset[j] );
		}
		for ( j = 0; j < (int)( mds->numBones * sizeof( mdsBoneFrameCompressed_t ) / sizeof( short ) ); j++ ) {
			( (short *)frame->bones )[j] = LittleShort( ( (short *)frame->bones )[j] );
		}
	}

	tag = (mdsTag_t *)( (byte *)mds + mds->ofsTags );
	for ( i = 0; i < mds->numTags; i++, tag++ ) {
		tag->boneIndex = LittleLong( tag->boneIndex );
		tag->torsoWeight = LittleFloat( tag->torsoWeight );
	}

	for ( i = 0; i < mds->numBones; i++ ) {
		bi = (mdsBoneInfo_t *)( (byte *)mds + mds->ofsBones + i * sizeof( mdsBoneInfo_t ) );
		bi->parent = LittleLong( bi->parent );
		bi->torsoWeight = LittleFloat( bi->torsoWeight );
		bi->parentDist = LittleFloat( bi->parentDist );
		bi->flags = LittleLong( bi->flags );
	}

	mod->type = MOD_MDS;
	return qtrue;
}

static qboolean R_LoadMDX( model_t *mod, void *buffer, const char *name ) {
	mdxHeader_t *mdx;
	mdxFrame_t *frame;
	mdxBoneInfo_t *bi;
	int version, size, frameSize;
	int i, j;
	short *bones;

	version = LittleLong( ( (mdxHeader_t *)buffer )->version );
	if ( version != MDX_VERSION ) {
		ri.Printf( PRINT_WARNING, "R_LoadMDX: %s has wrong version (%i should be %i)\n", name, version, MDX_VERSION );
		return qfalse;
	}

	size = LittleLong( ( (mdxHeader_t *)buffer )->ofsEnd );
	mdx = mod->mdx = ri.Hunk_Alloc( size, h_low );
	memcpy( mdx, buffer, size );

	mdx->ident = LittleLong( mdx->ident );
	mdx->version = LittleLong( mdx->version );
	mdx->numFrames = LittleLong( mdx->numFrames );
	mdx->numBones = LittleLong( mdx->numBones );
	mdx->ofsFrames = LittleLong( mdx->ofsFrames );
	mdx->ofsBones = LittleLong( mdx->ofsBones );
	mdx->ofsEnd = LittleLong( mdx->ofsEnd );
	mdx->torsoParent = LittleLong( mdx->torsoParent );

	if ( mdx->numFrames < 1 ) {
		ri.Printf( PRINT_WARNING, "R_LoadMDX: %s has no frames\n", name );
		return qfalse;
	}

	// frames are (mdxFrame_t + mdxBoneFrameCompressed_t[numBones]) packed back to back.
	frameSize = (int)( sizeof( mdxFrame_t ) + mdx->numBones * sizeof( mdxBoneFrameCompressed_t ) );

	for ( i = 0; i < mdx->numFrames; i++ ) {
		frame = (mdxFrame_t *)( (byte *)mdx + mdx->ofsFrames + i * frameSize );
		frame->radius = LittleFloat( frame->radius );
		for ( j = 0; j < 3; j++ ) {
			frame->bounds[0][j] = LittleFloat( frame->bounds[0][j] );
			frame->bounds[1][j] = LittleFloat( frame->bounds[1][j] );
			frame->localOrigin[j] = LittleFloat( frame->localOrigin[j] );
			frame->parentOffset[j] = LittleFloat( frame->parentOffset[j] );
		}

		bones = (short *)( (byte *)frame + sizeof( mdxFrame_t ) );
		for ( j = 0; j < mdx->numBones * (int)( sizeof( mdxBoneFrameCompressed_t ) / sizeof( short ) ); j++ ) {
			bones[j] = LittleShort( bones[j] );
		}
	}

	for ( i = 0; i < mdx->numBones; i++ ) {
		bi = (mdxBoneInfo_t *)( (byte *)mdx + mdx->ofsBones + i * sizeof( mdxBoneInfo_t ) );
		bi->parent = LittleLong( bi->parent );
		bi->torsoWeight = LittleFloat( bi->torsoWeight );
		bi->parentDist = LittleFloat( bi->parentDist );
		bi->flags = LittleLong( bi->flags );
	}

	mod->type = MOD_MDX;
	return qtrue;
}

// Tag data only -- surfaces/mesh are left unswapped in the hunk copy until real mesh rendering lands.
static qboolean R_LoadMDM( model_t *mod, void *buffer, const char *name ) {
	mdmHeader_t *mdm;
	mdmTag_t *tag;
	int version, size;
	int i, j, k;
	int *boneref;

	version = LittleLong( ( (mdmHeader_t *)buffer )->version );
	if ( version != MDM_VERSION ) {
		ri.Printf( PRINT_WARNING, "R_LoadMDM: %s has wrong version (%i should be %i)\n", name, version, MDM_VERSION );
		return qfalse;
	}

	size = LittleLong( ( (mdmHeader_t *)buffer )->ofsEnd );
	mdm = mod->mdm = ri.Hunk_Alloc( size, h_low );
	memcpy( mdm, buffer, size );

	mdm->ident = LittleLong( mdm->ident );
	mdm->version = LittleLong( mdm->version );
	mdm->numTags = LittleLong( mdm->numTags );
	mdm->numSurfaces = LittleLong( mdm->numSurfaces );
	mdm->ofsTags = LittleLong( mdm->ofsTags );
	mdm->ofsSurfaces = LittleLong( mdm->ofsSurfaces );
	mdm->ofsEnd = LittleLong( mdm->ofsEnd );
	mdm->lodBias = LittleFloat( mdm->lodBias );
	mdm->lodScale = LittleFloat( mdm->lodScale );

	// Tags are variable sized (trailing boneReferences array), walk by ofsEnd.
	tag = (mdmTag_t *)( (byte *)mdm + mdm->ofsTags );
	for ( i = 0; i < mdm->numTags; i++ ) {
		tag->boneIndex = LittleLong( tag->boneIndex );
		for ( j = 0; j < 3; j++ ) {
			tag->offset[j] = LittleFloat( tag->offset[j] );
			for ( k = 0; k < 3; k++ ) {
				tag->axis[j][k] = LittleFloat( tag->axis[j][k] );
			}
		}
		tag->numBoneReferences = LittleLong( tag->numBoneReferences );
		tag->ofsBoneReferences = LittleLong( tag->ofsBoneReferences );
		tag->ofsEnd = LittleLong( tag->ofsEnd );

		boneref = (int *)( (byte *)tag + tag->ofsBoneReferences );
		for ( j = 0; j < tag->numBoneReferences; j++, boneref++ ) {
			*boneref = LittleLong( *boneref );
		}

		tag = (mdmTag_t *)( (byte *)tag + tag->ofsEnd );
	}

	mod->type = MOD_MDM;
	return qtrue;
}

qhandle_t RE_RegisterModel( const char *name ) {
	model_t *mod;
	void *buffer;
	const char *ext;
	qhandle_t handle;
	qboolean loaded;

	if ( !name || !name[0] || strlen( name ) >= MAX_QPATH ) {
		return 0;
	}

	for ( handle = 1; handle < numModels; handle++ ) {
		if ( !Q_stricmp( models[handle].name, name ) ) {
			return handle;
		}
	}

	if ( numModels >= MAX_MOD_KNOWN ) {
		ri.Printf( PRINT_WARNING, "RE_RegisterModel: MAX_MOD_KNOWN hit, dropping '%s'\n", name );
		return 0;
	}

	handle = numModels++;
	mod = &models[handle];
	memset( mod, 0, sizeof( *mod ) );
	Q_strncpyz( mod->name, name, sizeof( mod->name ) );

	ext = COM_GetExtension( name );
	loaded = qfalse;

	if ( !Q_stricmp( ext, "mds" ) || !Q_stricmp( ext, "mdx" ) || !Q_stricmp( ext, "mdm" ) ) {
		long readLen;
		buffer = NULL;
		readLen = ri.FS_ReadFile( name, &buffer );
		if ( readLen > 0 && buffer ) {
			if ( !Q_stricmp( ext, "mds" ) ) {
				loaded = R_LoadMDS( mod, buffer, name );
			} else if ( !Q_stricmp( ext, "mdx" ) ) {
				loaded = R_LoadMDX( mod, buffer, name );
			} else {
				loaded = R_LoadMDM( mod, buffer, name );
			}
			ri.FS_FreeFile( buffer );
		}
		ri.Printf( PRINT_ALL, "...tr_model checkpoint: RE_RegisterModel('%s') readLen=%ld loaded=%d\n", name, readLen, (int)loaded );
	}

	// Any other/failed format: a valid placeholder handle with no tag data, same as before this file existed.
	if ( !loaded ) {
		mod->type = MOD_OTHER;
	}

	return handle;
}
