#version 410 core

out vec4 FragColor;

// Screen-space water shading inputs.
in vec2 TexCoords;
in vec4 ClipSpaceCoords;
in vec3 ViewSpacePos;
in float WaveHeight;
in vec3 WorldPos;

uniform sampler2D u_sceneColorTex;
uniform sampler2D u_noiseTex;
uniform sampler2D u_terrainHeightTex;

uniform float u_time;
uniform float u_terrainPlaneLen;
uniform float u_terrainHeightScale;
uniform vec3 u_shallowColor;
uniform vec3 u_deepColor;
uniform float u_refractionStrength;
uniform float u_depthScale;

void main()
{
    //  Project the water fragment to screen space so we can sample the scene color behind it.
    vec2 ndc = (ClipSpaceCoords.xy / ClipSpaceCoords.w) * 0.5 + 0.5;

    //  Sample animated noise, but keep the distortion very small so it feels like refraction,
    //    not a moving checkerboard.
    vec2 noiseA = texture(u_noiseTex, TexCoords * 1.8 + vec2(u_time * 0.025, u_time * 0.018)).rg;
    vec2 noiseB = texture(u_noiseTex, TexCoords * 3.6 - vec2(u_time * 0.014, u_time * 0.021)).rg;
    vec2 combinedDistort = (noiseA * 0.65 + noiseB * 0.35) - 0.5;
    combinedDistort = combinedDistort * 2.0;
    vec2 distortedNDC = ndc + combinedDistort * (u_refractionStrength * 0.35);

    // Sample the terrain heightmap directly so shallow/deep color and foam respond to
    //    actual ground contact instead of foreground silhouettes.
    vec2 terrainUV = (WorldPos.xz / u_terrainPlaneLen) + vec2(0.5);
    float terrainHeight = texture(u_terrainHeightTex, terrainUV).r * u_terrainHeightScale;

    //  Water depth above the terrain at this world position.
    float depthToTerrain = max(WorldPos.y - terrainHeight, 0.0);

    //  Smooth depth gradient: shallow near land edges, deep farther away.
    float depthFactor = clamp(depthToTerrain * u_depthScale, 0.0, 0.67);
    depthFactor = smoothstep(0.0, 1.0, depthFactor);
    depthFactor = depthFactor * depthFactor * (3.0 - 2.0 * depthFactor);
    vec3 waterColor = mix(u_shallowColor, u_deepColor, depthFactor);

    // Use the captured scene color as the refracted background and blend it
    //    lightly with the stylized water gradient.
    vec3 refractionColor = texture(u_sceneColorTex, distortedNDC).rgb;
    vec3 baseWaterColor = mix(refractionColor, waterColor, 0.72);

    //  Shoreline foam: only appears where the water is shallow above terrain.
    //    Two scrolling noise layers keep the foam from looking like a flat block.
    vec2 foamUVA = TexCoords * 7.0 + vec2(u_time * 0.055, -u_time * 0.03);
    vec2 foamUVB = TexCoords * 14.0 - vec2(u_time * 0.025, u_time * 0.045);
    float foamNoiseA = texture(u_noiseTex, foamUVA).r;
    float foamNoiseB = texture(u_noiseTex, foamUVB).r;
    float foamNoise = foamNoiseA * 0.62 + foamNoiseB * 0.38;

    // Foam generation zone: near the terrain contact line.
    float shorelineBand = 1.0 - smoothstep(0.02, 0.16, depthToTerrain);

    // Erosion: thin parts of the foam die first, leaving organic clumps.
    float foamMask = shorelineBand * smoothstep(0.34, 0.78, foamNoise);
    foamMask *= smoothstep(0.18, 0.86, foamNoise + shorelineBand * 0.25);
    foamMask = smoothstep(0.12, 0.78, foamMask);

    //  Fresnel makes glancing angles a touch brighter so the surface reads as water.
    vec3 viewDir = normalize(-ViewSpacePos);
    float fresnel = pow(1.0 - max(dot(normalize(vec3(0.0, 1.0, 0.0)), viewDir), 0.0), 3.0);
    vec3 fresnelTint = vec3(0.65, 0.82, 0.95) * fresnel * 0.25;

    vec3 finalColor = baseWaterColor + fresnelTint;
    finalColor = mix(finalColor, vec3(1.0), foamMask * 0.9);

    FragColor = vec4(finalColor, 1.0);
}
