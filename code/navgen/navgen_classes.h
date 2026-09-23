// navgen_classes.h -- per-agent-size bake parameters (see ai_cast_characters.c).

#ifndef __NAVGEN_CLASSES_H
#define __NAVGEN_CLASSES_H

typedef struct {
	const char *name;
	float radius;      // maxs[0], also maxs[1] (square footprint)
	float height;      // maxs[2] - mins[2]
	float climb;        // STEPSIZE; no per-character override exists today
} navGenClass_t;

#define NAVGEN_NUM_CLASSES 2

extern const navGenClass_t navGenClasses[NAVGEN_NUM_CLASSES];

#endif // __NAVGEN_CLASSES_H
