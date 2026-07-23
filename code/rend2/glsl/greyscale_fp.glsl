uniform sampler2D u_TextureMap;
uniform float     u_Greyscale;
varying vec2      var_TexCoords;

// From Rec. 709-1 4.2, Derivation of luminance signal
const vec3 LUMA = vec3(0.2125, 0.7154, 0.0721);

// Sam Hocevar's cheap RGB<->HSV conversion
vec3 rgb2hsv(vec3 c)
{
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));

    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

vec3 hsv2rgb(vec3 c)
{
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

void main() {
    vec4 color = texture2D(u_TextureMap, var_TexCoords);

    if (u_Greyscale >= 1.5) {
        // Selective-color grade: keep reds (flags, blood, ...), grey out everything else
        vec3 hsv = rgb2hsv(color.rgb);
        float hueDist = min(hsv.x, 1.0 - hsv.x); // hue wraps at 1.0, red sits at 0.0

        float hueMask = 1.0 - smoothstep(0.05, 0.09, hueDist);
        float satMask = smoothstep(0.15, 0.35, hsv.y);
        float redMask = hueMask * satMask;

        float y = dot(color.rgb, LUMA);
        vec3 greyed = vec3(y);
        vec3 punchedRed = hsv2rgb(vec3(hsv.x, clamp(hsv.y * 1.4, 0.0, 1.0), hsv.z));

        color.rgb = mix(greyed, punchedRed, redMask);
    } else if (u_Greyscale > 0.0) {
        float y = dot(color.rgb, LUMA);
        color.rgb = mix(color.rgb, vec3(y), clamp(u_Greyscale, 0.0, 1.0));
    }

    gl_FragColor = color;
}
