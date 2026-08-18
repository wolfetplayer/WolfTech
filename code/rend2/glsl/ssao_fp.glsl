uniform sampler2D u_ScreenDepthMap;

uniform vec4   u_ViewInfo; // zfar / znear, zfar, 1/width, 1/height
uniform float  u_SSAOIntensity;
uniform float  u_SSAORadius;

varying vec2   var_ScreenTex;

#if 0
vec2 poissonDisc[9] = vec2[9](
vec2(-0.7055767, 0.196515),    vec2(0.3524343, -0.7791386),
vec2(0.2391056, 0.9189604),    vec2(-0.07580382, -0.09224417),
vec2(0.5784913, -0.002528916), vec2(0.192888, 0.4064181),
vec2(-0.6335801, -0.5247476),  vec2(-0.5579782, 0.7491854),
vec2(0.7320465, 0.6317794)
);
#endif

// use the full 9-point poisson disc below (was only sampling the first 3)
#define NUM_SAMPLES 9

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

mat2 randomRotation( const vec2 p )
{
	float r = random(p);
	float sinr = sin(r);
	float cosr = cos(r);
	return mat2(cosr, sinr, -sinr, cosr);
}

float getLinearDepth(sampler2D depthMap, const vec2 tex, const float zFarDivZNear)
{
	float sampleZDivW = texture2D(depthMap, tex).r;
	return 1.0 / mix(zFarDivZNear, 1.0, sampleZDivW);
}

float ambientOcclusion(sampler2D depthMap, const vec2 tex, const float zFarDivZNear, const float zFar, const vec2 scale, const float radius)
{
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

	float result = 0.0;

	float sampleZ = getLinearDepth(depthMap, tex, zFarDivZNear);
	float scaleZ = zFarDivZNear * sampleZ;

	vec2 slope = vec2(dFdx(sampleZ), dFdy(sampleZ)) / vec2(dFdx(tex.x), dFdy(tex.y));

	if (length(slope) * zFar > 5000.0)
		return 1.0;

	// cap the radius so a near-zero scaleZ (e.g. crunched view-weapon depth) can't fling samples off-screen
	vec2 offsetScale = min(vec2(scale * radius / scaleZ), vec2(0.1));

	mat2 rmat = randomRotation(tex);

	float invZFar = 1.0 / zFar;
	float zLimit = 20.0 * invZFar;
	int i;
	for (i = 0; i < NUM_SAMPLES; i++)
	{
		vec2 offset = rmat * poissonDisc[i] * offsetScale;
		float sampleDiff = getLinearDepth(depthMap, tex + offset, zFarDivZNear) - sampleZ;

		bool s1 = abs(sampleDiff) > zLimit;
		bool s2 = sampleDiff + invZFar > dot(slope, offset);
		result += float(s1 || s2);
	}

	result *= 1.0 / float(NUM_SAMPLES);

	return result;
}

void main()
{
	float result = ambientOcclusion(u_ScreenDepthMap, var_ScreenTex, u_ViewInfo.x, u_ViewInfo.y, u_ViewInfo.wz, u_SSAORadius);

	// u_SSAOIntensity of 0 fades back to 1.0 (fully unoccluded/no effect)
	result = mix(1.0, result, u_SSAOIntensity);

	gl_FragColor = vec4(vec3(result), 1.0);
}
