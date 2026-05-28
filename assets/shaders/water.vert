#version 410 core

layout (location = 0) in vec3 vertPos;
layout (location = 1) in vec3 vertNor;
layout (location = 2) in vec2 vertTexCoord;

// Vertex-to-fragment data for the water surface.
out vec2 TexCoords;
out vec4 ClipSpaceCoords;
out vec3 ViewSpacePos;
out float WaveHeight;
out vec3 WorldPos;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_projection;
uniform float u_time;
uniform float u_waveSpeed;
uniform float u_waveLength;
uniform float u_waveHeight;

void main()
{
    // World-space position of the water volume/cube.
    vec3 worldPos = vec3(u_model * vec4(vertPos, 1.0));
    WorldPos = worldPos;

    // Stylized surface motion: broad sine waves, kept subtle to avoid noisy shimmer.
    float waveX = sin(worldPos.x * u_waveLength + u_time * u_waveSpeed);
    float waveZ = cos(worldPos.z * u_waveLength + u_time * u_waveSpeed * 0.8);
    WaveHeight = (waveX + waveZ) * u_waveHeight;
    worldPos.y += WaveHeight;

    // View-space position is used by the fragment shader for depth comparisons.
    vec4 viewPos = u_view * vec4(worldPos, 1.0);
    ViewSpacePos = viewPos.xyz;

    // Clip-space coordinates are used to sample the scene depth/color buffers.
    ClipSpaceCoords = u_projection * viewPos;
    TexCoords = vertTexCoord;

    gl_Position = ClipSpaceCoords;
}
