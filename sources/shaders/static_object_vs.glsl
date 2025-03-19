#version 330 core

struct VsOutput
{
  vec3 EyespaceNormal;
  vec3 WorldPosition;
};

uniform mat4 Transform;
uniform mat4 ViewProjection;

layout(location = 0) in vec3 Position;
layout(location = 1) in vec3 Normal;

out VsOutput vsOutput;

void main()
{
  vec3 VertexPosition = (Transform * vec4(Position, 1)).xyz;
  vsOutput.EyespaceNormal = mat3(Transform) * Normal;

  gl_Position = ViewProjection * vec4(VertexPosition, 1);
  vsOutput.WorldPosition = VertexPosition;
}