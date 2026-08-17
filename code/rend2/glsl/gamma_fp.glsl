uniform sampler2D u_DiffuseMap;
uniform vec4      u_Color;
uniform float     u_InvGamma;
uniform float     u_Time;

varying vec2      var_Tex1;

uniform sampler2D u_ColorGradeLUT;
uniform float     u_VignetteIntensity;
uniform float     u_FilmGrainIntensity;
uniform float     u_ChromaticAberration;

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

// samples R/G/B at slightly different UVs, radiating from screen center,
// to fake the color fringing of an imperfect lens. u_ChromaticAberration
// of 0 collapses all three taps onto the same texel (no-op).
vec4 SampleChromaticAberration(sampler2D tex, vec2 uv, float amount)
{
	vec2 offset = (uv - vec2(0.5)) * amount;
	vec4 center = texture2D(tex, uv);

	float r = texture2D(tex, uv - offset).r;
	float b = texture2D(tex, uv + offset).b;

	return vec4(r, center.g, b, center.a);
}

// darkens the screen edges/corners
vec3 ApplyVignette(vec3 color, vec2 uv, float intensity)
{
	float dist = length(uv - vec2(0.5)) * 1.4142135; // corners reach 1.0
	float vig = 1.0 - intensity * smoothstep(0.3, 1.0, dist);
	return color * vig;
}

float FilmGrainRandom(vec2 p)
{
	return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

// per-pixel noise that re-randomizes every frame via u_Time
vec3 ApplyFilmGrain(vec3 color, vec2 fragCoord, float time, float intensity)
{
	float noise = FilmGrainRandom(fragCoord + fract(time) * 1000.0) - 0.5;
	return color + noise * intensity;
}

void main()
{
	vec4 color = SampleChromaticAberration(u_DiffuseMap, var_Tex1, u_ChromaticAberration) * u_Color;

	color.rgb = pow(max(color.rgb, vec3(0.0)), vec3(u_InvGamma));
	color.rgb = ApplyColorGradeLUT(color.rgb);
	color.rgb = ApplyVignette(color.rgb, var_Tex1, u_VignetteIntensity);
	color.rgb = ApplyFilmGrain(color.rgb, gl_FragCoord.xy, u_Time, u_FilmGrainIntensity);

	gl_FragColor = color;
}
