// tr_marks.h -- decal projection onto world geometry (bullet holes, scorch marks, blood).

#ifndef TR_MARKS_H_
#define TR_MARKS_H_

int R_MarkFragments( int orientation, const vec3_t *points, const vec3_t projection,
					  int maxPoints, float *pointBuffer, int maxFragments, markFragment_t *fragmentBuffer );

#endif // TR_MARKS_H_
