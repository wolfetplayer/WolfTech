// nav_public.h -- plain-C entry points for the Recast/Detour nav module.

#ifndef __NAV_PUBLIC_H
#define __NAV_PUBLIC_H

#ifdef __cplusplus
extern "C" {
#endif

void Nav_Init( void );
void Nav_Shutdown( void );

// Phase 0 smoke test only; remove once Phase 2 adds the real query surface.
int Nav_Test( void );

#ifdef __cplusplus
}
#endif

#endif // __NAV_PUBLIC_H
