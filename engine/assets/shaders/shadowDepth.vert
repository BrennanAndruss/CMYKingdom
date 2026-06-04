#version 410 core

layout (location = 0) in vec3 vertPos;

uniform mat4 model;
uniform mat4 lightSpace;

void main()
{
	// Transform into light space
	gl_Position = lightSpace * model * vec4(vertPos, 1.0);
}