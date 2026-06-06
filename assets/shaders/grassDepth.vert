#version 410 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

layout (location = 2) in vec4 iPosScale;
layout (location = 3) in vec4 iRotBendSeed;

uniform mat4 lightSpace;
uniform float uTime;
uniform vec2 uWindDirection;
uniform float uWindStrength;
uniform float uWindSpeed;

out vec2 TexCoord;

void main()
{
    vec3 pos = aPos;

    float bladeHeight = iPosScale.w;
    float rotation = iRotBendSeed.x;
    float bendAmount = iRotBendSeed.y;
    float widthScale = iRotBendSeed.z;
    float seed = iRotBendSeed.w;

    float t = aTexCoord.y;

    pos.x *= widthScale;
    pos.y *= bladeHeight;

    // Permanent blade curve
    pos.z += bendAmount * t * t;

    vec2 windDir = normalize(uWindDirection);

    float travelingWave =
        sin(
            uTime * uWindSpeed +
            dot(iPosScale.xz, windDir) * 0.15 +
            seed
        );

    float gustWave =
        sin(
            uTime * (uWindSpeed * 0.37) +
            dot(iPosScale.xz, windDir) * 0.04 +
            seed * 0.5
        );

    float windAmount =
        ((travelingWave * 0.7) + (gustWave * 0.3))
        * uWindStrength;

    float bendFactor = t * t;

    pos.xz += windDir * windAmount * bendFactor;

    float c = cos(rotation);
    float s = sin(rotation);
    mat2 rot = mat2(c, -s, s, c);
    pos.xz = rot * pos.xz;

    vec3 worldPos = pos + iPosScale.xyz;
    gl_Position = lightSpace * vec4(worldPos, 1.0);

    TexCoord = aTexCoord;
}