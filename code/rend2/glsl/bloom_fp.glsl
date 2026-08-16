uniform sampler2D u_TextureMap;
uniform vec4      u_Color;
varying vec2      var_TexCoords;

const vec3 LUMINANCE_VECTOR = vec3(0.2125, 0.7154, 0.0721);

void main()
{
	vec4 color = texture2D(u_TextureMap, var_TexCoords);
	float luma = dot(color.rgb, LUMINANCE_VECTOR);

	// u_Color.r doubles as the bright-pass threshold
	float contribution = max(luma - u_Color.r, 0.0) / max(luma, 0.0001);

	gl_FragColor = vec4(color.rgb * contribution, 1.0);
}
