// tr_bones.h -- bone-hierarchy evaluation, tag lookup, and vertex skinning for MOD_MDS/MOD_MDM/MOD_MDX skeletal models.

#ifndef TR_BONES_H_
#define TR_BONES_H_

int R_LerpTag( orientation_t *tag, const refEntity_t *refent, const char *tagNameIn, int startIndex );

// Blends a surface's vertices by their bone weights into outPos (model-local, caller-sized to surf->numVerts). MDM falls back to bind pose if no skeleton is linked yet.
void R_SkinMDSSurface( const mdsHeader_t *mds, const mdsSurface_t *surf, const refEntity_t *refent, vec3_t *outPos );
void R_SkinMDMSurface( const mdmSurface_t *surf, const refEntity_t *refent, vec3_t *outPos );

#endif // TR_BONES_H_
