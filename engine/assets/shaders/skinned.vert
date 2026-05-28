#version 410 core

layout (location = 0) in vec3 vertPos;
layout (location = 1) in vec3 vertNor;
layout (location = 2) in vec2 vertTexCoord;
layout (location = 3) in uvec4 boneIds;
layout (location = 4) in vec4 boneWeights;

out vec3 fragPos;
out vec3 fragNor;
out vec2 fragTexCoord;

layout (std140) uniform CameraData
{
	mat4 view;
	mat4 projection;
	vec4 cameraPos;
};

uniform mat4 model;
uniform int isSkinned;
uniform int numBones;
uniform mat4 bones[100];

mat4 getSkinMatrix()
{
	if (isSkinned == 0)
	{
		return mat4(1.0);
	}

	mat4 skin = mat4(0.0);
	for (int i = 0; i < 4; ++i)
	{
		uint boneId = boneIds[i];
		if (boneId < uint(numBones))
		{
			skin += bones[boneId] * boneWeights[i];
		}
	}
	return skin;
}

void main()
{
	mat4 skin = getSkinMatrix();
	vec4 skinnedPos = skin * vec4(vertPos, 1.0);
	vec3 skinnedNor = mat3(skin) * vertNor;

	gl_Position = projection * view * model * skinnedPos;
	fragPos = vec3(model * skinnedPos);
	fragNor = mat3(transpose(inverse(model))) * skinnedNor;
	fragTexCoord = vertTexCoord;
}
