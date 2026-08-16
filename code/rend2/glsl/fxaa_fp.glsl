uniform sampler2D u_TextureMap;
uniform vec2      u_InvTexRes;

varying vec2      var_Tex1;


#define FXAA_SPAN_MAX   8.0
#define FXAA_REDUCE_MUL (1.0 / 8.0)
#define FXAA_REDUCE_MIN (1.0 / 128.0)

void main()
{
	vec3 rgbNW = texture2D(u_TextureMap, var_Tex1 + (vec2(-1.0, -1.0) * u_InvTexRes)).rgb;
	vec3 rgbNE = texture2D(u_TextureMap, var_Tex1 + (vec2( 1.0, -1.0) * u_InvTexRes)).rgb;
	vec3 rgbSW = texture2D(u_TextureMap, var_Tex1 + (vec2(-1.0,  1.0) * u_InvTexRes)).rgb;
	vec3 rgbSE = texture2D(u_TextureMap, var_Tex1 + (vec2( 1.0,  1.0) * u_InvTexRes)).rgb;
	vec4 rgbaM = texture2D(u_TextureMap, var_Tex1);
	vec3 rgbM  = rgbaM.rgb;

	vec3 luma = vec3(0.299, 0.587, 0.114);
	float lumaNW = dot(rgbNW, luma);
	float lumaNE = dot(rgbNE, luma);
	float lumaSW = dot(rgbSW, luma);
	float lumaSE = dot(rgbSE, luma);
	float lumaM  = dot(rgbM,  luma);

	float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
	float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

	vec2 dir;
	dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
	dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));

	float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * (FXAA_REDUCE_MUL * 0.25), FXAA_REDUCE_MIN);
	float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);

	dir = clamp(dir * rcpDirMin, vec2(-FXAA_SPAN_MAX), vec2(FXAA_SPAN_MAX)) * u_InvTexRes;

	vec3 rgbA = 0.5 * (
		texture2D(u_TextureMap, var_Tex1 + dir * (1.0 / 3.0 - 0.5)).rgb +
		texture2D(u_TextureMap, var_Tex1 + dir * (2.0 / 3.0 - 0.5)).rgb);

	vec3 rgbB = rgbA * 0.5 + 0.25 * (
		texture2D(u_TextureMap, var_Tex1 + dir * (0.0 / 3.0 - 0.5)).rgb +
		texture2D(u_TextureMap, var_Tex1 + dir * (3.0 / 3.0 - 0.5)).rgb);

	float lumaB = dot(rgbB, luma);

	vec3 rgbResult = (lumaB < lumaMin || lumaB > lumaMax) ? rgbA : rgbB;

	gl_FragColor = vec4(rgbResult, rgbaM.a);
}
