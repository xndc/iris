#version 300 es
precision highp float;

in vec2 VTexcoord0;

uniform sampler2D TexAlbedo;
uniform vec4 ConstAlbedo;
uniform float StippleHardCutoff;
uniform float StippleSoftCutoff;

float Dither2x2(vec2 position, float brightness) {
	int x = int(mod(position.x, 2.0));
	int y = int(mod(position.y, 2.0));
	int index = x + y * 2;
	float limit = 0.0;
	if (x < 8) {
		if (index == 0) limit = 0.25;
		if (index == 1) limit = 0.75;
		if (index == 2) limit = 1.00;
		if (index == 3) limit = 0.50;
	}
	return brightness < limit ? 0.0 : 1.0;
}

void main() {
	float alpha = ConstAlbedo.a * texture(TexAlbedo, VTexcoord0).a;
	if (alpha < StippleHardCutoff) {
		discard;
	}
	if (alpha < StippleSoftCutoff) {
		alpha = (alpha - StippleHardCutoff) / (StippleSoftCutoff - StippleHardCutoff);
		if (Dither2x2(gl_FragCoord.xy, alpha) < 0.5) {
			discard;
		}
	}
}
