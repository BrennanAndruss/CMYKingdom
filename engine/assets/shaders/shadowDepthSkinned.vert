#version 410 core

#define MAX_SHADER_BONES 100

layout (location = 0) in vec3 vertPos;
layout (location = 3) in uvec4 boneIds;
layout (location = 4) in vec4 boneWeights;

uniform mat4 model;
uniform mat4 lightSpace;
uniform mat4 bones[MAX_SHADER_BONES];

void main()
{
	mat4 skin = boneWeights.x * bones[boneIds.x]
			  + boneWeights.y * bones[boneIds.y]
			  + boneWeights.z * bones[boneIds.z]
			  + boneWeights.w * bones[boneIds.w];
	
	// Transform to light space
	gl_Position = lightSpace * model * skin * vec4(vertPos, 1.0);
}