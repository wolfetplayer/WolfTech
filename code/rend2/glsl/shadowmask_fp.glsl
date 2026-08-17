uniform sampler2D u_ScreenDepthMap;

uniform sampler2DShadow u_ShadowMap;
#if defined(USE_SHADOW_CASCADE)
uniform sampler2DShadow u_ShadowMap2;
uniform sampler2DShadow u_ShadowMap3;
uniform sampler2DShadow u_ShadowMap4;
#endif

#if defined(USE_PCSS)
uniform sampler2D u_ShadowMapRaw;
#if defined(USE_SHADOW_CASCADE)
uniform sampler2D u_ShadowMapRaw2;
uniform sampler2D u_ShadowMapRaw3;
uniform sampler2D u_ShadowMapRaw4;
#endif
uniform float     u_PCSSLightSize;
uniform float     u_PCSSDebug;
#endif

uniform mat4      u_ShadowMvp;
#if defined(USE_SHADOW_CASCADE)
uniform mat4      u_ShadowMvp2;
uniform mat4      u_ShadowMvp3;
uniform mat4      u_ShadowMvp4;
#endif

uniform vec3   u_ViewOrigin;
uniform vec4   u_ViewInfo; // zfar / znear, zfar

varying vec2   var_DepthTex;
varying vec3   var_ViewDir;

// depth is GL_DEPTH_COMPONENT24
// so the maximum error is 1.0 / 2^24
#define DEPTH_MAX_ERROR 0.000000059604644775390625

// Input: It uses texture coords as the random number seed.
// Output: Random number: [0,1), that is between 0.0 and 0.999999... inclusive.
// Author: Michael Pohoreski
// Copyright: Copyleft 2012 :-)
// Source: http://stackoverflow.com/questions/5149544/can-i-generate-a-random-number-inside-a-pixel-shader

float random( const vec2 p )
{
  // We need irrationals for pseudo randomness.
  // Most (all?) known transcendental numbers will (generally) work.
  const vec2 r = vec2(
    23.1406926327792690,  // e^pi (Gelfond's constant)
     2.6651441426902251); // 2^sqrt(2) (Gelfond-Schneider constant)
  //return fract( cos( mod( 123456789., 1e-7 + 256. * dot(p,r) ) ) );
  return mod( 123456789., 1e-7 + 256. * dot(p,r) );
}

#if defined(USE_PCSS)
// average depth of texels closer to the light than the receiver, within a
// small search window -- an estimate of how far away the occluder sits.
// returns -1.0 if nothing in the window blocks the light (fully lit, no penumbra)
float PCSSBlockerDistance(sampler2D rawShadowMap, vec2 st, float receiverDepth, float searchRadius)
{
	// same disc used below by PCF, so blocker search and filtering agree on what's nearby
	vec2 poissonDisc[9];

	poissonDisc[0] = vec2(-0.7055767, 0.196515);
	poissonDisc[1] = vec2(0.3524343, -0.7791386);
	poissonDisc[2] = vec2(0.2391056, 0.9189604);
	poissonDisc[3] = vec2(-0.07580382, -0.09224417);
	poissonDisc[4] = vec2(0.5784913, -0.002528916);
	poissonDisc[5] = vec2(0.192888, 0.4064181);
	poissonDisc[6] = vec2(-0.6335801, -0.5247476);
	poissonDisc[7] = vec2(-0.5579782, 0.7491854);
	poissonDisc[8] = vec2(0.7320465, 0.6317794);

	float blockerSum = 0.0;
	float numBlockers = 0.0;
	int i;

	for (i = 0; i < 9; i++)
	{
		float sampleDepth = texture2D(rawShadowMap, st + poissonDisc[i] * searchRadius).r;
		if (sampleDepth < receiverDepth - DEPTH_MAX_ERROR)
		{
			blockerSum += sampleDepth;
			numBlockers += 1.0;
		}
	}

	if (numBlockers < 1.0)
		return -1.0;

	return blockerSum / numBlockers;
}

// contact shadows stay crisp; shadows with a distant/large blocker gap widen out
float PCSSRadiusScale(sampler2D rawShadowMap, vec2 st, float receiverDepth)
{
	float searchRadius = u_PCSSLightSize * (4.0 / r_shadowMapSize);
	float blockerDepth = PCSSBlockerDistance(rawShadowMap, st, receiverDepth, searchRadius);

	if (blockerDepth < 0.0)
		return 1.0;

	// clamped so a rare huge blocker gap can't blow the filter radius out
	// to the point it starts sampling unrelated parts of the shadow map
	return clamp(1.0 + (receiverDepth - blockerDepth) * u_PCSSLightSize * 400.0, 1.0, 8.0);
}
#endif

float PCF(const sampler2DShadow shadowmap, const vec2 st, const float dist, const float radiusScale)
{
	float mult;
	float scale = (2.0 / r_shadowMapSize) * radiusScale;

#if 0
	// from http://http.developer.nvidia.com/GPUGems/gpugems_ch11.html
	vec2 offset = vec2(greaterThan(fract(var_DepthTex.xy * r_FBufScale * 0.5), vec2(0.25)));
	offset.y += offset.x;
	if (offset.y > 1.1) offset.y = 0.0;

	mult = shadow2D(shadowmap, vec3(st + (offset + vec2(-1.5,  0.5)) * scale, dist))
	     + shadow2D(shadowmap, vec3(st + (offset + vec2( 0.5,  0.5)) * scale, dist))
	     + shadow2D(shadowmap, vec3(st + (offset + vec2(-1.5, -1.5)) * scale, dist))
	     + shadow2D(shadowmap, vec3(st + (offset + vec2( 0.5, -1.5)) * scale, dist));

	mult *= 0.25;
#endif

#if defined(USE_SHADOW_FILTER)
	float r = random(var_DepthTex.xy);
	float sinr = sin(r) * scale;
	float cosr = cos(r) * scale;
	mat2 rmat = mat2(cosr, sinr, -sinr, cosr);

	mult =  shadow2D(shadowmap, vec3(st + rmat * vec2(-0.7055767, 0.196515), dist));
	mult += shadow2D(shadowmap, vec3(st + rmat * vec2(0.3524343, -0.7791386), dist));
	mult += shadow2D(shadowmap, vec3(st + rmat * vec2(0.2391056, 0.9189604), dist));
  #if defined(USE_SHADOW_FILTER2)
	mult += shadow2D(shadowmap, vec3(st + rmat * vec2(-0.07580382, -0.09224417), dist));
	mult += shadow2D(shadowmap, vec3(st + rmat * vec2(0.5784913, -0.002528916), dist));
	mult += shadow2D(shadowmap, vec3(st + rmat * vec2(0.192888, 0.4064181), dist));
	mult += shadow2D(shadowmap, vec3(st + rmat * vec2(-0.6335801, -0.5247476), dist));
	mult += shadow2D(shadowmap, vec3(st + rmat * vec2(-0.5579782, 0.7491854), dist));
	mult += shadow2D(shadowmap, vec3(st + rmat * vec2(0.7320465, 0.6317794), dist));

	mult *= 0.11111;
  #else
    mult *= 0.33333;
  #endif
#else
	mult = shadow2D(shadowmap, vec3(st, dist));
#endif

	return mult;
}

float getLinearDepth(sampler2D depthMap, vec2 tex, float zFarDivZNear)
{
	float sampleZDivW = texture2D(depthMap, tex).r - DEPTH_MAX_ERROR;
	return 1.0 / mix(zFarDivZNear, 1.0, sampleZDivW);
}

void main()
{
	float result;
	float radiusScale = 1.0;

	float depth = getLinearDepth(u_ScreenDepthMap, var_DepthTex, u_ViewInfo.x);
	vec4 biasPos = vec4(u_ViewOrigin + var_ViewDir * (depth - 0.5 / u_ViewInfo.x), 1.0);

	vec4 shadowpos = u_ShadowMvp * biasPos;

	if ( depth >= 0.999 )
	{
		result = 1.0;
	}
	else
#if defined(USE_SHADOW_CASCADE)
	if (all(lessThan(abs(shadowpos.xyz), vec3(abs(shadowpos.w)))))
#endif
	{
		shadowpos.xyz = shadowpos.xyz * (0.5 / shadowpos.w) + vec3(0.5);
#if defined(USE_PCSS)
		radiusScale = PCSSRadiusScale(u_ShadowMapRaw, shadowpos.xy, shadowpos.z);
#endif
		result = PCF(u_ShadowMap, shadowpos.xy, shadowpos.z, radiusScale);
	}
#if defined(USE_SHADOW_CASCADE)
	else
	{
		shadowpos = u_ShadowMvp2 * biasPos;

		if (all(lessThan(abs(shadowpos.xyz), vec3(abs(shadowpos.w)))))
		{
			shadowpos.xyz = shadowpos.xyz * (0.5 / shadowpos.w) + vec3(0.5);
#if defined(USE_PCSS)
			radiusScale = PCSSRadiusScale(u_ShadowMapRaw2, shadowpos.xy, shadowpos.z);
#endif
			result = PCF(u_ShadowMap2, shadowpos.xy, shadowpos.z, radiusScale);
		}
		else
		{
			shadowpos = u_ShadowMvp3 * biasPos;

			if (all(lessThan(abs(shadowpos.xyz), vec3(abs(shadowpos.w)))))
			{
				shadowpos.xyz = shadowpos.xyz * (0.5 / shadowpos.w) + vec3(0.5);
#if defined(USE_PCSS)
				radiusScale = PCSSRadiusScale(u_ShadowMapRaw3, shadowpos.xy, shadowpos.z);
#endif
				result = PCF(u_ShadowMap3, shadowpos.xy, shadowpos.z, radiusScale);
			}
			else
			{
				shadowpos = u_ShadowMvp4 * biasPos;
				shadowpos.xyz = shadowpos.xyz * (0.5 / shadowpos.w) + vec3(0.5);
#if defined(USE_PCSS)
				radiusScale = PCSSRadiusScale(u_ShadowMapRaw4, shadowpos.xy, shadowpos.z);
#endif
				result = PCF(u_ShadowMap4, shadowpos.xy, shadowpos.z, radiusScale);
			}
		}
	}
#endif

#if defined(USE_PCSS)
	// visualizes the penumbra estimate instead of the shadow: black = no
	// softening (radiusScale 1x), white = maxed out against the clamp (8x)
	if (u_PCSSDebug > 0.5)
	{
		gl_FragColor = vec4(vec3((radiusScale - 1.0) / 7.0), 1.0);
		return;
	}
#endif

	gl_FragColor = vec4(vec3(result), 1.0);
}
