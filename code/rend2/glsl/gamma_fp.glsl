uniform sampler2D u_DiffuseMap;
uniform vec4      u_Color;
uniform float     u_InvGamma;

varying vec2      var_Tex1;

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

void main()
{
	vec4 color = texture2D(u_DiffuseMap, var_Tex1) * u_Color;

	color.rgb = pow(max(color.rgb, vec3(0.0)), vec3(u_InvGamma));
	color.rgb = ApplyColorGradeLUT(color.rgb);

	gl_FragColor = color;
}
