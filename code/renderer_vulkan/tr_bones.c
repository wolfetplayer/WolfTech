// tr_bones.c -- bone-hierarchy evaluation and tag lookup, ported from the opengl1 renderer's tr_animation.c, tag lookup only (no mesh/vertex skinning yet).

#include <string.h>
#include <math.h>

#include "tr_local.h"
#include "tr_model.h"
#include "tr_bones.h"

static mdxBoneFrame_t bones[MDX_MAX_BONES], rawBones[MDX_MAX_BONES], oldBones[MDX_MAX_BONES];
static char validBones[MDX_MAX_BONES];
static char newBones[MDX_MAX_BONES];
static mdxBoneFrame_t *bonePtr, *parentBone;
static mdxBoneFrameCompressed_t *cBonePtr, *cTBonePtr, *cOldBonePtr, *cOldTBonePtr;
static mdxBoneFrameCompressed_t *cBoneList, *cOldBoneList, *cBoneListTorso, *cOldBoneListTorso;
static mdxBoneInfo_t *boneInfo, *thisBoneInfo, *parentBoneInfo;
static mdxFrame_t *frame, *torsoFrame, *oldFrame, *oldTorsoFrame;
static float frontlerp, backlerp, torsoFrontlerp, torsoBacklerp;
static short *sh, *sh2;
static float *pf;
static vec3_t angles, tangles, torsoParentOffset, torsoAxis[3], tmpAxis[3];
static vec3_t vec, v2, dir, t;
static float diff, a1, a2;
static vec4_t m1[4], m2[4];
static qboolean isTorso, fullTorso;
static refEntity_t lastBoneEntity;

static ID_INLINE void Matrix3Transpose( const vec3_t matrix[3], vec3_t transpose[3] ) {
	int i, j;
	for ( i = 0; i < 3; i++ ) {
		for ( j = 0; j < 3; j++ ) {
			transpose[i][j] = matrix[j][i];
		}
	}
}

static ID_INLINE void LocalMatrixTransformVector( vec3_t in, vec3_t mat[3], vec3_t out ) {
	out[0] = in[0] * mat[0][0] + in[1] * mat[0][1] + in[2] * mat[0][2];
	out[1] = in[0] * mat[1][0] + in[1] * mat[1][1] + in[2] * mat[1][2];
	out[2] = in[0] * mat[2][0] + in[1] * mat[2][1] + in[2] * mat[2][2];
}

static ID_INLINE void LocalScaledMatrixTransformVector( vec3_t in, float s, vec3_t mat[3], vec3_t out ) {
	out[0] = ( 1.0f - s ) * in[0] + s * ( in[0] * mat[0][0] + in[1] * mat[0][1] + in[2] * mat[0][2] );
	out[1] = ( 1.0f - s ) * in[1] + s * ( in[0] * mat[1][0] + in[1] * mat[1][1] + in[2] * mat[1][2] );
	out[2] = ( 1.0f - s ) * in[2] + s * ( in[0] * mat[2][0] + in[1] * mat[2][1] + in[2] * mat[2][2] );
}

static ID_INLINE void LocalAngleVector( vec3_t angles, vec3_t forward ) {
	float a, sp, sy, cp, cy;
	a = angles[YAW] * ( M_PI * 2 / 360 ); sy = sinf( a ); cy = cosf( a );
	a = angles[PITCH] * ( M_PI * 2 / 360 ); sp = sinf( a ); cp = cosf( a );
	forward[0] = cp * cy;
	forward[1] = cp * sy;
	forward[2] = -sp;
}

static ID_INLINE void LocalVectorMA( vec3_t org, float dist, vec3_t vec, vec3_t out ) {
	out[0] = org[0] + dist * vec[0];
	out[1] = org[1] + dist * vec[1];
	out[2] = org[2] + dist * vec[2];
}

#define ANGLES_SHORT_TO_FLOAT( pf, sh ) { *( pf++ ) = SHORT2ANGLE( *( sh++ ) ); *( pf++ ) = SHORT2ANGLE( *( sh++ ) ); *( pf++ ) = SHORT2ANGLE( *( sh++ ) ); }

static ID_INLINE void SLerp_Normal( vec3_t from, vec3_t to, float tt, vec3_t out ) {
	float ft = 1.0f - tt;
	out[0] = from[0] * ft + to[0] * tt;
	out[1] = from[1] * ft + to[1] * tt;
	out[2] = from[2] * ft + to[2] * tt;
	VectorNormalize( out );
}

static ID_INLINE void Matrix4MultiplyInto3x3AndTranslation( vec4_t a[4], vec4_t b[4], vec3_t dst[3], vec3_t t ) {
	dst[0][0] = a[0][0] * b[0][0] + a[0][1] * b[1][0] + a[0][2] * b[2][0] + a[0][3] * b[3][0];
	dst[0][1] = a[0][0] * b[0][1] + a[0][1] * b[1][1] + a[0][2] * b[2][1] + a[0][3] * b[3][1];
	dst[0][2] = a[0][0] * b[0][2] + a[0][1] * b[1][2] + a[0][2] * b[2][2] + a[0][3] * b[3][2];
	t[0]      = a[0][0] * b[0][3] + a[0][1] * b[1][3] + a[0][2] * b[2][3] + a[0][3] * b[3][3];

	dst[1][0] = a[1][0] * b[0][0] + a[1][1] * b[1][0] + a[1][2] * b[2][0] + a[1][3] * b[3][0];
	dst[1][1] = a[1][0] * b[0][1] + a[1][1] * b[1][1] + a[1][2] * b[2][1] + a[1][3] * b[3][1];
	dst[1][2] = a[1][0] * b[0][2] + a[1][1] * b[1][2] + a[1][2] * b[2][2] + a[1][3] * b[3][2];
	t[1]      = a[1][0] * b[0][3] + a[1][1] * b[1][3] + a[1][2] * b[2][3] + a[1][3] * b[3][3];

	dst[2][0] = a[2][0] * b[0][0] + a[2][1] * b[1][0] + a[2][2] * b[2][0] + a[2][3] * b[3][0];
	dst[2][1] = a[2][0] * b[0][1] + a[2][1] * b[1][1] + a[2][2] * b[2][1] + a[2][3] * b[3][1];
	dst[2][2] = a[2][0] * b[0][2] + a[2][1] * b[1][2] + a[2][2] * b[2][2] + a[2][3] * b[3][2];
	t[2]      = a[2][0] * b[0][3] + a[2][1] * b[1][3] + a[2][2] * b[2][3] + a[2][3] * b[3][3];
}

static ID_INLINE void Matrix4FromAxisPlusTranslation( vec3_t axis[3], const vec3_t t, vec4_t dst[4] ) {
	int i, j;
	for ( i = 0; i < 3; i++ ) {
		for ( j = 0; j < 3; j++ ) {
			dst[i][j] = axis[i][j];
		}
		dst[3][i] = 0;
		dst[i][3] = t[i];
	}
	dst[3][3] = 1;
}

static ID_INLINE void Matrix4FromScaledAxisPlusTranslation( vec3_t axis[3], float scale, const vec3_t t, vec4_t dst[4] ) {
	int i, j;
	for ( i = 0; i < 3; i++ ) {
		for ( j = 0; j < 3; j++ ) {
			dst[i][j] = scale * axis[i][j];
			if ( i == j ) {
				dst[i][j] += 1.0f - scale;
			}
		}
		dst[3][i] = 0;
		dst[i][3] = t[i];
	}
	dst[3][3] = 1;
}

// qfalse for an unset or invalid handle -- an MDM entity with no skeleton linked yet is a normal state.
static qboolean R_MDM_ResolveSkeleton( qhandle_t hModel, mdxHeader_t **outHeader ) {
	model_t *mod;

	if ( !hModel ) {
		return qfalse;
	}
	mod = R_GetModelByHandle( hModel );
	if ( !mod || mod->type != MOD_MDX || !mod->mdx ) {
		return qfalse;
	}
	*outHeader = mod->mdx;
	return qtrue;
}

// Single frame, no lerp.
static void R_CalcBone( int torsoParentBoneNum, const refEntity_t *refent, int boneNum ) {
	int j;
	(void)refent;

	thisBoneInfo = &boneInfo[boneNum];
	if ( thisBoneInfo->torsoWeight ) {
		cTBonePtr = &cBoneListTorso[boneNum];
		isTorso = qtrue;
		fullTorso = ( thisBoneInfo->torsoWeight == 1.0f );
	} else {
		isTorso = qfalse;
		fullTorso = qfalse;
	}
	cBonePtr = &cBoneList[boneNum];
	bonePtr = &bones[boneNum];

	if ( thisBoneInfo->parent >= 0 ) {
		parentBone = &bones[thisBoneInfo->parent];
		parentBoneInfo = &boneInfo[thisBoneInfo->parent];
	} else {
		parentBone = NULL;
		parentBoneInfo = NULL;
	}

	// rotation
	if ( fullTorso ) {
		sh = (short *)cTBonePtr->angles;
		pf = angles;
		ANGLES_SHORT_TO_FLOAT( pf, sh );
	} else {
		sh = (short *)cBonePtr->angles;
		pf = angles;
		ANGLES_SHORT_TO_FLOAT( pf, sh );
		if ( isTorso ) {
			sh = (short *)cTBonePtr->angles;
			pf = tangles;
			ANGLES_SHORT_TO_FLOAT( pf, sh );
			for ( j = 0; j < 3; j++ ) {
				diff = tangles[j] - angles[j];
				if ( fabsf( diff ) > 180 ) {
					diff = AngleNormalize180( diff );
				}
				angles[j] = angles[j] + thisBoneInfo->torsoWeight * diff;
			}
		}
	}
	AnglesToAxis( angles, bonePtr->matrix );

	// translation
	if ( parentBone ) {
		if ( fullTorso ) {
			sh = (short *)cTBonePtr->ofsAngles; pf = angles;
			*( pf++ ) = SHORT2ANGLE( *( sh++ ) ); *( pf++ ) = SHORT2ANGLE( *( sh++ ) ); *( pf++ ) = 0;
			LocalAngleVector( angles, vec );
			LocalVectorMA( parentBone->translation, thisBoneInfo->parentDist, vec, bonePtr->translation );
		} else {
			sh = (short *)cBonePtr->ofsAngles; pf = angles;
			*( pf++ ) = SHORT2ANGLE( *( sh++ ) ); *( pf++ ) = SHORT2ANGLE( *( sh++ ) ); *( pf++ ) = 0;
			LocalAngleVector( angles, vec );

			if ( isTorso ) {
				sh = (short *)cTBonePtr->ofsAngles; pf = tangles;
				*( pf++ ) = SHORT2ANGLE( *( sh++ ) ); *( pf++ ) = SHORT2ANGLE( *( sh++ ) ); *( pf++ ) = 0;
				LocalAngleVector( tangles, v2 );
				SLerp_Normal( vec, v2, thisBoneInfo->torsoWeight, vec );
				LocalVectorMA( parentBone->translation, thisBoneInfo->parentDist, vec, bonePtr->translation );
			} else {
				LocalVectorMA( parentBone->translation, thisBoneInfo->parentDist, vec, bonePtr->translation );
			}
		}
	} else {
		VectorCopy( frame->parentOffset, bonePtr->translation );
	}

	if ( boneNum == torsoParentBoneNum ) {
		VectorCopy( bonePtr->translation, torsoParentOffset );
	}

	validBones[boneNum] = 1;
	rawBones[boneNum] = *bonePtr;
	newBones[boneNum] = 1;
}

// Blend of two frames (backlerp/torsoBacklerp), shortest-route angle lerp.
static void R_CalcBoneLerp( int torsoParentBoneNum, const refEntity_t *refent, int boneNum ) {
	int j;
	(void)refent;

	thisBoneInfo = &boneInfo[boneNum];

	if ( thisBoneInfo->parent >= 0 ) {
		parentBone = &bones[thisBoneInfo->parent];
		parentBoneInfo = &boneInfo[thisBoneInfo->parent];
	} else {
		parentBone = NULL;
		parentBoneInfo = NULL;
	}

	if ( thisBoneInfo->torsoWeight ) {
		cTBonePtr = &cBoneListTorso[boneNum];
		cOldTBonePtr = &cOldBoneListTorso[boneNum];
		isTorso = qtrue;
		fullTorso = ( thisBoneInfo->torsoWeight == 1.0f );
	} else {
		isTorso = qfalse;
		fullTorso = qfalse;
	}
	cBonePtr = &cBoneList[boneNum];
	cOldBonePtr = &cOldBoneList[boneNum];
	bonePtr = &bones[boneNum];
	newBones[boneNum] = 1;

	// rotation (shortest route for the 170/-170 wraparound case)
	if ( fullTorso ) {
		sh = (short *)cTBonePtr->angles;
		sh2 = (short *)cOldTBonePtr->angles;
		pf = angles;

		a1 = SHORT2ANGLE( *( sh++ ) ); a2 = SHORT2ANGLE( *( sh2++ ) ); diff = AngleNormalize180( a1 - a2 );
		*( pf++ ) = a1 - torsoBacklerp * diff;
		a1 = SHORT2ANGLE( *( sh++ ) ); a2 = SHORT2ANGLE( *( sh2++ ) ); diff = AngleNormalize180( a1 - a2 );
		*( pf++ ) = a1 - torsoBacklerp * diff;
		a1 = SHORT2ANGLE( *( sh++ ) ); a2 = SHORT2ANGLE( *( sh2++ ) ); diff = AngleNormalize180( a1 - a2 );
		*( pf++ ) = a1 - torsoBacklerp * diff;
	} else {
		sh = (short *)cBonePtr->angles;
		sh2 = (short *)cOldBonePtr->angles;
		pf = angles;

		a1 = SHORT2ANGLE( *( sh++ ) ); a2 = SHORT2ANGLE( *( sh2++ ) ); diff = AngleNormalize180( a1 - a2 );
		*( pf++ ) = a1 - backlerp * diff;
		a1 = SHORT2ANGLE( *( sh++ ) ); a2 = SHORT2ANGLE( *( sh2++ ) ); diff = AngleNormalize180( a1 - a2 );
		*( pf++ ) = a1 - backlerp * diff;
		a1 = SHORT2ANGLE( *( sh++ ) ); a2 = SHORT2ANGLE( *( sh2++ ) ); diff = AngleNormalize180( a1 - a2 );
		*( pf++ ) = a1 - backlerp * diff;

		if ( isTorso ) {
			sh = (short *)cTBonePtr->angles;
			sh2 = (short *)cOldTBonePtr->angles;
			pf = tangles;

			a1 = SHORT2ANGLE( *( sh++ ) ); a2 = SHORT2ANGLE( *( sh2++ ) ); diff = AngleNormalize180( a1 - a2 );
			*( pf++ ) = a1 - torsoBacklerp * diff;
			a1 = SHORT2ANGLE( *( sh++ ) ); a2 = SHORT2ANGLE( *( sh2++ ) ); diff = AngleNormalize180( a1 - a2 );
			*( pf++ ) = a1 - torsoBacklerp * diff;
			a1 = SHORT2ANGLE( *( sh++ ) ); a2 = SHORT2ANGLE( *( sh2++ ) ); diff = AngleNormalize180( a1 - a2 );
			*( pf++ ) = a1 - torsoBacklerp * diff;

			for ( j = 0; j < 3; j++ ) {
				diff = tangles[j] - angles[j];
				if ( fabsf( diff ) > 180 ) {
					diff = AngleNormalize180( diff );
				}
				angles[j] = angles[j] + thisBoneInfo->torsoWeight * diff;
			}
		}
	}
	AnglesToAxis( angles, bonePtr->matrix );

	if ( parentBone ) {
		if ( fullTorso ) {
			sh = (short *)cTBonePtr->ofsAngles;
			sh2 = (short *)cOldTBonePtr->ofsAngles;
		} else {
			sh = (short *)cBonePtr->ofsAngles;
			sh2 = (short *)cOldBonePtr->ofsAngles;
		}

		pf = angles;
		*( pf++ ) = SHORT2ANGLE( *( sh++ ) );
		*( pf++ ) = SHORT2ANGLE( *( sh++ ) );
		*( pf++ ) = 0;
		LocalAngleVector( angles, v2 ); // new

		pf = angles;
		*( pf++ ) = SHORT2ANGLE( *( sh2++ ) );
		*( pf++ ) = SHORT2ANGLE( *( sh2++ ) );
		*( pf++ ) = 0;
		LocalAngleVector( angles, vec ); // old

		if ( fullTorso ) {
			SLerp_Normal( vec, v2, torsoFrontlerp, dir );
		} else {
			SLerp_Normal( vec, v2, frontlerp, dir );
		}

		if ( !fullTorso && isTorso ) { // partial legs/torso, lerp according to torsoWeight too
			sh = (short *)cTBonePtr->ofsAngles;
			sh2 = (short *)cOldTBonePtr->ofsAngles;

			pf = angles;
			*( pf++ ) = SHORT2ANGLE( *( sh++ ) );
			*( pf++ ) = SHORT2ANGLE( *( sh++ ) );
			*( pf++ ) = 0;
			LocalAngleVector( angles, v2 );

			pf = angles;
			*( pf++ ) = SHORT2ANGLE( *( sh2++ ) );
			*( pf++ ) = SHORT2ANGLE( *( sh2++ ) );
			*( pf++ ) = 0;
			LocalAngleVector( angles, vec );

			SLerp_Normal( vec, v2, torsoFrontlerp, v2 );
			SLerp_Normal( dir, v2, thisBoneInfo->torsoWeight, dir );
		}

		LocalVectorMA( parentBone->translation, thisBoneInfo->parentDist, dir, bonePtr->translation );
	} else {
		bonePtr->translation[0] = frontlerp * frame->parentOffset[0] + backlerp * oldFrame->parentOffset[0];
		bonePtr->translation[1] = frontlerp * frame->parentOffset[1] + backlerp * oldFrame->parentOffset[1];
		bonePtr->translation[2] = frontlerp * frame->parentOffset[2] + backlerp * oldFrame->parentOffset[2];
	}

	if ( boneNum == torsoParentBoneNum ) {
		VectorCopy( bonePtr->translation, torsoParentOffset );
	}

	validBones[boneNum] = 1;
	rawBones[boneNum] = *bonePtr;
	newBones[boneNum] = 1;
}

// Second pass: blends each torso-weighted bone's rotation/translation towards the entity's torsoAxis.
static void R_ApplyTorsoBlend( const int *boneList, int numBones, int totalSkeletonBones ) {
	int i;
	const int *boneRefs;
	float torsoWeight = 0;

	boneRefs = boneList;
	for ( i = 0; i < numBones; i++, boneRefs++ ) {
		thisBoneInfo = &boneInfo[*boneRefs];
		bonePtr = &bones[*boneRefs];

		if ( thisBoneInfo->torsoWeight > 0 ) {
			if ( !newBones[*boneRefs] ) {
				bones[*boneRefs] = oldBones[*boneRefs];
				continue;
			}

			if ( !( thisBoneInfo->flags & BONEFLAG_TAG ) ) {
				VectorSubtract( bonePtr->translation, torsoParentOffset, t );
				Matrix4FromAxisPlusTranslation( bonePtr->matrix, t, m1 );
				if ( torsoWeight != thisBoneInfo->torsoWeight ) {
					Matrix4FromScaledAxisPlusTranslation( torsoAxis, thisBoneInfo->torsoWeight, torsoParentOffset, m2 );
					torsoWeight = thisBoneInfo->torsoWeight;
				}
				Matrix4MultiplyInto3x3AndTranslation( m2, m1, bonePtr->matrix, bonePtr->translation );
			} else { // tags rotate/translate directly, no need for a full matrix multiply
				LocalScaledMatrixTransformVector( bonePtr->matrix[0], thisBoneInfo->torsoWeight, torsoAxis, tmpAxis[0] );
				LocalScaledMatrixTransformVector( bonePtr->matrix[1], thisBoneInfo->torsoWeight, torsoAxis, tmpAxis[1] );
				LocalScaledMatrixTransformVector( bonePtr->matrix[2], thisBoneInfo->torsoWeight, torsoAxis, tmpAxis[2] );
				memcpy( bonePtr->matrix, tmpAxis, sizeof( tmpAxis ) );

				VectorSubtract( bonePtr->translation, torsoParentOffset, t );
				LocalScaledMatrixTransformVector( t, thisBoneInfo->torsoWeight, torsoAxis, bonePtr->translation );
				VectorAdd( bonePtr->translation, torsoParentOffset, bonePtr->translation );
			}
		}
	}

	memcpy( oldBones, bones, sizeof( bones[0] ) * totalSkeletonBones );
}

// Computes bones[boneList[0..numBones)] for an MDS model's own embedded skeleton (self-contained, unlike MDM/MDX).
static void R_CalcBones( mdsHeader_t *header, const refEntity_t *refent, const int *boneList, int numBones ) {
	int i;
	const int *boneRefs;
	int frameSize;

	if ( memcmp( &lastBoneEntity, refent, sizeof( refEntity_t ) ) ) {
		memset( validBones, 0, header->numBones );
		lastBoneEntity = *refent;
	}
	memset( newBones, 0, header->numBones );

	if ( refent->oldframe == refent->frame ) {
		backlerp = 0; frontlerp = 1;
	} else {
		backlerp = refent->backlerp; frontlerp = 1.0f - backlerp;
	}
	if ( refent->oldTorsoFrame == refent->torsoFrame ) {
		torsoBacklerp = 0; torsoFrontlerp = 1;
	} else {
		torsoBacklerp = refent->torsoBacklerp; torsoFrontlerp = 1.0f - torsoBacklerp;
	}

	// mdsFrame_t's trailing bones[1] starts at exactly offset sizeof(mdxFrame_t) -- same fixed leading fields, no padding gap.
	frameSize = (int)( sizeof( mdxFrame_t ) + header->numBones * sizeof( mdxBoneFrameCompressed_t ) );

	frame = (mdxFrame_t *)( (byte *)header + header->ofsFrames + refent->frame * frameSize );
	oldFrame = (mdxFrame_t *)( (byte *)header + header->ofsFrames + refent->oldframe * frameSize );
	torsoFrame = (mdxFrame_t *)( (byte *)header + header->ofsFrames + refent->torsoFrame * frameSize );
	oldTorsoFrame = (mdxFrame_t *)( (byte *)header + header->ofsFrames + refent->oldTorsoFrame * frameSize );

	cBoneList = (mdxBoneFrameCompressed_t *)( (byte *)frame + sizeof( mdxFrame_t ) );
	cBoneListTorso = (mdxBoneFrameCompressed_t *)( (byte *)torsoFrame + sizeof( mdxFrame_t ) );

	boneInfo = (mdxBoneInfo_t *)( (byte *)header + header->ofsBones );
	boneRefs = boneList;

	Matrix3Transpose( refent->torsoAxis, torsoAxis );

	if ( !backlerp && !torsoBacklerp ) {
		for ( i = 0; i < numBones; i++, boneRefs++ ) {
			if ( validBones[*boneRefs] ) {
				bones[*boneRefs] = rawBones[*boneRefs];
				continue;
			}
			if ( boneInfo[*boneRefs].parent >= 0 && !validBones[boneInfo[*boneRefs].parent] && !newBones[boneInfo[*boneRefs].parent] ) {
				R_CalcBone( header->torsoParent, refent, boneInfo[*boneRefs].parent );
			}
			R_CalcBone( header->torsoParent, refent, *boneRefs );
		}
	} else {
		cOldBoneList = (mdxBoneFrameCompressed_t *)( (byte *)oldFrame + sizeof( mdxFrame_t ) );
		cOldBoneListTorso = (mdxBoneFrameCompressed_t *)( (byte *)oldTorsoFrame + sizeof( mdxFrame_t ) );

		for ( i = 0; i < numBones; i++, boneRefs++ ) {
			if ( validBones[*boneRefs] ) {
				bones[*boneRefs] = rawBones[*boneRefs];
				continue;
			}
			if ( boneInfo[*boneRefs].parent >= 0 && !validBones[boneInfo[*boneRefs].parent] && !newBones[boneInfo[*boneRefs].parent] ) {
				R_CalcBoneLerp( header->torsoParent, refent, boneInfo[*boneRefs].parent );
			}
			R_CalcBoneLerp( header->torsoParent, refent, *boneRefs );
		}
	}

	R_ApplyTorsoBlend( boneList, numBones, header->numBones );
}

// Builds boneList in root-to-leaf order (each bone's parent is added before it) by walking up from bi.
static void R_RecursiveBoneListAdd( int bi, int *boneList, int *numBones, mdxBoneInfo_t *boneInfoList ) {
	if ( boneInfoList[bi].parent >= 0 ) {
		R_RecursiveBoneListAdd( boneInfoList[bi].parent, boneList, numBones, boneInfoList );
	}
	boneList[(*numBones)++] = bi;
}

// MDS tags are themselves zero-length bones (BONEFLAG_TAG), so the tag's transform is exactly that bone's.
static int R_GetBoneTag( orientation_t *outTag, mdsHeader_t *mds, int startTagIndex, const refEntity_t *refent, const char *tagName ) {
	int i;
	mdsTag_t *pTag;
	mdxBoneInfo_t *boneInfoList;
	int boneList[MDX_MAX_BONES];
	int numBones;

	if ( startTagIndex > mds->numTags ) {
		return -1;
	}

	pTag = (mdsTag_t *)( (byte *)mds + mds->ofsTags );
	pTag += startTagIndex;

	for ( i = startTagIndex; i < mds->numTags; i++, pTag++ ) {
		if ( !strcmp( pTag->name, tagName ) ) {
			break;
		}
	}

	if ( i >= mds->numTags ) {
		ri.Printf( PRINT_ALL, "...tr_bones checkpoint: R_GetBoneTag couldn't find tag '%s' among %d tags\n", tagName, mds->numTags );
		return -1;
	}

	boneInfoList = (mdxBoneInfo_t *)( (byte *)mds + mds->ofsBones );
	numBones = 0;
	R_RecursiveBoneListAdd( pTag->boneIndex, boneList, &numBones, boneInfoList );

	R_CalcBones( mds, refent, boneList, numBones );

	memcpy( outTag->axis, bones[pTag->boneIndex].matrix, sizeof( outTag->axis ) );
	VectorCopy( bones[pTag->boneIndex].translation, outTag->origin );

	return i;
}

// Computes bones[boneList[0..numBones)] for refent's linked MDX skeleton(s) (up to 4: legs/oldlegs/torso/oldtorso, all sharing one bone hierarchy). qfalse if no legs skeleton is linked yet.
static qboolean R_MDM_CalcBones( const refEntity_t *refent, const int *boneList, int numBones ) {
	int i;
	const int *boneRefs;
	mdxHeader_t *legsHeader, *oldLegsHeader, *torsoHeader, *oldTorsoHeader;
	int legsFrameSize, oldLegsFrameSize, torsoFrameSize, oldTorsoFrameSize;

	if ( !R_MDM_ResolveSkeleton( refent->frameModel, &legsHeader ) ) {
		ri.Printf( PRINT_ALL, "...tr_bones checkpoint: R_MDM_CalcBones failed -- frameModel handle %d isn't a resolved MDX skeleton\n", refent->frameModel );
		return qfalse;
	}
	if ( !R_MDM_ResolveSkeleton( refent->oldframeModel, &oldLegsHeader ) ) {
		oldLegsHeader = legsHeader;
	}
	if ( !R_MDM_ResolveSkeleton( refent->torsoFrameModel, &torsoHeader ) ) {
		torsoHeader = legsHeader;
	}
	if ( !R_MDM_ResolveSkeleton( refent->oldTorsoFrameModel, &oldTorsoHeader ) ) {
		oldTorsoHeader = torsoHeader;
	}

	if ( memcmp( &lastBoneEntity, refent, sizeof( refEntity_t ) ) ) {
		memset( validBones, 0, legsHeader->numBones );
		lastBoneEntity = *refent;
	}
	memset( newBones, 0, legsHeader->numBones );

	if ( refent->oldframe == refent->frame && oldLegsHeader == legsHeader ) {
		backlerp = 0; frontlerp = 1;
	} else {
		backlerp = refent->backlerp; frontlerp = 1.0f - backlerp;
	}

	if ( refent->oldTorsoFrame == refent->torsoFrame && oldTorsoHeader == torsoHeader ) {
		torsoBacklerp = 0; torsoFrontlerp = 1;
	} else {
		torsoBacklerp = refent->torsoBacklerp; torsoFrontlerp = 1.0f - torsoBacklerp;
	}

	legsFrameSize = (int)( sizeof( mdxFrame_t ) + legsHeader->numBones * sizeof( mdxBoneFrameCompressed_t ) );
	oldLegsFrameSize = (int)( sizeof( mdxFrame_t ) + oldLegsHeader->numBones * sizeof( mdxBoneFrameCompressed_t ) );
	torsoFrameSize = (int)( sizeof( mdxFrame_t ) + torsoHeader->numBones * sizeof( mdxBoneFrameCompressed_t ) );
	oldTorsoFrameSize = (int)( sizeof( mdxFrame_t ) + oldTorsoHeader->numBones * sizeof( mdxBoneFrameCompressed_t ) );

	frame = (mdxFrame_t *)( (byte *)legsHeader + legsHeader->ofsFrames + refent->frame * legsFrameSize );
	oldFrame = (mdxFrame_t *)( (byte *)oldLegsHeader + oldLegsHeader->ofsFrames + refent->oldframe * oldLegsFrameSize );
	torsoFrame = (mdxFrame_t *)( (byte *)torsoHeader + torsoHeader->ofsFrames + refent->torsoFrame * torsoFrameSize );
	oldTorsoFrame = (mdxFrame_t *)( (byte *)oldTorsoHeader + oldTorsoHeader->ofsFrames + refent->oldTorsoFrame * oldTorsoFrameSize );

	// torsoParent is always the first bone needed in the list
	cBoneList = (mdxBoneFrameCompressed_t *)( (byte *)frame + sizeof( mdxFrame_t ) );
	cBoneListTorso = (mdxBoneFrameCompressed_t *)( (byte *)torsoFrame + sizeof( mdxFrame_t ) );

	boneInfo = (mdxBoneInfo_t *)( (byte *)legsHeader + legsHeader->ofsBones );
	boneRefs = boneList;

	Matrix3Transpose( refent->torsoAxis, torsoAxis );

	if ( !backlerp && !torsoBacklerp ) {
		for ( i = 0; i < numBones; i++, boneRefs++ ) {
			if ( validBones[*boneRefs] ) {
				bones[*boneRefs] = rawBones[*boneRefs];
				continue;
			}
			if ( boneInfo[*boneRefs].parent >= 0 && !validBones[boneInfo[*boneRefs].parent] && !newBones[boneInfo[*boneRefs].parent] ) {
				R_CalcBone( legsHeader->torsoParent, refent, boneInfo[*boneRefs].parent );
			}
			R_CalcBone( legsHeader->torsoParent, refent, *boneRefs );
		}
	} else {
		cOldBoneList = (mdxBoneFrameCompressed_t *)( (byte *)oldFrame + sizeof( mdxFrame_t ) );
		cOldBoneListTorso = (mdxBoneFrameCompressed_t *)( (byte *)oldTorsoFrame + sizeof( mdxFrame_t ) );

		for ( i = 0; i < numBones; i++, boneRefs++ ) {
			if ( validBones[*boneRefs] ) {
				bones[*boneRefs] = rawBones[*boneRefs];
				continue;
			}
			if ( boneInfo[*boneRefs].parent >= 0 && !validBones[boneInfo[*boneRefs].parent] && !newBones[boneInfo[*boneRefs].parent] ) {
				R_CalcBoneLerp( legsHeader->torsoParent, refent, boneInfo[*boneRefs].parent );
			}
			R_CalcBoneLerp( legsHeader->torsoParent, refent, *boneRefs );
		}
	}

	R_ApplyTorsoBlend( boneList, numBones, legsHeader->numBones );

	return qtrue;
}

// mdmTag_t is a fixed local axis/offset relative to an ordinary bone; its bone-reference list (needed to compute that bone) is precomputed in the file.
static int R_MDM_GetTag( orientation_t *outTag, mdmHeader_t *mdm, int startTagIndex, const refEntity_t *refent, const char *tagName ) {
	int i;
	mdmTag_t *pTag;
	int *boneRefs;
	mdxBoneFrame_t *bone;

	if ( startTagIndex > mdm->numTags ) {
		return -1;
	}

	pTag = (mdmTag_t *)( (byte *)mdm + mdm->ofsTags );
	for ( i = 0; i < startTagIndex; i++ ) {
		pTag = (mdmTag_t *)( (byte *)pTag + pTag->ofsEnd );
	}

	for ( ; i < mdm->numTags; i++ ) {
		if ( !strcmp( pTag->name, tagName ) ) {
			break;
		}
		pTag = (mdmTag_t *)( (byte *)pTag + pTag->ofsEnd );
	}

	if ( i >= mdm->numTags ) {
		ri.Printf( PRINT_ALL, "...tr_bones checkpoint: R_MDM_GetTag couldn't find tag '%s' among %d tags\n", tagName, mdm->numTags );
		return -1;
	}

	boneRefs = (int *)( (byte *)pTag + pTag->ofsBoneReferences );
	if ( !R_MDM_CalcBones( refent, boneRefs, pTag->numBoneReferences ) ) {
		return -1;
	}

	bone = &bones[pTag->boneIndex];

	LocalMatrixTransformVector( pTag->axis[0], bone->matrix, outTag->axis[0] );
	LocalMatrixTransformVector( pTag->axis[1], bone->matrix, outTag->axis[1] );
	LocalMatrixTransformVector( pTag->axis[2], bone->matrix, outTag->axis[2] );

	LocalMatrixTransformVector( pTag->offset, bone->matrix, outTag->origin );
	VectorAdd( outTag->origin, bone->translation, outTag->origin );

	return i;
}

// MD3 tags are a flat frame*numTags array (no ofsEnd chaining, no bones) -- just a named per-frame transform.
static int R_MD3GetTag( const md3Header_t *md3, int frameNum, const char *tagName, int startTagIndex, const md3Tag_t **outTag ) {
	const md3Tag_t *tag;
	int i;

	if ( frameNum < 0 || frameNum >= md3->numFrames ) {
		frameNum = md3->numFrames - 1; // bad frame mid-model-change isn't an error
	}
	if ( startTagIndex > md3->numTags ) {
		*outTag = NULL;
		return -1;
	}

	tag = (const md3Tag_t *)( (const byte *)md3 + md3->ofsTags ) + (size_t)frameNum * md3->numTags;
	for ( i = 0; i < md3->numTags; i++, tag++ ) {
		if ( i >= startTagIndex && !strcmp( tag->name, tagName ) ) {
			*outTag = tag;
			return i;
		}
	}

	*outTag = NULL;
	return -1;
}

static int R_MD3LerpTag( orientation_t *outTag, const md3Header_t *md3, int startTagIndex, const refEntity_t *refent, const char *tagName ) {
	const md3Tag_t *start, *end;
	float frontLerp, backLerp;
	int retval, j;

	retval = R_MD3GetTag( md3, refent->oldframe, tagName, startTagIndex, &start );
	R_MD3GetTag( md3, refent->frame, tagName, startTagIndex, &end );

	if ( !start || !end ) {
		return -1;
	}

	frontLerp = 1.0f - refent->backlerp;
	backLerp = refent->backlerp;

	for ( j = 0; j < 3; j++ ) {
		outTag->origin[j] = start->origin[j] * backLerp + end->origin[j] * frontLerp;
		outTag->axis[0][j] = start->axis[0][j] * backLerp + end->axis[0][j] * frontLerp;
		outTag->axis[1][j] = start->axis[1][j] * backLerp + end->axis[1][j] * frontLerp;
		outTag->axis[2][j] = start->axis[2][j] * backLerp + end->axis[2][j] * frontLerp;
	}

	VectorNormalize( outTag->axis[0] );
	VectorNormalize( outTag->axis[1] );
	VectorNormalize( outTag->axis[2] );

	return retval;
}

int R_LerpTag( orientation_t *tag, const refEntity_t *refent, const char *tagNameIn, int startIndex ) {
	model_t *model;

	AxisClear( tag->axis );
	VectorClear( tag->origin );

	model = R_GetModelByHandle( refent->hModel );
	if ( !model ) {
		return -1;
	}

	if ( model->type == MOD_MD3 && model->md3 ) {
		return R_MD3LerpTag( tag, model->md3, startIndex, refent, tagNameIn );
	}
	if ( model->type == MOD_MDS && model->mds ) {
		return R_GetBoneTag( tag, model->mds, startIndex, refent, tagNameIn );
	}
	if ( model->type == MOD_MDM && model->mdm ) {
		return R_MDM_GetTag( tag, model->mdm, startIndex, refent, tagNameIn );
	}

	ri.Printf( PRINT_ALL, "...tr_bones checkpoint: R_LerpTag('%s') hModel=%d -- model %s, type=%d\n",
			   tagNameIn, refent->hModel, model->name, (int)model->type );
	return -1; // not an MD3/MDS/MDM, or tag support for this model type isn't implemented yet
}
