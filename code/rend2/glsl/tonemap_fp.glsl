uniform sampler2D u_TextureMap;
uniform sampler2D u_LevelsMap;

uniform vec4      u_Color;


uniform vec2      u_AutoExposureMinMax;
uniform vec3      u_ToneMinAvgMaxLinear;
uniform float     u_InvGamma;

varying vec2      var_TexCoords;
varying float     var_InvWhite;

const vec3  LUMINANCE_VECTOR =   vec3(0.2125, 0.7154, 0.0721); //vec3(0.299, 0.587, 0.114);

uniform sampler2D u_ColorGradeLUT;

// u_ColorGradeLUT is a 16^3 color cube flattened into a 256x16 2D strip
// (16 tiles of 16x16, blue selects the tile); see main/gfx/luts/neutral.png.
// Always bound to either the active grade or the built-in no-op identity LUT.
vec3 ApplyColorGradeLUT(vec3 color)
{
	const float lutSize = 16.0;

	vec3 scaled = clamp(color, 0.0, 1.0) * (lutSize - 1.0);

	float slice     = floor(scaled.b);
	float sliceFrac = scaled.b - slice;
	float sliceNext = min(slice + 1.0, lutSize - 1.0);

	float u = (scaled.r + 0.5) / (lutSize * lutSize);
	float v = (scaled.g + 0.5) / lutSize;

	vec3 sampleLow  = texture2D(u_ColorGradeLUT, vec2(u + slice     / lutSize, v)).rgb;
	vec3 sampleHigh = texture2D(u_ColorGradeLUT, vec2(u + sliceNext / lutSize, v)).rgb;

	return mix(sampleLow, sampleHigh, sliceFrac);
}

float FilmicTonemap(float x)
{
	const float SS  = 0.22; // Shoulder Strength
	const float LS  = 0.30; // Linear Strength
	const float LA  = 0.10; // Linear Angle
	const float TS  = 0.20; // Toe Strength
	const float TAN = 0.01; // Toe Angle Numerator
	const float TAD = 0.30; // Toe Angle Denominator

	return ((x*(SS*x+LA*LS)+TS*TAN)/(x*(SS*x+LS)+TS*TAD)) - TAN/TAD;
}

void main()
{
	vec4 color = texture2D(u_TextureMap, var_TexCoords) * u_Color;

#if defined(USE_PBR)
	color.rgb *= color.rgb;
#endif

	vec3 minAvgMax = texture2D(u_LevelsMap, var_TexCoords).rgb;
	vec3 logMinAvgMaxLum = clamp(minAvgMax * 20.0 - 10.0, -u_AutoExposureMinMax.y, -u_AutoExposureMinMax.x);

	float invAvgLum = u_ToneMinAvgMaxLinear.y * exp2(-logMinAvgMaxLum.y);

	color.rgb = color.rgb * invAvgLum - u_ToneMinAvgMaxLinear.xxx;
	color.rgb = max(vec3(0.0), color.rgb);

	color.r = FilmicTonemap(color.r);
	color.g = FilmicTonemap(color.g);
	color.b = FilmicTonemap(color.b);

	color.rgb = clamp(color.rgb * var_InvWhite, 0.0, 1.0);

#if defined(USE_PBR)
	color.rgb = sqrt(color.rgb);
#endif

	// software gamma correction, replaces the hw gamma ramp SDL3 no longer supports
	color.rgb = pow(color.rgb, vec3(u_InvGamma));

	color.rgb = ApplyColorGradeLUT(color.rgb);

	// add a bit of dither to reduce banding
	color.rgb += vec3(1.0/510.0 * mod(gl_FragCoord.x + gl_FragCoord.y, 2.0) - 1.0/1020.0);

	gl_FragColor = color;
}
