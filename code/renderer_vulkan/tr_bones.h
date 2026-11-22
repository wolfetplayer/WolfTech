// tr_bones.h -- bone-hierarchy evaluation, tag lookup, and vertex skinning for MOD_MDS/MOD_MDM/MOD_MDX skeletal models.

#ifndef TR_BONES_H_
#define TR_BONES_H_

int R_LerpTag( orientation_t *tag, const refEntity_t *refent, const char *tagNameIn, int startIndex );

// Blends a surface's vertices by their bone weights into outPos (model-local, caller-sized to surf->numVerts). MDM falls back to bind pose if no skeleton is linked yet.
void R_SkinMDSSurface( const mdsHeader_t *mds, const mdsSurface_t *surf, const refEntity_t *refent, vec3_t *outPos );
void R_SkinMDMSurface( const mdmSurface_t *surf, const refEntity_t *refent, vec3_t *outPos );

// Every one of refent's bones (not one surface's referenced subset) as GLSL-ready column-major mat4s, for vk_entity.c's GPU bone-skinning path; see vk_entitySkinnedVert_t's boneIndex, which indexes these directly. MDM's qfalse means no MDX skeleton is linked yet -- same meaning as the internal R_MDM_CalcBones this wraps.
void R_CalcAllBoneMatricesMDS( const mdsHeader_t *mds, const refEntity_t *refent, float outMat4[MDX_MAX_BONES][16] );
qboolean R_CalcAllBoneMatricesMDM( const refEntity_t *refent, float outMat4[MDX_MAX_BONES][16] );

#endif // TR_BONES_H_
