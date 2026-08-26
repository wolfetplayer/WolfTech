/*
===========================================================================
UTF-8 decoding helpers, adapted from ET:Legacy's q_unicode.c. Scoped to what's
needed to decode UTF-8 and look glyphs up in a fontInfoExtra_t.
===========================================================================
*/

#include "q_unicode.h"

/*
================
Q_UTF8_Width

Number of bytes the UTF-8 sequence starting at str occupies (1-4).
================
*/
int Q_UTF8_Width( const char *str ) {
	int ewidth;
	const unsigned char *s = (const unsigned char *)str;

	if ( !str ) {
		return 0;
	}

	if ( *s <= 0x7F ) {
		ewidth = 0;
	} else if ( 0xC2 <= *s && *s <= 0xDF ) {
		ewidth = 1;
	} else if ( 0xE0 <= *s && *s <= 0xEF ) {
		ewidth = 2;
	} else if ( 0xF0 <= *s && *s <= 0xF4 ) {
		ewidth = 3;
	} else {
		ewidth = 0;
	}

	for ( ; *s && ewidth > 0; s++, ewidth-- )
		;

	return s - (const unsigned char *)str + 1;
}

/*
================
Q_UTF8_WidthCP

Number of UTF-8 bytes needed to encode a given codepoint.
================
*/
int Q_UTF8_WidthCP( int ch ) {
	if ( ch <= 0x007F ) {
		return 1;
	}
	if ( ch <= 0x07FF ) {
		return 2;
	}
	if ( ch <= 0xFFFF ) {
		return 3;
	}
	if ( ch <= 0x10FFFF ) {
		return 4;
	}
	return 0;
}

/*
================
Q_UTF8_ValidateSingle

Validates the single UTF-8 sequence starting at str.
================
*/
qboolean Q_UTF8_ValidateSingle( const char *str ) {
	int i = 0, utfBytes = 0;
	size_t len = strlen( str );
	byte current = str[0];

	if ( current <= 0x7F ) {
		utfBytes = 0; // 0XXXXXXX
	} else if ( ( current & 0xE0 ) == 0xC0 ) {
		utfBytes = 1; // 110XXXXX
	} else if ( current == 0xED && ( len - 1 ) > 0 && ( (byte)str[1] & 0xA0 ) == 0xA0 ) {
		return qfalse; // U+D800 to U+DFFF
	} else if ( ( current & 0xF0 ) == 0xE0 ) {
		utfBytes = 2; // 1110XXXX
	} else if ( ( current & 0xF8 ) == 0xF0 ) {
		utfBytes = 3; // 11110XXX
	} else {
		return qfalse;
	}

	if ( utfBytes > len ) {
		return qfalse;
	}

	for ( ; 0 < utfBytes && i < len; utfBytes-- ) {
		if ( ( ++i == len ) || ( ( (byte)str[i] & 0xC0 ) != 0x80 ) ) {
			return qfalse;
		}
	}

	return qtrue;
}

/*
================
Q_UTF8_Strlen

Number of codepoints (not bytes) in a UTF-8 string.
================
*/
size_t Q_UTF8_Strlen( const char *str ) {
	size_t l = 0;
	while ( *str ) {
		l++;
		str += Q_UTF8_Width( str );
	}
	return l;
}

/*
================
Q_UTF8_ContByte

True if c is a UTF-8 continuation byte (10xxxxxx).
================
*/
qboolean Q_UTF8_ContByte( char c ) {
	return ( (unsigned char)c & 0xC0 ) == 0x80;
}

static qboolean getbit( const unsigned char *p, int pos ) {
	p += pos / 8;
	pos %= 8;

	return ( p && ( *p & ( 1 << ( 7 - pos ) ) ) ) != 0;
}

static void setbit( unsigned char *p, int pos, qboolean on ) {
	p += pos / 8;
	pos %= 8;

	if ( on ) {
		*p |= 1 << ( 7 - pos );
	} else {
		*p &= ~( 1 << ( 7 - pos ) );
	}
}

static void shiftbitsright( unsigned char *p, unsigned long num, unsigned long by ) {
	int step, off;
	unsigned char *e;

	if ( by >= num ) {
		for ( ; num > 8; p++, num -= 8 ) {
			*p = 0;
		}
		*p &= ( (unsigned long) ~0x00 ) >> num;
		return;
	}

	step = by / 8;
	off = by % 8;

	for ( e = p + ( num + 7 ) / 8 - 1; e > p + step; e-- ) {
		*e = ( *( e - step ) >> off ) | ( *( e - step - 1 ) << ( 8 - off ) );
	}

	*e = *( e - step ) >> off;

	for ( e = p; e < p + step; e++ ) {
		*e = 0;
	}
}

/*
================
Q_UTF8_CodePoint

Decodes the UTF-8 sequence at str into a single Unicode codepoint.
================
*/
uint32_t Q_UTF8_CodePoint( const char *str ) {
	int i, j;
	int n = 0;
	int size = Q_UTF8_Width( str );
	uint32_t codepoint = 0;
	unsigned char *p = (unsigned char *) &codepoint;

	if ( !str || !str[0] ) {
		return 0;
	}

	// invalid/extended-ASCII byte: pass it through as-is
	if ( !Q_UTF8_ValidateSingle( str ) ) {
		return (unsigned char)str[0];
	}

	if ( size > sizeof( codepoint ) ) {
		size = sizeof( codepoint );
	} else if ( size < 1 ) {
		size = 1;
	}

	for ( i = ( size > 1 ? size : 0 ); i < 8; i++ ) {
		setbit( p, n++, getbit( (const unsigned char *)str, i ) );
	}

	for ( i = 1; i < size; i++ ) {
		for ( j = 2; j < 8; j++ ) {
			setbit( p, n++, getbit( ( (const unsigned char *)str ) + i, j ) );
		}
	}

	shiftbitsright( p, 8 * sizeof( codepoint ), 8 * sizeof( codepoint ) - n );

#ifdef Q3_LITTLE_ENDIAN
	for ( i = 0; i < sizeof( codepoint ) / 2; i++ ) {
		p[i] ^= p[sizeof( codepoint ) - 1 - i];
		p[sizeof( codepoint ) - 1 - i] ^= p[i];
		p[i] ^= p[sizeof( codepoint ) - 1 - i];
	}
#endif

	return codepoint;
}

/*
================
Q_UTF8_Encode

Encodes a codepoint as UTF-8 into out (>= 4 bytes). Returns bytes written.
================
*/
int Q_UTF8_Encode( uint32_t codepoint, char *out ) {
	if ( codepoint <= 0x7F ) {
		out[0] = (char)codepoint;
		return 1;
	} else if ( codepoint <= 0x7FF ) {
		out[0] = (char)( 0xC0 | ( codepoint >> 6 ) );
		out[1] = (char)( 0x80 | ( codepoint & 0x3F ) );
		return 2;
	} else if ( codepoint <= 0xFFFF ) {
		out[0] = (char)( 0xE0 | ( codepoint >> 12 ) );
		out[1] = (char)( 0x80 | ( ( codepoint >> 6 ) & 0x3F ) );
		out[2] = (char)( 0x80 | ( codepoint & 0x3F ) );
		return 3;
	} else {
		out[0] = (char)( 0xF0 | ( codepoint >> 18 ) );
		out[1] = (char)( 0x80 | ( ( codepoint >> 12 ) & 0x3F ) );
		out[2] = (char)( 0x80 | ( ( codepoint >> 6 ) & 0x3F ) );
		out[3] = (char)( 0x80 | ( codepoint & 0x3F ) );
		return 4;
	}
}

/*
================
Q_UTF8_GetGlyphExtended

Glyph lookup for an extended font; out-of-range codepoints fall back to INVALID_CHAR_OFFSET.
================
*/
glyphInfo_t *Q_UTF8_GetGlyphExtended( fontInfoExtra_t *font, uint32_t codepoint ) {
	if ( codepoint > GLYPH_UTF_END ) {
		codepoint = INVALID_CHAR_OFFSET;
	}

	if ( codepoint <= GLYPH_END ) {
		return &font->glyphs[codepoint];
	}

	return &font->glyphsUTF8[codepoint];
}
