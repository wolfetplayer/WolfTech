uniform sampler2D u_TextureMap;

uniform vec2      u_InvTexRes;
varying vec2      var_TexCoords;

const vec3 LUMINANCE_VECTOR = vec3(0.2125, 0.7154, 0.0721);

// Karis-weighted average (1/(1+luma) per sample) so a lone bright pixel doesn't flicker across frames like a plain box average would
void main()
{
	vec4 s0 = texture2D(u_TextureMap, var_TexCoords + u_InvTexRes * vec2(-0.5, -0.5));
	vec4 s1 = texture2D(u_TextureMap, var_TexCoords + u_InvTexRes * vec2( 0.5, -0.5));
	vec4 s2 = texture2D(u_TextureMap, var_TexCoords + u_InvTexRes * vec2(-0.5,  0.5));
	vec4 s3 = texture2D(u_TextureMap, var_TexCoords + u_InvTexRes * vec2( 0.5,  0.5));

	float w0 = 1.0 / (1.0 + dot(s0.rgb, LUMINANCE_VECTOR));
	float w1 = 1.0 / (1.0 + dot(s1.rgb, LUMINANCE_VECTOR));
	float w2 = 1.0 / (1.0 + dot(s2.rgb, LUMINANCE_VECTOR));
	float w3 = 1.0 / (1.0 + dot(s3.rgb, LUMINANCE_VECTOR));

	gl_FragColor = (s0 * w0 + s1 * w1 + s2 * w2 + s3 * w3) / (w0 + w1 + w2 + w3);
}
