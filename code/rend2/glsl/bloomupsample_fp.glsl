uniform sampler2D u_TextureMap;

uniform vec2      u_InvTexRes;
varying vec2      var_TexCoords;

// standard 3x3 tent filter; caller additively blends the result onto the next larger mip
void main()
{
	vec4 color;

	color  = texture2D(u_TextureMap, var_TexCoords + u_InvTexRes * vec2(-1.0, -1.0));
	color += texture2D(u_TextureMap, var_TexCoords + u_InvTexRes * vec2( 0.0, -1.0)) * 2.0;
	color += texture2D(u_TextureMap, var_TexCoords + u_InvTexRes * vec2( 1.0, -1.0));

	color += texture2D(u_TextureMap, var_TexCoords + u_InvTexRes * vec2(-1.0,  0.0)) * 2.0;
	color += texture2D(u_TextureMap, var_TexCoords + u_InvTexRes * vec2( 0.0,  0.0)) * 4.0;
	color += texture2D(u_TextureMap, var_TexCoords + u_InvTexRes * vec2( 1.0,  0.0)) * 2.0;

	color += texture2D(u_TextureMap, var_TexCoords + u_InvTexRes * vec2(-1.0,  1.0));
	color += texture2D(u_TextureMap, var_TexCoords + u_InvTexRes * vec2( 0.0,  1.0)) * 2.0;
	color += texture2D(u_TextureMap, var_TexCoords + u_InvTexRes * vec2( 1.0,  1.0));

	gl_FragColor = color * 0.0625;
}
