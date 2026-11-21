// tr_model.c -- model registry, see tr_model.h.

#include <string.h>

#include <stdlib.h>

#include "tr_local.h"
#include "tr_shader.h"
#include "tr_model.h"
#include "tr_bsp.h"

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

// Base frames plus optional per-model-frame compressed deltas against the nearest one -- vk_entity.c decodes them at draw time.
static qboolean R_LoadMDC( model_t *mod, void *buffer, const char *name ) {
	mdcHeader_t *mdc;
	md3Frame_t *frame;
	mdcSurface_t *surf;
	md3Shader_t *shader;
	md3Triangle_t *tri;
	md3St_t *st;
	md3XyzNormal_t *xyz;
	mdcXyzCompressed_t *xyzComp;
	short *ps;
	int version, size;
	int i, j;

	version = LittleLong( ( (mdcHeader_t *)buffer )->version );
	if ( version != MDC_VERSION ) {
		ri.Printf( PRINT_WARNING, "R_LoadMDC: %s has wrong version (%i should be %i)\n", name, version, MDC_VERSION );
		return qfalse;
	}

	size = LittleLong( ( (mdcHeader_t *)buffer )->ofsEnd );
	mdc = mod->mdc = ri.Hunk_Alloc( size, h_low );
	memcpy( mdc, buffer, size );

	mdc->ident = LittleLong( mdc->ident );
	mdc->version = LittleLong( mdc->version );
	mdc->numFrames = LittleLong( mdc->numFrames );
	mdc->numTags = LittleLong( mdc->numTags );
	mdc->numSurfaces = LittleLong( mdc->numSurfaces );
	mdc->ofsFrames = LittleLong( mdc->ofsFrames );
	mdc->ofsTagNames = LittleLong( mdc->ofsTagNames );
	mdc->ofsTags = LittleLong( mdc->ofsTags );
	mdc->ofsSurfaces = LittleLong( mdc->ofsSurfaces );
	mdc->ofsEnd = LittleLong( mdc->ofsEnd );
	mdc->flags = LittleLong( mdc->flags );
	mdc->numSkins = LittleLong( mdc->numSkins );

	if ( mdc->numFrames < 1 ) {
		ri.Printf( PRINT_WARNING, "R_LoadMDC: %s has no frames\n", name );
		return qfalse;
	}

	frame = (md3Frame_t *)( (byte *)mdc + mdc->ofsFrames );
	for ( i = 0; i < mdc->numFrames; i++, frame++ ) {
		frame->radius = LittleFloat( frame->radius );
		for ( j = 0; j < 3; j++ ) {
			frame->bounds[0][j] = LittleFloat( frame->bounds[0][j] );
			frame->bounds[1][j] = LittleFloat( frame->bounds[1][j] );
			frame->localOrigin[j] = LittleFloat( frame->localOrigin[j] );
		}
	}

	// mdcTag_t is plain shorts, already little-endian on this platform -- no swap needed.

	surf = (mdcSurface_t *)( (byte *)mdc + mdc->ofsSurfaces );
	for ( i = 0; i < mdc->numSurfaces; i++ ) {
		surf->ident = LittleLong( surf->ident );
		surf->flags = LittleLong( surf->flags );
		surf->numBaseFrames = LittleLong( surf->numBaseFrames );
		surf->numCompFrames = LittleLong( surf->numCompFrames );
		surf->numShaders = LittleLong( surf->numShaders );
		surf->numTriangles = LittleLong( surf->numTriangles );
		surf->ofsTriangles = LittleLong( surf->ofsTriangles );
		surf->numVerts = LittleLong( surf->numVerts );
		surf->ofsShaders = LittleLong( surf->ofsShaders );
		surf->ofsSt = LittleLong( surf->ofsSt );
		surf->ofsXyzNormals = LittleLong( surf->ofsXyzNormals );
		surf->ofsXyzCompressed = LittleLong( surf->ofsXyzCompressed );
		surf->ofsFrameBaseFrames = LittleLong( surf->ofsFrameBaseFrames );
		surf->ofsFrameCompFrames = LittleLong( surf->ofsFrameCompFrames );
		surf->ofsEnd = LittleLong( surf->ofsEnd );

		shader = (md3Shader_t *)( (byte *)surf + surf->ofsShaders );
		for ( j = 0; j < surf->numShaders; j++, shader++ ) {
			shader->shaderIndex = RE_RegisterShaderInternal( shader->name, qfalse );
		}

		tri = (md3Triangle_t *)( (byte *)surf + surf->ofsTriangles );
		for ( j = 0; j < surf->numTriangles; j++, tri++ ) {
			tri->indexes[0] = LittleLong( tri->indexes[0] );
			tri->indexes[1] = LittleLong( tri->indexes[1] );
			tri->indexes[2] = LittleLong( tri->indexes[2] );
		}

		st = (md3St_t *)( (byte *)surf + surf->ofsSt );
		for ( j = 0; j < surf->numVerts; j++, st++ ) {
			st->st[0] = LittleFloat( st->st[0] );
			st->st[1] = LittleFloat( st->st[1] );
		}

		xyz = (md3XyzNormal_t *)( (byte *)surf + surf->ofsXyzNormals );
		for ( j = 0; j < surf->numVerts * surf->numBaseFrames; j++, xyz++ ) {
			xyz->xyz[0] = LittleShort( xyz->xyz[0] );
			xyz->xyz[1] = LittleShort( xyz->xyz[1] );
			xyz->xyz[2] = LittleShort( xyz->xyz[2] );
			xyz->normal = LittleShort( xyz->normal );
		}

		xyzComp = (mdcXyzCompressed_t *)( (byte *)surf + surf->ofsXyzCompressed );
		for ( j = 0; j < surf->numVerts * surf->numCompFrames; j++, xyzComp++ ) {
			xyzComp->ofsVec = (unsigned int)LittleLong( (int)xyzComp->ofsVec );
		}

		ps = (short *)( (byte *)surf + surf->ofsFrameBaseFrames );
		for ( j = 0; j < mdc->numFrames; j++, ps++ ) {
			*ps = LittleShort( *ps );
		}

		ps = (short *)( (byte *)surf + surf->ofsFrameCompFrames );
		for ( j = 0; j < mdc->numFrames; j++, ps++ ) {
			*ps = LittleShort( *ps );
		}

		surf = (mdcSurface_t *)( (byte *)surf + surf->ofsEnd );
	}

	mod->type = MOD_MDC;
	return qtrue;
}

// Full surface/vertex/triangle data -- unlike MDS/MDM, MD3 carries its own per-frame mesh, so this is all drawing needs.
static qboolean R_LoadMD3( model_t *mod, void *buffer, const char *name ) {
	md3Header_t *md3;
	md3Frame_t *frame;
	md3Surface_t *surf;
	md3Shader_t *shader;
	md3Triangle_t *tri;
	md3St_t *st;
	md3XyzNormal_t *xyz;
	md3Tag_t *tag;
	int version, size;
	int i, j;

	version = LittleLong( ( (md3Header_t *)buffer )->version );
	if ( version != MD3_VERSION ) {
		ri.Printf( PRINT_WARNING, "R_LoadMD3: %s has wrong version (%i should be %i)\n", name, version, MD3_VERSION );
		return qfalse;
	}

	size = LittleLong( ( (md3Header_t *)buffer )->ofsEnd );
	md3 = mod->md3 = ri.Hunk_Alloc( size, h_low );
	memcpy( md3, buffer, size );

	md3->ident = LittleLong( md3->ident );
	md3->version = LittleLong( md3->version );
	md3->numFrames = LittleLong( md3->numFrames );
	md3->numTags = LittleLong( md3->numTags );
	md3->numSurfaces = LittleLong( md3->numSurfaces );
	md3->ofsFrames = LittleLong( md3->ofsFrames );
	md3->ofsTags = LittleLong( md3->ofsTags );
	md3->ofsSurfaces = LittleLong( md3->ofsSurfaces );
	md3->ofsEnd = LittleLong( md3->ofsEnd );

	if ( md3->numFrames < 1 ) {
		ri.Printf( PRINT_WARNING, "R_LoadMD3: %s has no frames\n", name );
		return qfalse;
	}

	frame = (md3Frame_t *)( (byte *)md3 + md3->ofsFrames );
	for ( i = 0; i < md3->numFrames; i++, frame++ ) {
		frame->radius = LittleFloat( frame->radius );
		for ( j = 0; j < 3; j++ ) {
			frame->bounds[0][j] = LittleFloat( frame->bounds[0][j] );
			frame->bounds[1][j] = LittleFloat( frame->bounds[1][j] );
			frame->localOrigin[j] = LittleFloat( frame->localOrigin[j] );
		}
	}

	tag = (md3Tag_t *)( (byte *)md3 + md3->ofsTags );
	for ( i = 0; i < md3->numTags * md3->numFrames; i++, tag++ ) {
		for ( j = 0; j < 3; j++ ) {
			tag->origin[j] = LittleFloat( tag->origin[j] );
			tag->axis[0][j] = LittleFloat( tag->axis[0][j] );
			tag->axis[1][j] = LittleFloat( tag->axis[1][j] );
			tag->axis[2][j] = LittleFloat( tag->axis[2][j] );
		}
	}

	surf = (md3Surface_t *)( (byte *)md3 + md3->ofsSurfaces );
	for ( i = 0; i < md3->numSurfaces; i++ ) {
		surf->ident = LittleLong( surf->ident );
		surf->flags = LittleLong( surf->flags );
		surf->numFrames = LittleLong( surf->numFrames );
		surf->numShaders = LittleLong( surf->numShaders );
		surf->numTriangles = LittleLong( surf->numTriangles );
		surf->ofsTriangles = LittleLong( surf->ofsTriangles );
		surf->numVerts = LittleLong( surf->numVerts );
		surf->ofsShaders = LittleLong( surf->ofsShaders );
		surf->ofsSt = LittleLong( surf->ofsSt );
		surf->ofsXyzNormals = LittleLong( surf->ofsXyzNormals );
		surf->ofsEnd = LittleLong( surf->ofsEnd );

		// shaderIndex is repurposed here as the resolved renderer shader handle, not a file-local index.
		shader = (md3Shader_t *)( (byte *)surf + surf->ofsShaders );
		for ( j = 0; j < surf->numShaders; j++, shader++ ) {
			shader->shaderIndex = RE_RegisterShaderInternal( shader->name, qfalse );
		}

		tri = (md3Triangle_t *)( (byte *)surf + surf->ofsTriangles );
		for ( j = 0; j < surf->numTriangles; j++, tri++ ) {
			tri->indexes[0] = LittleLong( tri->indexes[0] );
			tri->indexes[1] = LittleLong( tri->indexes[1] );
			tri->indexes[2] = LittleLong( tri->indexes[2] );
		}

		st = (md3St_t *)( (byte *)surf + surf->ofsSt );
		for ( j = 0; j < surf->numVerts; j++, st++ ) {
			st->st[0] = LittleFloat( st->st[0] );
			st->st[1] = LittleFloat( st->st[1] );
		}

		xyz = (md3XyzNormal_t *)( (byte *)surf + surf->ofsXyzNormals );
		for ( j = 0; j < surf->numVerts * surf->numFrames; j++, xyz++ ) {
			xyz->xyz[0] = LittleShort( xyz->xyz[0] );
			xyz->xyz[1] = LittleShort( xyz->xyz[1] );
			xyz->xyz[2] = LittleShort( xyz->xyz[2] );
			xyz->normal = LittleShort( xyz->normal );
		}

		surf = (md3Surface_t *)( (byte *)surf + surf->ofsEnd );
	}

	mod->type = MOD_MD3;
	return qtrue;
}

static qboolean R_LoadMDS( model_t *mod, void *buffer, const char *name ) {
	mdsHeader_t *mds;
	mdsFrame_t *frame;
	mdsBoneInfo_t *bi;
	mdsTag_t *tag;
	mdsSurface_t *surf;
	mdsTriangle_t *tri;
	mdsVertex_t *v;
	int *boneref;
	int version, size, frameSize;
	int i, j, k;

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

	// Surfaces: mesh + per-vertex bone weights, for vk_entity.c's CPU skinning.
	surf = (mdsSurface_t *)( (byte *)mds + mds->ofsSurfaces );
	for ( i = 0; i < mds->numSurfaces; i++ ) {
		surf->ident = LittleLong( surf->ident );
		surf->minLod = LittleLong( surf->minLod );
		surf->ofsHeader = LittleLong( surf->ofsHeader );
		surf->numVerts = LittleLong( surf->numVerts );
		surf->ofsVerts = LittleLong( surf->ofsVerts );
		surf->numTriangles = LittleLong( surf->numTriangles );
		surf->ofsTriangles = LittleLong( surf->ofsTriangles );
		surf->ofsCollapseMap = LittleLong( surf->ofsCollapseMap );
		surf->numBoneReferences = LittleLong( surf->numBoneReferences );
		surf->ofsBoneReferences = LittleLong( surf->ofsBoneReferences );
		surf->ofsEnd = LittleLong( surf->ofsEnd );

		surf->shaderIndex = RE_RegisterShaderInternal( surf->shader, qfalse );

		tri = (mdsTriangle_t *)( (byte *)surf + surf->ofsTriangles );
		for ( j = 0; j < surf->numTriangles; j++, tri++ ) {
			tri->indexes[0] = LittleLong( tri->indexes[0] );
			tri->indexes[1] = LittleLong( tri->indexes[1] );
			tri->indexes[2] = LittleLong( tri->indexes[2] );
		}

		v = (mdsVertex_t *)( (byte *)surf + surf->ofsVerts );
		for ( j = 0; j < surf->numVerts; j++ ) {
			v->normal[0] = LittleFloat( v->normal[0] );
			v->normal[1] = LittleFloat( v->normal[1] );
			v->normal[2] = LittleFloat( v->normal[2] );
			v->texCoords[0] = LittleFloat( v->texCoords[0] );
			v->texCoords[1] = LittleFloat( v->texCoords[1] );
			v->numWeights = LittleLong( v->numWeights );
			for ( k = 0; k < v->numWeights; k++ ) {
				v->weights[k].boneIndex = LittleLong( v->weights[k].boneIndex );
				v->weights[k].boneWeight = LittleFloat( v->weights[k].boneWeight );
				v->weights[k].offset[0] = LittleFloat( v->weights[k].offset[0] );
				v->weights[k].offset[1] = LittleFloat( v->weights[k].offset[1] );
				v->weights[k].offset[2] = LittleFloat( v->weights[k].offset[2] );
			}
			v = (mdsVertex_t *)&v->weights[v->numWeights];
		}

		boneref = (int *)( (byte *)surf + surf->ofsBoneReferences );
		for ( j = 0; j < surf->numBoneReferences; j++, boneref++ ) {
			*boneref = LittleLong( *boneref );
		}

		surf = (mdsSurface_t *)( (byte *)surf + surf->ofsEnd );
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

static qboolean R_LoadMDM( model_t *mod, void *buffer, const char *name ) {
	mdmHeader_t *mdm;
	mdmTag_t *tag;
	mdmSurface_t *surf;
	mdmTriangle_t *tri;
	mdmVertex_t *v;
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

	// Surfaces: mesh + per-vertex bone weights, for vk_entity.c's CPU skinning.
	surf = (mdmSurface_t *)( (byte *)mdm + mdm->ofsSurfaces );
	for ( i = 0; i < mdm->numSurfaces; i++ ) {
		surf->ident = LittleLong( surf->ident );
		surf->minLod = LittleLong( surf->minLod );
		surf->ofsHeader = LittleLong( surf->ofsHeader );
		surf->numVerts = LittleLong( surf->numVerts );
		surf->ofsVerts = LittleLong( surf->ofsVerts );
		surf->numTriangles = LittleLong( surf->numTriangles );
		surf->ofsTriangles = LittleLong( surf->ofsTriangles );
		surf->ofsCollapseMap = LittleLong( surf->ofsCollapseMap );
		surf->numBoneReferences = LittleLong( surf->numBoneReferences );
		surf->ofsBoneReferences = LittleLong( surf->ofsBoneReferences );
		surf->ofsEnd = LittleLong( surf->ofsEnd );

		surf->shaderIndex = RE_RegisterShaderInternal( surf->shader, qfalse );

		tri = (mdmTriangle_t *)( (byte *)surf + surf->ofsTriangles );
		for ( j = 0; j < surf->numTriangles; j++, tri++ ) {
			tri->indexes[0] = LittleLong( tri->indexes[0] );
			tri->indexes[1] = LittleLong( tri->indexes[1] );
			tri->indexes[2] = LittleLong( tri->indexes[2] );
		}

		v = (mdmVertex_t *)( (byte *)surf + surf->ofsVerts );
		for ( j = 0; j < surf->numVerts; j++ ) {
			v->normal[0] = LittleFloat( v->normal[0] );
			v->normal[1] = LittleFloat( v->normal[1] );
			v->normal[2] = LittleFloat( v->normal[2] );
			v->texCoords[0] = LittleFloat( v->texCoords[0] );
			v->texCoords[1] = LittleFloat( v->texCoords[1] );
			v->numWeights = LittleLong( v->numWeights );
			for ( k = 0; k < v->numWeights; k++ ) {
				v->weights[k].boneIndex = LittleLong( v->weights[k].boneIndex );
				v->weights[k].boneWeight = LittleFloat( v->weights[k].boneWeight );
				v->weights[k].offset[0] = LittleFloat( v->weights[k].offset[0] );
				v->weights[k].offset[1] = LittleFloat( v->weights[k].offset[1] );
				v->weights[k].offset[2] = LittleFloat( v->weights[k].offset[2] );
			}
			v = (mdmVertex_t *)&v->weights[v->numWeights];
		}

		boneref = (int *)( (byte *)surf + surf->ofsBoneReferences );
		for ( j = 0; j < surf->numBoneReferences; j++, boneref++ ) {
			*boneref = LittleLong( *boneref );
		}

		surf = (mdmSurface_t *)( (byte *)surf + surf->ofsEnd );
	}

	mod->type = MOD_MDM;
	return qtrue;
}

// Supported extensions, in the reference engine's fallback preference order (minus iqm/mdr, unsupported here).
static const char *s_modelExts[] = { "mds", "mdm", "mdx", "md3", "mdc" };
static const int s_numModelExts = sizeof( s_modelExts ) / sizeof( s_modelExts[0] );

// Reads and parses filename as the format named by ext; false if missing or unparseable.
static qboolean TryLoadModelFile( model_t *mod, const char *filename, const char *ext ) {
	void *buffer;
	long readLen;
	qboolean loaded = qfalse;

	buffer = NULL;
	readLen = ri.FS_ReadFile( filename, &buffer );
	if ( readLen > 0 && buffer ) {
		if ( !Q_stricmp( ext, "md3" ) || !Q_stricmp( ext, "mdc" ) ) {
			int ident = LittleLong( *(const int *)buffer );
			if ( ident == MD3_IDENT ) {
				loaded = R_LoadMD3( mod, buffer, filename );
			} else if ( ident == MDC_IDENT ) {
				loaded = R_LoadMDC( mod, buffer, filename );
			} else {
				ri.Printf( PRINT_WARNING, "RE_RegisterModel: unknown fileid for '%s'\n", filename );
			}
		} else if ( !Q_stricmp( ext, "mds" ) ) {
			loaded = R_LoadMDS( mod, buffer, filename );
		} else if ( !Q_stricmp( ext, "mdx" ) ) {
			loaded = R_LoadMDX( mod, buffer, filename );
		} else {
			loaded = R_LoadMDM( mod, buffer, filename );
		}
		ri.FS_FreeFile( buffer );
	}
	ri.Printf( PRINT_ALL, "...tr_model checkpoint: RE_RegisterModel('%s') readLen=%ld loaded=%d\n", filename, readLen, (int)loaded );
	return loaded;
}

qhandle_t RE_RegisterModel( const char *name ) {
	model_t *mod;
	const char *ext;
	qhandle_t handle;
	qboolean loaded;
	char localName[MAX_QPATH];

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

	// Inline BSP submodel (func_door/func_rotating/etc.), not a loaded file.
	if ( name[0] == '*' ) {
		int index = atoi( name + 1 );
		if ( index >= 0 && index < tr_world.numSubmodels ) {
			mod->type = MOD_BRUSH;
			mod->bmodelIndex = index;
		} else {
			ri.Printf( PRINT_WARNING, "RE_RegisterModel: invalid inline model '%s' (%d submodels loaded)\n",
				name, tr_world.numSubmodels );
			mod->type = MOD_OTHER;
		}
		return handle;
	}

	ext = COM_GetExtension( name );
	loaded = qfalse;

	if ( ext[0] && ( !Q_stricmp( ext, "md3" ) || !Q_stricmp( ext, "mdc" ) || !Q_stricmp( ext, "mds" ) || !Q_stricmp( ext, "mdx" ) || !Q_stricmp( ext, "mdm" ) ) ) {
		loaded = TryLoadModelFile( mod, name, ext );
	}

	// Named extension missing -- try every other supported one, same fallback the reference engine does.
	if ( !loaded ) {
		int i;
		COM_StripExtension( name, localName, sizeof( localName ) );
		for ( i = 0; i < s_numModelExts; i++ ) {
			char altName[MAX_QPATH];
			if ( ext[0] && !Q_stricmp( ext, s_modelExts[i] ) ) {
				continue; // already tried above
			}
			Com_sprintf( altName, sizeof( altName ), "%s.%s", localName, s_modelExts[i] );
			loaded = TryLoadModelFile( mod, altName, s_modelExts[i] );
			if ( loaded ) {
				break;
			}
		}
	}

	// Any other/failed format: a valid placeholder handle with no tag data, same as before this file existed.
	if ( !loaded ) {
		mod->type = MOD_OTHER;
	}

	return handle;
}
