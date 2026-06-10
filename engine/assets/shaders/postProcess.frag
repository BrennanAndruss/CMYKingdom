#version 410 core

in vec2 fragTexCoord;

out vec4 color;

uniform sampler2D hdrSceneTex;

uniform float exposure;
uniform float contrast;
uniform float saturation;
uniform vec3 lift;
uniform vec3 gamma;
uniform vec3 gain;

vec3 tonemapReinhard(vec3 color)
{
	return color / (vec3(1.0) + color);
}

// Narkowicz ACES approximation
vec3 tonemapACES(vec3 x)
{
	const float a = 2.51;
	const float b = 0.03;
	const float c = 2.43;
	const float d = 0.59;
	const float e = 0.14;

	return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

vec3 applyTonemap(vec3 color)
{
	// return tonemapReinhard(color);
	return tonemapACES(color);
}

vec3 applyColorGrading(vec3 color)
{
	// Apply exposure in stops
	color *= pow(2.0, exposure);

	// Apply lift/gamma/gain
	color = color * gain + lift;
	color = pow(max(color, 0.0), 1.0 / gamma);

	// Multiply contrast value centered around 0.5
	color = max((color - 0.5) * contrast + 0.5, 0.0);

	// Convert to luminance and apply saturation
	float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
	color = mix(vec3(luminance), color, saturation);

	return color;
}

void main()
{
	vec3 hdr = texture(hdrSceneTex, fragTexCoord).rgb;

	// Perform color grading in linear space
	vec3 graded = applyColorGrading(hdr);

	// Tonemap HDR to SDR
	vec3 tonemapped = applyTonemap(graded);

	// Gamma corretion: Convert linear to sRGB
	vec3 result = pow(tonemapped, vec3(1.0 / 2.2));

	color = vec4(result, 1.0);
}