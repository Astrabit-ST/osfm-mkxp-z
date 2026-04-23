uniform sampler2D texture;
uniform sampler2D palette;

uniform float paletteWidth;
uniform float paletteRow;

varying vec2 v_texCoord;

void main()
{
	vec4 frag = texture2D(texture, v_texCoord);

	float iMax = max(paletteWidth - 1., 1.);
	for (float i = 0.; i < paletteWidth; ++i) {
		float x = i / iMax;
		if (texture2D(palette, vec2(x, 0.)) == frag) {
			gl_FragColor = texture2D(palette, vec2(x, paletteRow));
			return;
		}
	}

	gl_FragColor = frag;
}
