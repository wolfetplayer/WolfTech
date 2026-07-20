uniform sampler2D u_DiffuseMap;
uniform vec4      u_Color;
uniform float     u_InvGamma;

varying vec2      var_Tex1;


void main()
{
	vec4 color = texture2D(u_DiffuseMap, var_Tex1) * u_Color;

	color.rgb = pow(max(color.rgb, vec3(0.0)), vec3(u_InvGamma));

	gl_FragColor = color;
}
