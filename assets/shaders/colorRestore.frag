#version 410 core

in vec2 fragTexCoord;

out vec4 color;

uniform sampler2D sceneTex;
uniform sampler2D depthTex;

uniform mat4 invPV;

uniform bool pulseActive;
uniform vec3 pulseCenter;
uniform float pulseRadius;
uniform float pulseThickness;
uniform float pulseSoftness;
uniform float pulseBoost;
uniform int activePulseType; // 0 = Cyan, 1 = Magenta, 2 = Yellow

uniform float cyan;
uniform float magenta;
uniform float yellow;
uniform float key;

// Simple high-frequency hash
float hash3(vec3 p) {
    return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453123);
}

// Sphere SDF
float pulseSignedDist(vec3 pos)
{
	vec4 sphere = vec4(pulseCenter, pulseRadius);
	return distance(pos, sphere.xyz) - sphere.w;
}

void main()
{
	vec3 scene = texture(sceneTex, fragTexCoord).rgb;
	float depth = texture(depthTex, fragTexCoord).r;

	// Reconstruct world position from screen coordinates and depth
	vec4 clipSpacePos = vec4(fragTexCoord * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
	vec4 worldSpacePos = invPV * clipSpacePos;
	vec3 worldPos = worldSpacePos.xyz / worldSpacePos.w;

	// Current persistent state for mixing
	float currentCyan = cyan;
	float currentMagenta = magenta;
	float currentYellow = yellow;

	float rimMask = 0.0;

	// Process dynamic color surge if a pulse is active
	if (pulseActive)
	{
		// Compute signed distance of world space pos from pulse
		// Negative values indicate the position is inside the pulse
		float pulseSD = pulseSignedDist(worldPos);

		// Apply distortion to the sphere shell
		pulseSD += hash3(floor(worldPos * 0.4)) * 1.8;

		if (pulseSD < 0.0)
		{
			// Restore color within the pulse
			if (activePulseType == 0) 
				currentCyan = min(cyan + pulseBoost, 1.0);
			else if (activePulseType == 1) 
				currentMagenta = min(magenta + pulseBoost, 1.0);
			else if (activePulseType == 2) 
				currentYellow = min(yellow + pulseBoost, 1.0);
		}
		else if (pulseSD < pulseThickness)
		{
			// Set rim mask for highlights at the edge of the pulse shockwave
			rimMask = 1.0 - smoothstep(pulseThickness * 0.5 - pulseSoftness, pulseThickness * 0.5, abs(pulseSD));
		}
	}

	// Convert scene to grayscale based on luminance
	float gray = dot(scene, vec3(0.299, 0.587, 0.114));
	vec3 grayscale = vec3(gray);

	// Convert scene RGB to CMY
	float c = 1.0 - scene.r;
	float m = 1.0 - scene.g;
	float y = 1.0 - scene.b;

	// Apply localized channel factors for subtractive CMYK color restoration
	float activeC = c * currentCyan;
	float activeM = m * currentMagenta;
	float activeY = y * currentYellow;
	vec3 restored = vec3(1.0 - activeC, 1.0 - activeM, 1.0 - activeY);

	// Determine overall saturation mix factor
	float maxChannelValue = max(currentCyan, max(currentMagenta, currentYellow));
	vec3 finalComposite = mix(grayscale, restored, maxChannelValue);

	// Apply pure color rim highlight to pulse border
	if (rimMask > 0.0)
	{
		vec3 pureRimColor = vec3(0.0);
		if (activePulseType == 0) 
			pureRimColor = vec3(0.0, 1.0, 1.0);
		else if (activePulseType == 1) 
			pureRimColor = vec3(1.0, 0.0, 1.0);
		else if (activePulseType == 2)
			pureRimColor = vec3(1.0, 1.0, 0.0);

		// Blend the glowing border smoothly into the scene color
		finalComposite = mix(finalComposite, pureRimColor, rimMask * 0.85);
	}

	color = vec4(finalComposite, 1.0);
}