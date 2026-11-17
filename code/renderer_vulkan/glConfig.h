// glConfig.h -- window-mode/glconfig_t bookkeeping, adapted from vkQuake3 (GPLv2-or-later).

#ifndef GLCONFIG_H_
#define GLCONFIG_H_

void R_InitDisplayResolution( void );
void R_SetWinMode( int mode, unsigned int w, unsigned int h, unsigned int hz );
void R_GetWinResolution( int *w, int *h );

void R_glConfigInit( void );
void R_glConfigClear( void );
void R_GetGlConfig( glconfig_t *const pOut );

#endif // GLCONFIG_H_
