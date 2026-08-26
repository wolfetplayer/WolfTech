/*
===========================================================================
UTF-8 decoding helpers, adapted from ET:Legacy's q_unicode.c. Scoped to what's
needed to decode UTF-8 and look glyphs up in a fontInfoExtra_t.
===========================================================================
*/

#ifndef __Q_UNICODE_H
#define __Q_UNICODE_H

#include "q_shared.h"

// the cross sign drawn in place of a codepoint the font has no glyph for
#define INVALID_CHAR_OFFSET 215

// number of bytes the UTF-8 sequence starting at str occupies (1-4)
int Q_UTF8_Width( const char *str );

// number of UTF-8 bytes needed to encode a given codepoint
int Q_UTF8_WidthCP( int ch );

// validates a single UTF-8 sequence starting at str
qboolean Q_UTF8_ValidateSingle( const char *str );

// number of codepoints (not bytes) in a UTF-8 string
size_t Q_UTF8_Strlen( const char *str );

// decodes the UTF-8 sequence at str into a single Unicode codepoint
uint32_t Q_UTF8_CodePoint( const char *str );

// true if c is a UTF-8 continuation byte (10xxxxxx), not the start of a sequence
qboolean Q_UTF8_ContByte( char c );

// encodes a codepoint as UTF-8 into out (>= 4 bytes); returns bytes written (1-4)
int Q_UTF8_Encode( uint32_t codepoint, char *out );

// glyph lookup for an extended font; falls back to INVALID_CHAR_OFFSET if out of range
glyphInfo_t *Q_UTF8_GetGlyphExtended( fontInfoExtra_t *font, uint32_t codepoint );

#endif // __Q_UNICODE_H
