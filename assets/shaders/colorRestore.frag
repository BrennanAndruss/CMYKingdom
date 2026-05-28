#version 410 core

in vec2 fragTexCoord;

out vec4 color;

uniform sampler2D sceneTex;
uniform float cyan;
uniform float magenta;
uniform float yellow;
uniform float key;

void main()
{
	vec3 scene = texture(sceneTex, fragTexCoord).rgb;

	// Convert scene to grayscale based on luminance
	float gray = dot(scene, vec3(0.299, 0.587, 0.114));
	vec3 grayscale = vec3(gray);

	// Convert scene RGB to CMY
	float c = 1.0 - scene.r;
	float m = 1.0 - scene.g;
	float y = 1.0 - scene.b;

	// Subtractive CMYK color restoration
	float activeC = c * cyan;
	float activeM = m * magenta;
	float activeY = y * yellow;
	vec3 restored = vec3(1.0 - activeC, 1.0 - activeM, 1.0 - activeY);

	// Blend between gray and restored based on the average restored channel strength
	float restoredAmount = (cyan + magenta + yellow) / 3.0;
	restoredAmount *= (1.0 - key);

	color = vec4(mix(grayscale, restored, restoredAmount), 1.0);
}