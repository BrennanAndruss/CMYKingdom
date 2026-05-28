#version 410 core

layout (location = 0) in vec3 vertPos;
layout (location = 1) in vec3 vertNor;
layout (location = 2) in vec2 vertTexCoord;

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

void main()
{
	gl_Position = projection * view * model * vec4(vertPos, 1.0);
	fragPos = vec3(model * vec4(vertPos, 1.0));
	fragNor = mat3(transpose(inverse(model))) * vertNor;
	fragTexCoord = vertTexCoord;
}